/*
  Patina - JUCE GUI Demo
  ============================================================================
  Real-time BBD delay demo with compander.
  GUI controls:
    PLAY / STOP  - toggle 440 Hz sine through the analog DSP chain
    Delay        - BBD delay time (10 - 800 ms)
    Compander    - Pre/Post-BBD compander amount (0 - 1)
    Supply       - BBD supply voltage (9 - 18 V), affects headroom & tone
*/

#include <JuceHeader.h>

#include "dsp/circuits/bbd/BbdStageEmulator.h"
#include "dsp/circuits/delay/DelayLine.h"
#include "dsp/circuits/compander/CompanderModule.h"
#include "dsp/constants/PartsConstants.h"

#include <atomic>
#include <vector>

//==============================================================================
class MainComponent : public juce::AudioAppComponent
{
public:
    MainComponent()
    {
        // ---- Play button ----
        addAndMakeVisible(playButton);
        playButton.setButtonText("PLAY");
        playButton.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff313244));
        playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffa6e3a1));
        playButton.onClick = [this]
        {
            bool next = !isPlaying.load();
            isPlaying.store(next);
            playButton.setButtonText(next ? "STOP" : "PLAY");
        };

        // ---- Sliders ----
        auto setup = [&](juce::Slider& s, juce::Label& l,
                         const juce::String& name,
                         double lo, double hi, double val,
                         const juce::String& suffix)
        {
            addAndMakeVisible(s);
            s.setRange(lo, hi);
            s.setValue(val, juce::dontSendNotification);
            s.setSliderStyle(juce::Slider::LinearHorizontal);
            s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22);
            s.setTextValueSuffix(suffix);
            s.setColour(juce::Slider::thumbColourId,        juce::Colour(0xff89b4fa));
            s.setColour(juce::Slider::trackColourId,        juce::Colour(0xff45475a));
            s.setColour(juce::Slider::backgroundColourId,   juce::Colour(0xff313244));

            addAndMakeVisible(l);
            l.setText(name, juce::dontSendNotification);
            l.setFont(juce::FontOptions(13.0f));
            l.setColour(juce::Label::textColourId, juce::Colour(0xffcdd6f4));
            l.setJustificationType(juce::Justification::right);
            l.attachToComponent(&s, true);
        };

        setup(delaySlider,  delayLabel,  "Delay",    10.0, 800.0, 300.0, " ms");
        setup(compSlider,   compLabel,   "Compander", 0.0,   1.0,   0.0, "");
        setup(supplySlider, supplyLabel, "Supply",
              PartsConstants::V_supplyMin,
              PartsConstants::V_supplyMax,
              PartsConstants::V_supplyMin, " V");

        delaySlider .onValueChange = [this]{ delayMs.store((float)delaySlider .getValue()); };
        compSlider  .onValueChange = [this]{ compAmt.store((float)compSlider  .getValue()); };
        supplySlider.onValueChange = [this]{ supplyV.store((float)supplySlider.getValue()); };

        setSize(520, 270);
        setAudioChannels(0, 2);   // output-only (no microphone permission needed)
    }

    ~MainComponent() override
    {
        shutdownAudio();
    }

    // =========================================================================
    // Audio thread
    // =========================================================================

    void prepareToPlay(int /*samplesPerBlock*/, double sampleRate) override
    {
        sr         = sampleRate;
        phase      = 0.0;
        pingSample = 0;

        // Ring buffer: 1ch, enough for 2 s of delay
        const int maxDelaySamples = static_cast<int>(sampleRate * 2.0) + 1;
        ringBuf.setSize(1, maxDelaySamples);
        ringBuf.clear();
        writePos = 0;

        bbd.prepare(1, sampleRate);   // mono BBD tone colouring
        comp.prepare(2, sampleRate);  // 2 virtual channels (pre/post)
    }

    void releaseResources() override {}

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        if (!isPlaying.load())
        {
            info.clearActiveBufferRegion();
            return;
        }

        const int    n           = info.numSamples;
        const float  ca          = compAmt.load();
        const double sv          = (double)supplyV.load();
        const double delaySamps  = (double)delayMs.load() * 0.001 * sr;
        const int    ringSize    = ringBuf.getNumSamples();

        // Ping parameters: 1 kHz tone, 20 ms burst, fires every 800 ms
        const int pingPeriod   = static_cast<int>(sr * 0.8);
        const int pingDuration = static_cast<int>(sr * 0.020);
        const double pingFreq  = 1000.0;

        auto* L = info.buffer->getWritePointer(0, info.startSample);
        auto* R = info.buffer->getWritePointer(1, info.startSample);

        for (int i = 0; i < n; ++i)
        {
            // 1. Generate periodic 1 kHz ping (20 ms on, 780 ms off)
            float dry = 0.0f;
            if (pingSample < pingDuration)
            {
                // Sine burst with a simple Hann-shaped amplitude envelope
                double env = 0.5 * (1.0 - std::cos(juce::MathConstants<double>::twoPi
                                                    * pingSample / pingDuration));
                dry = static_cast<float>(0.35 * env *
                    std::sin(juce::MathConstants<double>::twoPi * pingFreq * pingSample / sr));
            }
            if (++pingSample >= pingPeriod) pingSample = 0;

            // 2. Pre-BBD compression
            float x = comp.processCompress(0, dry, ca);

            // 3. Write dry signal into ring buffer
            ringBuf.setSample(0, writePos, x);

            // 4. Read delayed signal from ring buffer (BBD S&H quantization)
            float delayed = DelayLineView::readFromDelay(
                ringBuf, writePos, 0,
                delaySamps,
                PartsConstants::bbdStagesDefault,
                true);

            // 5. BBD tone colouring: LPF + saturation (1-element channel vector)
            oneSample[0] = delayed;
            bbd.process(oneSample, 0.0, PartsConstants::bbdStagesDefault, sv, false, 0.0);
            delayed = oneSample[0];

            // 6. Post-BBD expansion
            delayed = comp.processExpand(0, delayed, ca);

            // 7. Advance ring buffer write position
            if (++writePos >= ringSize) writePos = 0;

            // Mix: dry signal + delayed BBD echo (equal mix)
            L[i] = R[i] = 0.7f * dry + 0.7f * delayed;
        }
    }

    // =========================================================================
    // GUI thread
    // =========================================================================

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1e1e2e));

        // Title bar area
        auto titleArea = getLocalBounds().removeFromTop(48);
        g.setColour(juce::Colour(0xff181825));
        g.fillRect(titleArea);

        g.setColour(juce::Colour(0xffcdd6f4));
        g.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
        g.drawText("Patina  -  BBD Delay Demo",
                   titleArea.withTrimmedLeft(16),
                   juce::Justification::centredLeft);

        // Separator
        g.setColour(juce::Colour(0xff45475a));
        g.fillRect(0, 48, getWidth(), 1);
    }

    void resized() override
    {
        const int labelW = 90;
        const int rowH   = 40;
        auto area = getLocalBounds().reduced(16).withTrimmedTop(48);

        area.removeFromTop(8);
        playButton.setBounds(area.removeFromTop(34).withSizeKeepingCentre(120, 30));
        area.removeFromTop(12);

        delaySlider .setBounds(area.removeFromTop(rowH).withTrimmedLeft(labelW));
        area.removeFromTop(4);
        compSlider  .setBounds(area.removeFromTop(rowH).withTrimmedLeft(labelW));
        area.removeFromTop(4);
        supplySlider.setBounds(area.removeFromTop(rowH).withTrimmedLeft(labelW));
    }

