/*
  Patina — PatinaExamples: Tube Power Amp + LC Filter Demo
  ============================================================================
  Real-time audio demo showcasing the new L3 circuits:

    Signal chain:
      Test signal (440 Hz ping)
        → TubePreamp (12AX7 preamp stage)
        → PassiveLCFilter (tone stack — wah-style LC resonance)
        → PushPullPowerStage (class AB power amplifier)
        → Output

  GUI Controls:
    PLAY / STOP    — toggle test signal
    Amp Topology   — Marshall / Fender Twin / Vox AC30 / Deluxe / Hi-Fi KT88
    Drive          — preamp drive (0–1)
    Bias           — power amp bias: cold class B → hot class A (0–1)
    Sag            — power supply sag: regulated → vintage rectifier (0–1)
    NFB            — negative feedback amount (0–1)
    Tone Freq      — LC filter center frequency (100–4000 Hz)
    Tone Reso      — LC filter resonance (0–1)
    LC Drive       — inductor saturation (0–1)
    Filter Type    — LPF / HPF / BPF / Notch
    Output         — master volume (0–1)
*/

#include <JuceHeader.h>

#include "dsp/circuits/saturation/TubePreamp.h"
#include "dsp/circuits/filters/PassiveLCFilter.h"
#include "dsp/circuits/saturation/PushPullPowerStage.h"
#include "dsp/parts/InductorPrimitive.h"
#include "dsp/circuits/mixer/Mixer.h"

