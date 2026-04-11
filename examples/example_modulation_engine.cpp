/*
    example_modulation_engine.cpp — ModulationEngine sample
    ============================================================
    Demonstrates three modulation effects in sequence:
        1. Phaser  — 6-stage JFET all-pass filter
        2. Tremolo — bias / optical / VCA modes
        3. Chorus  — short delay + LFO + stereo width

    Build:
        c++ -std=c++17 -O2 -I.. example_modulation_engine.cpp -o example_mod
*/

#include "include/patina.h"
#include "examples/wav_writer.h"

#include <cstdio>
#include <cmath>
#include <vector>

int main()
{
    constexpr int   kSampleRate   = 48000;
    constexpr int   kBlockSize    = 256;
    constexpr int   kChannels     = 2;
    constexpr float kDurationSec  = 6.0f;  // 2 sec × 3 types
    constexpr int   kTotalSamples = (int)(kSampleRate * kDurationSec);

    patina::ModulationEngine engine;
    patina::ProcessSpec spec{(double)kSampleRate, kBlockSize, kChannels};
    engine.prepare(spec);

    // Rich clean tone (E chord style)
    auto testSignal = wav::sineBurst(kSampleRate, 330.0f, kDurationSec, 0.5f, 50.0f, 250.0f);

    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    const int segmentLen = kTotalSamples / 3;

    // ===== Section 1: Phaser (6-stage, slow sweep) =====
    {
        std::printf("  Section 1: Phaser (6-stage JFET)\n");
        engine.reset();

        patina::ModulationEngine::Params params;
        params.type         = patina::ModulationEngine::Phaser;
        params.rate         = 0.8f;       // 0.8 Hz — slow sweep
        params.depth        = 0.7f;
        params.feedback     = 0.6f;       // stronger feedback for pronounced notches
        params.mix          = 0.6f;
        params.centerFreqHz = 800.0f;
        params.freqSpreadHz = 600.0f;
        params.numStages    = 6;

        for (int pos = 0; pos < segmentLen; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, segmentLen - pos);
            const float* inPtrs[2] = { &testSignal[(size_t)pos],
                                       &testSignal[(size_t)pos] };
            float* outPtrs[2] = { &output[0][(size_t)pos],
                                  &output[1][(size_t)pos] };
            engine.processBlock(inPtrs, outPtrs, kChannels, n, params);
        }
    }

    // ===== Section 2: Tremolo (Optical mode) =====
    {
        std::printf("  Section 2: Tremolo (Optical)\n");
        engine.reset();

        patina::ModulationEngine::Params params;
        params.type              = patina::ModulationEngine::Tremolo;
        params.rate              = 5.0f;    // 5 Hz — vintage amp style
        params.depth             = 0.7f;
        params.tremoloMode       = 1;       // Optical (photocell asymmetric response)
        params.stereoPhaseInvert = true;     // stereo phase invert for panning effect
        params.mix               = 1.0f;

        const int start = segmentLen;
        const int end   = segmentLen * 2;
        for (int pos = start; pos < end; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, end - pos);
            const float* inPtrs[2] = { &testSignal[(size_t)pos],
                                       &testSignal[(size_t)pos] };
            float* outPtrs[2] = { &output[0][(size_t)pos],
                                  &output[1][(size_t)pos] };
            engine.processBlock(inPtrs, outPtrs, kChannels, n, params);
        }
    }

    // ===== Section 3: Chorus (stereo widening) =====
    {
        std::printf("  Section 3: Chorus (Stereo)\n");
        engine.reset();

        patina::ModulationEngine::Params params;
        params.type          = patina::ModulationEngine::Chorus;
        params.rate          = 1.2f;     // 1.2 Hz
        params.depth         = 0.4f;
        params.mix           = 0.5f;
        params.chorusDelayMs = 8.0f;     // 8 ms base delay
        params.stereoWidth   = 0.7f;     // wide stereo image

        const int start = segmentLen * 2;
        for (int pos = start; pos < kTotalSamples; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, kTotalSamples - pos);
            const float* inPtrs[2] = { &testSignal[(size_t)pos],
                                       &testSignal[(size_t)pos] };
            float* outPtrs[2] = { &output[0][(size_t)pos],
                                  &output[1][(size_t)pos] };
            engine.processBlock(inPtrs, outPtrs, kChannels, n, params);
        }
    }

    if (wav::write("output_modulation.wav", output, kSampleRate))
        std::printf("=> output_modulation.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, kDurationSec);

    return 0;
}
