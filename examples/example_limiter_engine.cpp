/*
    example_limiter_engine.cpp — LimiterEngine sample
    ============================================================
    Apply three analog limiter types in sequence:
        1. FET  (1176 All-Buttons style — very fast FET)
        2. VCA  (SSL / dbx style — ∞:1 hard knee)
        3. Opto (classic opto limit mode — photocell)

    Build:
        c++ -std=c++17 -O2 -I.. example_limiter_engine.cpp -o example_limiter
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
    constexpr float kDurationSec  = 4.5f;  // 3 types × 1.5 sec
    constexpr int   kTotalSamples = (int)(kSampleRate * kDurationSec);

    patina::LimiterEngine engine;
    patina::ProcessSpec spec{(double)kSampleRate, kBlockSize, kChannels};
    engine.prepare(spec);

    // Generate a signal with varying level to make dynamics apparent
    auto testSignal = wav::sineBurst(kSampleRate, 330.0f, kDurationSec, 0.9f, 40.0f, 200.0f);

    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    const int segmentLen = kTotalSamples / 3;

    struct LimSetting {
        const char* name;
        int type;
    };
    LimSetting settings[] = {
        {"FET (1176 All-Buttons)",     patina::LimiterEngine::Fet},
        {"VCA (SSL / dbx Inf:1)",      patina::LimiterEngine::Vca},
        {"Opto (LA-2A Limit mode)",    patina::LimiterEngine::Opto},
    };

    for (int seg = 0; seg < 3; ++seg)
    {
        engine.reset();

        patina::LimiterEngine::Params params;
        params.type       = settings[seg].type;
        params.ceiling    = 0.6f;       // -8 dBFS 天井
        params.attack     = 0.2f;
        params.release    = 0.4f;
        params.outputGain = 0.5f;
        params.mix        = 1.0f;

        std::printf("  Section %d: %s\n", seg + 1, settings[seg].name);

        const int start = seg * segmentLen;
        const int end   = (seg == 2) ? kTotalSamples : start + segmentLen;

        for (int pos = start; pos < end; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, end - pos);
            const float* inPtrs[2] = { &testSignal[(size_t)pos],
                                       &testSignal[(size_t)pos] };
            float* outPtrs[2] = { &output[0][(size_t)pos],
                                  &output[1][(size_t)pos] };

            engine.processBlock(inPtrs, outPtrs, kChannels, n, params);
        }

        // Display gain reduction
        float gr = 0.0f;
        switch (params.type)
        {
            case patina::LimiterEngine::Fet:
                gr = engine.getFetGainReductionDb(); break;
            case patina::LimiterEngine::Vca:
                gr = engine.getVcaGainReductionDb(); break;
            case patina::LimiterEngine::Opto:
                gr = engine.getOptoGainReductionDb(); break;
        }
        std::printf("    Gain Reduction: %.1f dB\n", gr);
    }

    if (wav::write("output_limiter.wav", output, kSampleRate))
        std::printf("=> output_limiter.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, kDurationSec);

    return 0;
}
