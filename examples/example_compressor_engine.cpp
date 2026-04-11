/*
    example_compressor_engine.cpp — CompressorEngine sample
    ============================================================
    Apply several analog compressor types in sequence:
        1. Photo (classic opto style — optical)
        2. FET   (fast FET style)
        3. Variable-Mu (tube-based variable-mu style)

    Build:
        c++ -std=c++17 -O2 -I.. example_compressor_engine.cpp -o example_comp
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
    constexpr float kDurationSec  = 6.0f;  // 4 types × 1.5 sec
    constexpr int   kTotalSamples = (int)(kSampleRate * kDurationSec);

    patina::CompressorEngine engine;
    patina::ProcessSpec spec{(double)kSampleRate, kBlockSize, kChannels};
    engine.prepare(spec);

    // Generate a signal with dynamic contrast for clearer compression behavior
    auto testSignal = wav::sineBurst(kSampleRate, 330.0f, kDurationSec, 0.9f, 40.0f, 200.0f);

    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    const int segmentLen = kTotalSamples / 4;

    struct CompSetting {
        const char* name;
        int type;
    };
    CompSetting settings[] = {
        {"Photo (LA-2A style)",   patina::CompressorEngine::Photo},
        {"FET (1176 style)",      patina::CompressorEngine::Fet},
        {"Variable-Mu (670)",     patina::CompressorEngine::VariableMu},
        {"VCA (SSL style)",       patina::CompressorEngine::Vca},
    };

    for (int seg = 0; seg < 4; ++seg)
    {
        engine.reset();

        patina::CompressorEngine::Params params;
        params.type       = settings[seg].type;
        params.inputGain  = 0.7f;
        params.threshold  = 0.5f;
        params.outputGain = 0.6f;
        params.attack     = 0.3f;     // FET で使用
        params.release    = 0.5f;     // FET で使用
        params.ratio      = 2;        // FET: 4:1, VariableMu: TC selector 2
        params.mix        = 1.0f;

        // Enable noise gate (suppress noise during low signal)
        params.enableGate      = true;
        params.gateThresholdDb = -50.0f;

        std::printf("  Section %d: %s\n", seg + 1, settings[seg].name);

        const int start = seg * segmentLen;
        const int end   = (seg == 3) ? kTotalSamples : start + segmentLen;

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
            case patina::CompressorEngine::Photo:
                gr = engine.getPhotoGainReductionDb(); break;
            case patina::CompressorEngine::Fet:
                gr = engine.getFetGainReductionDb(); break;
            case patina::CompressorEngine::VariableMu:
                gr = engine.getVarMuGainReductionDb(); break;
            case patina::CompressorEngine::Vca:
                gr = engine.getVcaGainReductionDb(); break;
        }
        std::printf("    Gain Reduction: %.1f dB\n", gr);
    }

    if (wav::write("output_compressor.wav", output, kSampleRate))
        std::printf("=> output_compressor.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, kDurationSec);

    return 0;
}
