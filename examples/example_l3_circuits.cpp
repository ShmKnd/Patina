/*
  example_l3_circuits.cpp — L3 Circuits sample
  ============================================================
  Demonstrates assembling L3 circuit modules from dsp/circuits/ into a
  signal-processing chain. This is a modular-style demo where circuit
  blocks are freely interconnected rather than using an integrated L4
  Engine.

  Sections (each 1 second):
     1. DiodeClipper → ToneFilter
         Clipping followed by tone control (L3 drive-family)
     2. StateVariableFilter — LP / HP / BP sweep
         Compare OTA-based SVF modes
     3. LadderFilter — 4-pole LP sweep with resonance
         Drive the transistor ladder near self-oscillation
     4. TubePreamp — varying drive amount
         12AX7 preamp distortion curve
     5. TapeSaturation — tape speed and saturation
         Tape compression + wow/flutter
     6. AnalogLfo → PhaserStage
         Phaser effect by LFO modulation
     7. CompanderModule → DelayLine → BbdStageEmulator → CompanderExpand
         Hand-build a BBD delay using only L3 modules (no BbdDelayEngine)

  Build:
     c++ -std=c++17 -O2 -I.. example_l3_circuits.cpp -o example_l3_circuits
*/

#include "include/patina.h"
#include "examples/wav_writer.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <random>