#include <atomic>
#include <memory>

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

        // ---- Amp Topology selector ----
        addAndMakeVisible(topoBox);
        topoBox.addItem("Marshall 50W (EL34)",  1);
        topoBox.addItem("Fender Twin (6L6GC)",  2);
        topoBox.addItem("Vox AC30 (EL84)",      3);
        topoBox.addItem("Fender Deluxe (6V6)",  4);
        topoBox.addItem("Hi-Fi KT88",           5);
        topoBox.setSelectedId(1, juce::dontSendNotification);
        topoBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff313244));
        topoBox.setColour(juce::ComboBox::textColourId,       juce::Colour(0xffcdd6f4));
        topoBox.onChange = [this]{ ampTopo.store(topoBox.getSelectedId()); };

        addAndMakeVisible(topoLabel);
        topoLabel.setText("Amp", juce::dontSendNotification);
        topoLabel.setFont(juce::FontOptions(13.0f));
        topoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcdd6f4));
        topoLabel.setJustificationType(juce::Justification::right);

        // ---- Filter Type selector ----
        addAndMakeVisible(filterTypeBox);
        filterTypeBox.addItem("LPF",   1);
        filterTypeBox.addItem("HPF",   2);
        filterTypeBox.addItem("BPF",   3);
        filterTypeBox.addItem("Notch", 4);
        filterTypeBox.setSelectedId(3, juce::dontSendNotification);  // BPF default (wah-like)
        filterTypeBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff313244));
        filterTypeBox.setColour(juce::ComboBox::textColourId,       juce::Colour(0xffcdd6f4));
        filterTypeBox.onChange = [this]{ filterType.store(filterTypeBox.getSelectedId() - 1); };

        addAndMakeVisible(filterTypeLabel);
        filterTypeLabel.setText("Filter", juce::dontSendNotification);
        filterTypeLabel.setFont(juce::FontOptions(13.0f));
        filterTypeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcdd6f4));
        filterTypeLabel.setJustificationType(juce::Justification::right);

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
            s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 22);
            s.setTextValueSuffix(suffix);
            s.setColour(juce::Slider::thumbColourId,      juce::Colour(0xfff38ba8));
            s.setColour(juce::Slider::trackColourId,      juce::Colour(0xff45475a));
            s.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff313244));

            addAndMakeVisible(l);
            l.setText(name, juce::dontSendNotification);
            l.setFont(juce::FontOptions(13.0f));
            l.setColour(juce::Label::textColourId, juce::Colour(0xffcdd6f4));
            l.setJustificationType(juce::Justification::right);
            l.attachToComponent(&s, true);
        };

        // Power amp controls
        setup(driveSlider,    driveLabel,    "Drive",     0.0,    1.0,    0.6,  "");
        setup(biasSlider,     biasLabel,     "Bias",      0.0,    1.0,    0.65, "");
        setup(sagSlider,      sagLabel,      "Sag",       0.0,    1.0,    0.5,  "");
        setup(nfbSlider,      nfbLabel,      "NFB",       0.0,    1.0,    0.3,  "");

        // LC filter controls
        setup(toneFreqSlider, toneFreqLabel, "Tone Freq", 100.0,  4000.0, 1200.0, " Hz");
        setup(toneResoSlider, toneResoLabel, "Tone Reso", 0.0,    1.0,    0.5,    "");
        setup(lcDriveSlider,  lcDriveLabel,  "LC Drive",  0.0,    1.0,    0.2,    "");

        // Output
        setup(outputSlider,   outputLabel,   "Output",    0.0,    1.0,    0.7,    "");

        // Wire up atomics
        driveSlider   .onValueChange = [this]{ preampDrive.store((float)driveSlider.getValue()); };
        biasSlider    .onValueChange = [this]{ ampBias.store((float)biasSlider.getValue()); };
        sagSlider     .onValueChange = [this]{ ampSag.store((float)sagSlider.getValue()); };
        nfbSlider     .onValueChange = [this]{ ampNfb.store((float)nfbSlider.getValue()); };
        toneFreqSlider.onValueChange = [this]{ toneFreq.store((float)toneFreqSlider.getValue()); };
        toneResoSlider.onValueChange = [this]{ toneReso.store((float)toneResoSlider.getValue()); };
        lcDriveSlider .onValueChange = [this]{ lcDrive.store((float)lcDriveSlider.getValue()); };
        outputSlider  .onValueChange = [this]{ outputLevel.store((float)outputSlider.getValue()); };

        setSize(580, 520);
        setAudioChannels(0, 2);   // output-only
    }

    ~MainComponent() override { shutdownAudio(); }

    // =========================================================================
    // Audio thread
    // =========================================================================

    void prepareToPlay(int /*samplesPerBlock*/, double sampleRate) override
    {
        sr         = sampleRate;
        pingSample = 0;

        preamp.prepare(1, sampleRate);

        lcFilter = std::make_unique<PassiveLCFilter>(InductorPrimitive::WahInductor());
        lcFilter->prepare(1, sampleRate);

        rebuildAmp(1);  // Marshall default
    }

    void releaseResources() override {}

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        if (!isPlaying.load())
        {
            info.clearActiveBufferRegion();
            return;
        }

        // Check if topology changed
        int wantedTopo = ampTopo.load();
        if (wantedTopo != currentTopo)
        {
            rebuildAmp(wantedTopo);
            currentTopo = wantedTopo;
        }

        const int n = info.numSamples;

        // Read atomic parameters
        const float drv      = preampDrive.load();
        const float bias     = ampBias.load();
        const float sag      = ampSag.load();
        const float nfb      = ampNfb.load();
        const float tFreq    = toneFreq.load();
        const float tReso    = toneReso.load();
        const float lcDrv    = lcDrive.load();
        const int   fType    = filterType.load();
        const float outLvl   = outputLevel.load();

        // Build params structs
        TubePreamp::Params preP;
        preP.drive = drv;
        preP.bias  = 0.5f;
        preP.outputLevel = 0.8f;
        preP.enableGridConduction = true;

        PassiveLCFilter::Params lcP;
        lcP.cutoffHz   = tFreq;
        lcP.resonance  = tReso;
        lcP.drive      = lcDrv;
        lcP.filterType = fType;

        PushPullPowerStage::Params ampP;
        ampP.inputGainDb      = 6.0f + drv * 12.0f;  // 6–18 dB based on drive
        ampP.bias             = bias;
        ampP.negativeFeedback = nfb;
        ampP.sagAmount        = sag;
        ampP.outputLevel      = outLvl;
        ampP.presenceHz       = 5000.0f;

        // Ping parameters
        const int pingPeriod   = static_cast<int>(sr * 0.5);
        const int pingDuration = static_cast<int>(sr * 0.030);
        const double pingFreq  = 330.0;  // E4

        auto* L = info.buffer->getWritePointer(0, info.startSample);
        auto* R = info.buffer->getWritePointer(1, info.startSample);

        for (int i = 0; i < n; ++i)
        {
            // 1. Generate test signal (330 Hz ping)
            float dry = 0.0f;
            if (pingSample < pingDuration)
            {
                double env = 0.5 * (1.0 - std::cos(
                    juce::MathConstants<double>::twoPi * pingSample / pingDuration));
                dry = static_cast<float>(0.6 * env *
                    std::sin(juce::MathConstants<double>::twoPi * pingFreq * pingSample / sr));
            }
            if (++pingSample >= pingPeriod) pingSample = 0;

            // 2. Preamp stage (12AX7)
            float v = preamp.process(0, dry, preP);

            // 3. LC Filter tone stack (wah-style inductor)
            v = lcFilter->process(0, v, lcP);

            // 4. Power amplifier stage
            v = powerAmp->process(0, v, ampP);

            L[i] = R[i] = v;
        }
    }

    // =========================================================================
    // GUI thread
    // =========================================================================

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1e1e2e));

        // Title bar
        auto titleArea = getLocalBounds().removeFromTop(48);
        g.setColour(juce::Colour(0xff181825));
        g.fillRect(titleArea);

        g.setColour(juce::Colour(0xffcdd6f4));
        g.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
        g.drawText("Patina Examples  —  Tube Amp + LC Filter",
                   titleArea.withTrimmedLeft(16),
                   juce::Justification::centredLeft);

        // Separator
        g.setColour(juce::Colour(0xff45475a));
        g.fillRect(0, 48, getWidth(), 1);

        // Section labels
        auto drawSection = [&](int y, const juce::String& text, juce::Colour c)
        {
            g.setColour(c.withAlpha(0.15f));
            g.fillRect(0, y, getWidth(), 20);
            g.setColour(c);
            g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
            g.drawText(text, 16, y, 300, 20, juce::Justification::centredLeft);
        };

        drawSection(62, "PREAMP + POWER AMP", juce::Colour(0xfff38ba8));
        drawSection(262, "LC TONE STACK",      juce::Colour(0xff89b4fa));
        drawSection(412, "OUTPUT",             juce::Colour(0xffa6e3a1));
    }

    void resized() override
    {
        const int labelW = 90;
        const int rowH   = 36;
        const int gap    = 3;
        auto area = getLocalBounds().reduced(16).withTrimmedTop(48);

        area.removeFromTop(8);

        // Top row: PLAY + Topology
        auto topRow = area.removeFromTop(34);
        playButton.setBounds(topRow.removeFromLeft(100).withSizeKeepingCentre(90, 30));
        topRow.removeFromLeft(12);
        topoLabel.setBounds(topRow.removeFromLeft(40));
        topoBox  .setBounds(topRow.removeFromLeft(200).withSizeKeepingCentre(190, 28));

        // Preamp + Power Amp section
        area.removeFromTop(28);  // section label space
        driveSlider.setBounds(area.removeFromTop(rowH).withTrimmedLeft(labelW));
        area.removeFromTop(gap);
        biasSlider .setBounds(area.removeFromTop(rowH).withTrimmedLeft(labelW));
        area.removeFromTop(gap);
        sagSlider  .setBounds(area.removeFromTop(rowH).withTrimmedLeft(labelW));
        area.removeFromTop(gap);
        nfbSlider  .setBounds(area.removeFromTop(rowH).withTrimmedLeft(labelW));

        // LC Tone Stack section
        area.removeFromTop(28);  // section label space

        // Filter type row
        auto filterRow = area.removeFromTop(30);
        filterTypeLabel.setBounds(filterRow.removeFromLeft(labelW));
        filterTypeBox  .setBounds(filterRow.removeFromLeft(150).withSizeKeepingCentre(140, 26));
        area.removeFromTop(gap);

        toneFreqSlider.setBounds(area.removeFromTop(rowH).withTrimmedLeft(labelW));
        area.removeFromTop(gap);
        toneResoSlider.setBounds(area.removeFromTop(rowH).withTrimmedLeft(labelW));
        area.removeFromTop(gap);
        lcDriveSlider .setBounds(area.removeFromTop(rowH).withTrimmedLeft(labelW));

        // Output section
        area.removeFromTop(28);  // section label space
        outputSlider.setBounds(area.removeFromTop(rowH).withTrimmedLeft(labelW));
    }