private:
    // DSP modules (accessed only from audio thread after prepare)
    BbdStageEmulator bbd;
    CompanderModule  comp;

    // Ring buffer for delay line
    patina::compat::AudioBuffer<float> ringBuf;
    int writePos  = 0;
    std::vector<float> oneSample { 0.0f };  // 1-element scratch for BBD calls

    double sr         = 44100.0;
    double phase      = 0.0;   // unused now, kept for future use
    int    pingSample = 0;     // position within the ping period

    // Parameters - written from GUI thread, read from audio thread
    std::atomic<bool>  isPlaying { false };
    std::atomic<float> delayMs   { 300.0f };
    std::atomic<float> compAmt   {   0.5f };
    std::atomic<float> supplyV   {   9.0f };

    juce::TextButton playButton;
    juce::Slider     delaySlider, compSlider, supplySlider;
    juce::Label      delayLabel,  compLabel,  supplyLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

//==============================================================================
class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow(const juce::String& name)
        : DocumentWindow(name,
                         juce::Colour(0xff1e1e2e),
                         DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainComponent(), true);
        setResizable(false, false);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

//==============================================================================
class PatinaApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Patina Demo"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override { mainWindow.reset(); }

    void systemRequestedQuit() override { quit(); }

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(PatinaApp)