int main()
{
    constexpr int    kSampleRate   = 48000;
    constexpr int    kChannels     = 2;
    constexpr float  kSectionSec   = 1.0f;
    constexpr int    kSectionLen   = (int)(kSampleRate * kSectionSec);
    constexpr int    kNumSections  = 7;
    constexpr int    kTotalSamples = kSectionLen * kNumSections;

    patina::ProcessSpec spec{(double)kSampleRate, 256, kChannels};

    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    // Test signal
    auto testSig = wav::sineBurst(kSampleRate, 440.0f,
                                   (float)kNumSections, 0.7f, 30.0f, 200.0f);
    int offset = 0;

    // ================================================================
    // Section 1: DiodeClipper → ToneFilter
    // ================================================================
    {
        std::printf("Section 1: DiodeClipper → ToneFilter\n");
        DiodeClipper clipper;
        ToneFilter tone;
        clipper.prepare(spec);
        tone.prepare(spec);

        DiodeClipper::Params clipParams;
        clipParams.drive = 0.8f;
        clipParams.mode  = 1;   // Diode
        clipParams.diodeType = 0; // Si
        clipParams.temperature = 25.0f;

        const int half = kSectionLen / 2;
        for (int i = 0; i < kSectionLen; ++i)
        {
            float x = testSig[(size_t)(offset + i)];

            // L: Diode clipping + darker tone
            float clipped = clipper.process(0, x, clipParams);
            if (i < half)
                tone.setDefaultCutoff(1500.0f);
            else
                tone.setDefaultCutoff(6000.0f);  // second half: brighter tone
            output[0][(size_t)(offset + i)] = tone.processSample(0, clipped);

            // R: Tanh clipping
            DiodeClipper::Params tanhP = clipParams;
            tanhP.mode = 2; // Tanh
            float tClipped = clipper.process(1, x, tanhP);
            output[1][(size_t)(offset + i)] = tone.processSample(1, tClipped);
        }
    }
    offset += kSectionLen;

    // ================================================================
    // Section 2: StateVariableFilter — LP/HP/BP sweep
    // ================================================================
    {
        std::printf("Section 2: StateVariableFilter — LP/HP/BP sweep\n");
        StateVariableFilter svf;
        svf.prepare(spec);

        StateVariableFilter::Params svfParams;
        svfParams.resonance = 0.6f;
        svfParams.temperature = 25.0f;

        const int third = kSectionLen / 3;
        for (int i = 0; i < kSectionLen; ++i)
        {
            float x = testSig[(size_t)(offset + i)];

            // Slowly sweep the cutoff (200Hz → 4000Hz)
            float t = (float)i / (float)kSectionLen;
            svfParams.cutoffHz = 200.0f + t * 3800.0f;

            // L: Mode switch (LP → HP → BP)
            if (i < third)
                svfParams.type = 0;  // LP
            else if (i < third * 2)
                svfParams.type = 1;  // HP
            else
                svfParams.type = 2;  // BP

            output[0][(size_t)(offset + i)] = svf.process(0, x, svfParams.type);

            // R: retrieve all outputs simultaneously
            svf.setCutoffHz(svfParams.cutoffHz);
            svf.setResonance(svfParams.resonance);
            auto allOut = svf.processAll(1, x);
            // Mix LP and BP
            output[1][(size_t)(offset + i)] = allOut.lp * 0.6f + allOut.bp * 0.4f;
        }
    }
    offset += kSectionLen;

    // ================================================================
    // Section 3: LadderFilter — resonance + 4-pole sweep
    // ================================================================
    {
        std::printf("Section 3: LadderFilter — resonance + drive\n");
        LadderFilter ladder;
        ladder.prepare(spec);

        LadderFilter::Params ladderParams;
        ladderParams.temperature = 25.0f;

        for (int i = 0; i < kSectionLen; ++i)
        {
            float x = testSig[(size_t)(offset + i)];
            float t = (float)i / (float)kSectionLen;

            // Cutoff: 80Hz → 8000Hz log sweep
            ladderParams.cutoffHz = 80.0f * std::pow(100.0f, t);

            // L: high resonance (near self-oscillation)
            ladderParams.resonance = 0.85f;
            ladderParams.drive = 0.3f;
            output[0][(size_t)(offset + i)] = ladder.process(0, x, ladderParams);

            // R: low resonance + high drive
            ladderParams.resonance = 0.3f;
            ladderParams.drive = 0.8f;
            output[1][(size_t)(offset + i)] = ladder.process(1, x, ladderParams);
        }
    }
    offset += kSectionLen;

    // ================================================================
    // Section 4: TubePreamp — drive variation
    // ================================================================
    {
        std::printf("Section 4: TubePreamp — 12AX7 drive variation\n");
        TubePreamp preamp;
        preamp.prepare(spec);

        TubePreamp::Params preParams;
        preParams.bias = 0.5f;
        preParams.outputLevel = 0.6f;
        preParams.enableGridConduction = true;
        preParams.tubeAge = 0.0f;
        preParams.supplyRipple = 0.0f;

        const int third = kSectionLen / 3;
        for (int i = 0; i < kSectionLen; ++i)
        {
            float x = testSig[(size_t)(offset + i)];

            // L: gradually increasing drive (clean → crunch → lead)
            if (i < third)
                preParams.drive = 0.2f;    // clean
            else if (i < third * 2)
                preParams.drive = 0.5f;    // crunch
            else
                preParams.drive = 0.9f;    // lead

            output[0][(size_t)(offset + i)] = preamp.process(0, x, preParams);

            // R: aged tube + supply ripple
            TubePreamp::Params agedParams = preParams;
            agedParams.tubeAge = 0.6f;
            agedParams.supplyRipple = 0.15f;
            output[1][(size_t)(offset + i)] = preamp.process(1, x, agedParams);
        }
    }
    offset += kSectionLen;

    // ================================================================
    // Section 5: TapeSaturation — テープ速度と飽和
    // ================================================================
    {
        std::printf("Section 5: TapeSaturation — tape compression + wow/flutter\n");
        TapeSaturation tape;
        tape.prepare(spec);

        TapeSaturation::Params tapeParams;
        tapeParams.inputGain = 0.0f;
        tapeParams.biasAmount = 0.5f;
        tapeParams.enableHeadBump = true;
        tapeParams.enableHfRolloff = true;
        tapeParams.headWear = 0.0f;
        tapeParams.tapeAge = 0.0f;

        const int half = kSectionLen / 2;
        for (int i = 0; i < kSectionLen; ++i)
        {
            float x = testSig[(size_t)(offset + i)];

            if (i < half)
            {
                // L: 15 ips, medium saturation, no wow/flutter
                tapeParams.saturation = 0.4f;
                tapeParams.tapeSpeed = 1.0f;
                tapeParams.wowFlutter = 0.0f;
                output[0][(size_t)(offset + i)] = tape.process(0, x, tapeParams);

                // R: 7.5 ips, high saturation, wow/flutter present
                tapeParams.saturation = 0.8f;
                tapeParams.tapeSpeed = 0.5f;
                tapeParams.wowFlutter = 0.5f;
                output[1][(size_t)(offset + i)] = tape.process(1, x, tapeParams);
            }
            else
            {
                // second half: aged tape
                tapeParams.saturation = 0.6f;
                tapeParams.tapeSpeed = 1.0f;
                tapeParams.wowFlutter = 0.3f;
                tapeParams.headWear = 0.5f;
                tapeParams.tapeAge = 0.7f;
                output[0][(size_t)(offset + i)] = tape.process(0, x, tapeParams);

                tapeParams.headWear = 0.0f;
                tapeParams.tapeAge = 0.0f;
                output[1][(size_t)(offset + i)] = tape.process(1, x, tapeParams);
            }
        }
    }
    offset += kSectionLen;

    // ================================================================
    // Section 6: AnalogLfo → PhaserStage (LFO modulation)
    // ================================================================
    {
        std::printf("Section 6: AnalogLfo → PhaserStage — phaser\n");
        AnalogLfo lfo;
        PhaserStage phaser;
        lfo.prepare(spec);
        phaser.prepare(spec);

        lfo.setRateHz(2.0);

        PhaserStage::Params phParams;
        phParams.depth = 0.7f;
        phParams.feedback = 0.5f;
        phParams.centerFreqHz = 800.0f;
        phParams.freqSpreadHz = 600.0f;
        phParams.numStages = 6;
        phParams.temperature = 25.0f;

        for (int i = 0; i < kSectionLen; ++i)
        {
            float x = testSig[(size_t)(offset + i)];
            lfo.stepAll();

            // L: triangle LFO → phaser
            phParams.lfoValue = lfo.getTri(0);
            output[0][(size_t)(offset + i)] = phaser.process(0, x, phParams);

            // R: sine-like LFO → phaser (smoother)
            phParams.lfoValue = lfo.getSinLike(1);
            output[1][(size_t)(offset + i)] = phaser.process(1, x, phParams);
        }
    }

    // ================================================================
    // Section 7: CompanderModule → DelayLine → BbdStageEmulator
    //            Hand-build a BBD delay using only L3 modules (no BbdDelayEngine)
    // ================================================================
    {
        std::printf("Section 7: Hand-built BBD delay (Compander+Delay+BbdStage)\n");

        // ---- Initialize modules ----
        CompanderModule compressor;
        CompanderModule expander;
        BbdStageEmulator bbdEmu;
        BbdSampler bbdSampler;
        ToneFilter tone;

        compressor.prepare(spec);
        expander.prepare(spec);
        bbdEmu.prepare(spec);
        bbdSampler.prepare(spec);
        tone.prepare(spec);
        tone.setDefaultCutoff(3000.0f);

        // ---- Delay buffer (simple ring buffer) ----
        constexpr float kDelayMs = 200.0f;
        const int delaySamples = (int)(kSampleRate * kDelayMs / 1000.0f);
        const int bufLen = delaySamples + 1;
        std::vector<std::vector<float>> delayBuf(kChannels,
            std::vector<float>((size_t)bufLen, 0.0f));
        int writePos = 0;

        // ---- Feedback values ----
        std::vector<float> fb(kChannels, 0.0f);
        constexpr float kFeedback = 0.45f;
        constexpr float kCompAmount = 0.5f;
        constexpr float kMix = 0.5f;
        constexpr int kBbdStages = 4096;

        // BBD step
        const double step = std::max(1.0, (double)delaySamples / (double)kBbdStages);

        // RNG (required by BbdSampler)
        std::minstd_rand rng(42);
        std::normal_distribution<double> normalDist(0.0, 1.0);

        std::vector<float> bbdFrame(kChannels);

        for (int i = 0; i < kSectionLen; ++i)
        {
            float dry = testSig[(size_t)(offset + i)];

            for (int ch = 0; ch < kChannels; ++ch)
            {
                // (1) NE570 compressor (pre-BBD)
                float wet = compressor.processCompress(ch, dry, kCompAmount);

                // (2) feedback mix → write to delay
                float toDelay = wet + fb[(size_t)ch] * kFeedback;
                delayBuf[(size_t)ch][(size_t)writePos] = toDelay;

                // (3) delay read (linear interpolation)
                int readIdx = writePos - delaySamples;
                if (readIdx < 0) readIdx += bufLen;
                float delayed = delayBuf[(size_t)ch][(size_t)readIdx];

                // (4) BBD sampler (S&H quantization)
                delayed = bbdSampler.processSample(ch, delayed, step,
                                                   true, false, 0.0,
                                                   rng, normalDist);

                bbdFrame[(size_t)ch] = delayed;
            }

            // (5) BBD stage emulator (all channels)
            bbdEmu.process(bbdFrame, step, kBbdStages, 9.0, false, 0.0);

            for (int ch = 0; ch < kChannels; ++ch)
            {
                float wetOut = bbdFrame[(size_t)ch];

                // (6) NE570 expander (post-BBD)
                wetOut = expander.processExpand(ch, wetOut, kCompAmount);

                // (7) tone filter
                wetOut = tone.processSample(ch, wetOut);

                // store feedback
                fb[(size_t)ch] = wetOut;

                // (8) Dry/Wet mix (equal power)
                float gDry, gWet;
                DryWetMixer::equalPowerGainsFast(kMix, gDry, gWet);
                output[ch][(size_t)(offset + i)] = dry * gDry + wetOut * gWet;
            }

            writePos = (writePos + 1) % bufLen;
        }

        std::printf("  Manual BBD chain: delay=%.0fms, stages=%d, feedback=%.0f%%\n",
                kDelayMs, kBbdStages, kFeedback * 100.0f);
    }

    // --- WAV output ---
    if (wav::write("output_l3_circuits.wav", output, kSampleRate))
        std::printf("\n=> output_l3_circuits.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, (float)kTotalSamples / kSampleRate);
    else
        std::fprintf(stderr, "Error: failed to write WAV\n");

    return 0;
}