private:
    void rebuildAmp(int topo)
    {
        switch (topo)
        {
            case 1:  powerAmp = std::make_unique<PushPullPowerStage>(PushPullPowerStage::Marshall50W());   break;
            case 2:  powerAmp = std::make_unique<PushPullPowerStage>(PushPullPowerStage::FenderTwin());    break;
            case 3:  powerAmp = std::make_unique<PushPullPowerStage>(PushPullPowerStage::VoxAC30());       break;
            case 4:  powerAmp = std::make_unique<PushPullPowerStage>(PushPullPowerStage::FenderDeluxe());  break;
            case 5:  powerAmp = std::make_unique<PushPullPowerStage>(PushPullPowerStage::HiFi_KT88());     break;
            default: powerAmp = std::make_unique<PushPullPowerStage>(PushPullPowerStage::Marshall50W());   break;
        }
        powerAmp->prepare(1, sr);
    }

    // DSP modules
    TubePreamp preamp;
    std::unique_ptr<PassiveLCFilter>      lcFilter;
    std::unique_ptr<PushPullPowerStage>   powerAmp;

    double sr         = 44100.0;
    int    pingSample = 0;
    int    currentTopo = 1;

    // Thread-safe parameters
    std::atomic<bool>  isPlaying    { false };
    std::atomic<int>   ampTopo      { 1 };
    std::atomic<float> preampDrive  { 0.6f };
    std::atomic<float> ampBias      { 0.65f };
    std::atomic<float> ampSag       { 0.5f };
    std::atomic<float> ampNfb       { 0.3f };
    std::atomic<float> toneFreq     { 1200.0f };
    std::atomic<float> toneReso     { 0.5f };
    std::atomic<float> lcDrive      { 0.2f };
    std::atomic<int>   filterType   { 2 };  // BPF
    std::atomic<float> outputLevel  { 0.7f };

    // GUI components
    juce::TextButton playButton;
    juce::ComboBox   topoBox, filterTypeBox;
    juce::Label      topoLabel, filterTypeLabel;
    juce::Slider     driveSlider, biasSlider, sagSlider, nfbSlider;
    juce::Slider     toneFreqSlider, toneResoSlider, lcDriveSlider;
    juce::Slider     outputSlider;
    juce::Label      driveLabel, biasLabel, sagLabel, nfbLabel;
    juce::Label      toneFreqLabel, toneResoLabel, lcDriveLabel;
    juce::Label      outputLabel;

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
class PatinaExamplesApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName()    override { return "Patina Examples"; }
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

START_JUCE_APPLICATION(PatinaExamplesApp)
