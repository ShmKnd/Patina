/*
    example_channel_strip_engine.cpp — ChannelStripEngine sample
    ============================================================
    An analog-console style channel strip:
        Input → NoiseGate → TubePreamp (12AX7) → SVF EQ → TransformerModel → Output

    Three scenarios:
        1. Clean recording  — light preamp + LP EQ
        2. Rock vocal      — medium preamp + BP EQ + transformer color
        3. High-gain       — heavy preamp + HP EQ + aged tube

    Build:
        c++ -std=c++17 -O2 -I.. example_channel_strip_engine.cpp -o example_strip
*/

#include "include/patina.h"
#include "examples/wav_writer.h"

#include <cstdio>
#include <vector>

int main()
{
    constexpr int   kSampleRate   = 48000;
    constexpr int   kBlockSize    = 256;
    constexpr int   kChannels     = 2;
    constexpr float kDurationSec  = 4.5f;
    constexpr int   kTotalSamples = (int)(kSampleRate * kDurationSec);

    patina::ChannelStripEngine engine;
    patina::ProcessSpec spec{(double)kSampleRate, kBlockSize, kChannels};
    engine.prepare(spec);

    auto testSignal = wav::sineBurst(kSampleRate, 440.0f, kDurationSec, 0.6f, 30.0f, 300.0f);

    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    const int segmentLen = kTotalSamples / 3;

    struct StripSetting {
        const char* name;
        float drive;
        float bias;
        float tubeAge;
        int   eqType;     // 0=LP, 1=HP, 2=BP, 3=Notch
        float eqFreq;
        float eqRes;
    };
    StripSetting settings[] = {
        {"Clean recording (LP EQ)",   0.2f, 0.5f, 0.0f, 0, 3000.0f, 0.3f},
        {"Rock vocal (BP EQ)",        0.5f, 0.5f, 0.0f, 2, 2000.0f, 0.6f},
        {"High-gain (HP + aged tube)",0.8f, 0.45f,0.4f, 1, 200.0f,  0.4f},
    };

    for (int seg = 0; seg < 3; ++seg)
    {
        engine.reset();
        auto& s = settings[seg];

        patina::ChannelStripEngine::Params params;
        params.preampDrive      = s.drive;
        params.preampBias       = s.bias;
        params.preampOutput     = 0.7f;
        params.tubeAge          = s.tubeAge;

        params.enableEq         = true;
        params.eqType           = s.eqType;
        params.eqCutoffHz       = s.eqFreq;
        params.eqResonance      = s.eqRes;

        params.enableGate       = true;
        params.gateThresholdDb  = -55.0f;

        std::printf("  Section %d: %s\n", seg + 1, s.name);

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

        // Metering output
        std::printf("    Output level: %.3f\n", engine.getOutputLevel(0));
    }

    if (wav::write("output_channel_strip.wav", output, kSampleRate))
        std::printf("=> output_channel_strip.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, kDurationSec);

    return 0;
}
