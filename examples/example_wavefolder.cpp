/*
    example_wavefolder.cpp — WaveFolder sample
    ============================================================
    Demo of a classic modular-style wavefolder:
        1. Low fold  — gentle folding adding soft harmonics
        2. High fold — deep folding creating metallic harmonics
        3. Ge diode  — warm fold using germanium diodes

    Build:
        c++ -std=c++17 -O2 -I.. example_wavefolder.cpp -o example_wavefolder
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
    constexpr float kDurationSec  = 6.0f;  // 2 秒 × 3 セクション
    constexpr int   kTotalSamples = (int)(kSampleRate * kDurationSec);

    WaveFolder folder;
    patina::ProcessSpec spec{(double)kSampleRate, kBlockSize, kChannels};
    folder.prepare(spec);

    // Test signal: 200Hz sine burst (low freq makes folding effects audible)
    auto testSignal = wav::sineBurst(kSampleRate, 200.0f, kDurationSec, 0.6f, 80.0f, 300.0f);

    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    const int segmentLen = kTotalSamples / 3;

    // ===== Section 1: Low Fold (Si diode, 2 stages) =====
    {
        std::printf("  Section 1: Low Fold (Si, 2 stages)\n");
        folder.reset();

        WaveFolder::Params params;
        params.foldAmount = 0.3f;
        params.symmetry   = 1.0f;
        params.numStages  = 2;
        params.diodeType  = 0;  // Si

        for (int pos = 0; pos < segmentLen; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, segmentLen - pos);
            for (int i = 0; i < n; ++i)
            {
                float in = testSignal[(size_t)(pos + i)];
                float out = folder.process(0, in, params);
                output[0][(size_t)(pos + i)] = out;
                output[1][(size_t)(pos + i)] = out;
            }
        }
    }

    // ===== Section 2: High Fold (Si diode, 6 stages) =====
    {
        std::printf("  Section 2: High Fold (Si, 6 stages, asymmetric)\n");
        folder.reset();

        WaveFolder::Params params;
        params.foldAmount = 0.85f;
        params.symmetry   = 0.6f;   // Asymmetry adds even harmonics
        params.numStages  = 6;
        params.diodeType  = 0;

        const int start = segmentLen;
        const int end   = segmentLen * 2;
        for (int pos = start; pos < end; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, end - pos);
            for (int i = 0; i < n; ++i)
            {
                float in = testSignal[(size_t)(pos + i)];
                float out = folder.process(0, in, params);
                output[0][(size_t)(pos + i)] = out;
                output[1][(size_t)(pos + i)] = out;
            }
        }
    }

    // ===== Section 3: Ge diode fold (warm, vintage) =====
    {
        std::printf("  Section 3: Ge Diode Fold (warm vintage)\n");
        folder.reset();

        WaveFolder::Params params;
        params.foldAmount = 0.7f;
        params.symmetry   = 0.8f;
        params.numStages  = 4;
        params.diodeType  = 2;  // Ge
        params.temperature = 30.0f;

        const int start = segmentLen * 2;
        for (int pos = start; pos < kTotalSamples; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, kTotalSamples - pos);
            for (int i = 0; i < n; ++i)
            {
                float in = testSignal[(size_t)(pos + i)];
                float out = folder.process(0, in, params);
                output[0][(size_t)(pos + i)] = out;
                output[1][(size_t)(pos + i)] = out;
            }
        }
    }

    if (wav::write("output_wavefolder.wav", output, kSampleRate))
        std::printf("=> output_wavefolder.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, kDurationSec);
    else
        std::printf("ERROR: failed to write WAV\n");

    return 0;
}
