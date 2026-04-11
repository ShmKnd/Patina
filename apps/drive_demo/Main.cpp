/*
  Patina — Drive / Distortion Demo
  ============================================================================
  Chain:
    Input (sine ping / mic)
      → TL072InputBuffer   (1MΩ Hi-Z buffer + cable LPF + headroom)
      → DriveModule        (RC-HP → gain → Diode or Tanh saturation)
      → OutputModule       (3-pole LPF + supply-voltage-dependent saturation)
      → Dry/Wet mix

  GUI controls:
    PLAY / STOP  — toggle test signal (440 Hz ping, 20 ms / 0.8 s)
    Drive        — saturation amount (0–1)
    Mix          — Dry/Wet equal-power crossfade (0=dry, 1=wet)
    Supply       — supply voltage (9–18 V), affects headroom & tone
    Mode         — Bypass / Diode clip / Tanh soft-sat
*/

#include <JuceHeader.h>

#include "dsp/circuits/drive/InputBuffer.h"
#include "dsp/circuits/drive/DiodeClipper.h"
#include "dsp/circuits/drive/OutputStage.h"
#include "dsp/constants/PartsConstants.h"
#include "dsp/circuits/mixer/Mixer.h"

#include <atomic>
#include <array>

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

        // ---- Mode selector ----
        addAndMakeVisible(modeBox);
        modeBox.addItem("Bypass",    1);
        modeBox.addItem("Diode",     2);
        modeBox.addItem("Tanh",      3);
        modeBox.setSelectedId(2, juce::dontSendNotification);
        modeBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff313244));
        modeBox.setColour(juce::ComboBox::textColourId,       juce::Colour(0xffcdd6f4));
        modeBox.onChange = [this]{ driveMode.store(modeBox.getSelectedId() - 1); };

        addAndMakeVisible(modeLabel);
        modeLabel.setText("Mode", juce::dontSendNotification);
        modeLabel.setFont(juce::FontOptions(13.0f));
        modeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcdd6f4));
        modeLabel.setJustificationType(juce::Justification::right);

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
            s.setColour(juce::Slider::thumbColourId,      juce::Colour(0xffcba6f7));
            s.setColour(juce::Slider::trackColourId,      juce::Colour(0xff45475a));
            s.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff313244));

            addAndMakeVisible(l);
            l.setText(name, juce::dontSendNotification);
            l.setFont(juce::FontOptions(13.0f));
            l.setColour(juce::Label::textColourId, juce::Colour(0xffcdd6f4));
            l.setJustificationType(juce::Justification::right);
            l.attachToComponent(&s, true);
        };

        setup(driveSlider,  driveLabel,  "Drive",  0.0,  1.0,  0.5, "");
        setup(mixSlider,    mixLabel,    "Mix",    0.0,  1.0,  1.0, "");
        setup(supplySlider, supplyLabel, "Supply",
              PartsConstants::V_supplyMin,
              PartsConstants::V_supplyMax,
              PartsConstants::V_supplyMin, " V");

        driveSlider .onValueChange = [this]{ driveAmt.store((float)driveSlider.getValue()); };
        mixSlider   .onValueChange = [this]{ mixAmt  .store((float)mixSlider  .getValue()); };
        supplySlider.onValueChange = [this]{ supplyV .store((float)supplySlider.getValue()); };

        setSize(540, 310);
        setAudioChannels(0, 2);
    }

    ~MainComponent() override { shutdownAudio(); }

    // =========================================================================
    // Audio thread
    // =========================================================================

    void prepareToPlay(int /*samplesPerBlock*/, double sampleRate) override
    {
        sr         = sampleRate;
        pingSample = 0;

        inputBuf.prepare(2, sampleRate);
        drive   .prepare(2, sampleRate);
        outMod  .prepare(2, sampleRate);
        outMod  .setCutoffHz(12000.0);  // 出力 LPF: 12 kHz
    }

    void releaseResources() override {}

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        if (!isPlaying.load())
        {
            info.clearActiveBufferRegion();
            return;
        }

        const int   n           = info.numSamples;
        const float drv         = driveAmt.load();
        const float mix         = mixAmt  .load();
        const float sv          = supplyV .load();
        const int   mode        = driveMode.load();

        const int pingPeriod   = static_cast<int>(sr * 0.8);
        const int pingDuration = static_cast<int>(sr * 0.020);

        // equal-power Dry/Wet gains
        float gDry, gWet;
        Mixer::equalPowerGainsFast(static_cast<double>(mix), gDry, gWet);

        auto* L = info.buffer->getWritePointer(0, info.startSample);
        auto* R = info.buffer->getWritePointer(1, info.startSample);

        for (int i = 0; i < n; ++i)
        {
            // 1. Test signal: 440 Hz ping (20 ms burst / 0.8 s period)
            float dry = 0.0f;
            if (pingSample < pingDuration)
            {
                double env = 0.5 * (1.0 - std::cos(
                    juce::MathConstants<double>::twoPi * pingSample / pingDuration));
                dry = static_cast<float>(0.7 * env *
                    std::sin(juce::MathConstants<double>::twoPi * 440.0 * pingSample / sr));
            }
            if (++pingSample >= pingPeriod) pingSample = 0;

            // 2. Drive / saturation  (TL072InputBuffer skipped: its headroom
            //    saturation fights DriveModule and masks distortion in a demo)
            float wet = drive.process(0, dry, drv, mode);

            // 4. Output 3-pole LPF + supply-voltage saturation
            wet = outMod.process(0, wet, static_cast<double>(sv));

            // 5. Equal-power Dry/Wet mix
            float out = gDry * dry + gWet * wet;

            L[i] = R[i] = out;
        }
    }

    // =========================================================================
    // GUI thread
    // =========================================================================

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1e1e2e));

        auto titleArea = getLocalBounds().removeFromTop(48);
        g.setColour(juce::Colour(0xff181825));
        g.fillRect(titleArea);

        g.setColour(juce::Colour(0xffcdd6f4));
        g.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
        g.drawText("Patina  —  Drive Demo",
                   titleArea.withTrimmedLeft(16),
                   juce::Justification::centredLeft);

        g.setColour(juce::Colour(0xff45475a));
        g.fillRect(0, 48, getWidth(), 1);
    }

    void resized() override
    {
        const int labelW = 90;
        const int rowH   = 38;
        auto area = getLocalBounds().reduced(16).withTrimmedTop(48);

        area.removeFromTop(8);

        // PLAY button + Mode selector on same row
        auto topRow = area.removeFromTop(34);
        playButton.setBounds(topRow.removeFromLeft(110).withSizeKeepingCentre(100, 30));
        topRow.removeFromLeft(16);
        modeLabel.setBounds(topRow.removeFromLeft(50));
        modeBox  .setBounds(topRow.removeFromLeft(130).withSizeKeepingCentre(120, 28));

        area.removeFromTop(10);
        driveSlider .setBounds(area.removeFromTop(rowH).withTrimmedLeft(labelW));
        area.removeFromTop(4);
        mixSlider   .setBounds(area.removeFromTop(rowH).withTrimmedLeft(labelW));
        area.removeFromTop(4);
        supplySlider.setBounds(area.removeFromTop(rowH).withTrimmedLeft(labelW));
    }

private:
    // DSP modules (audio thread only after prepare)
    TL072InputBuffer inputBuf;
    DriveModule      drive;
    OutputModule     outMod;

    double sr         = 44100.0;
    int    pingSample = 0;

    // Thread-safe parameters
    std::atomic<bool>  isPlaying  { false };
    std::atomic<float> driveAmt   { 0.5f  };
    std::atomic<float> mixAmt     { 1.0f  };
    std::atomic<float> supplyV    { static_cast<float>(PartsConstants::V_supplyMin) };
    std::atomic<int>   driveMode  { 1 };   // 0=Bypass 1=Diode 2=Tanh

    juce::TextButton playButton;
    juce::ComboBox   modeBox;
    juce::Label      modeLabel;
    juce::Slider     driveSlider, mixSlider, supplySlider;
    juce::Label      driveLabel,  mixLabel,  supplyLabel;

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
class PatinaDriveApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName()    override { return "Patina Drive"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override { mainWindow.reset(); }

    void systemRequestedQuit() override { quit(); }

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(PatinaDriveApp)
