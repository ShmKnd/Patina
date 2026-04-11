/*
    example_tape_machine_engine.cpp — TapeMachineEngine sample
    ============================================================
    Studio tape machine simulation:
        - Tape saturation (magnetic hysteresis)
        - Wow & flutter
        - Head bump (low-frequency resonance)
        - Output transformer model (British console color)

    Three sections:
        1. Clean tape (15 ips, low saturation)
        2. Hot tape   (15 ips, high saturation + transformer)
        3. Lo-fi      (7.5 ips, head wear + tape degradation)

    Build:
        c++ -std=c++17 -O2 -I.. example_tape_machine_engine.cpp -o example_tape
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

    patina::TapeMachineEngine engine;
    patina::ProcessSpec spec{(double)kSampleRate, kBlockSize, kChannels};
    engine.prepare(spec);

    auto testSignal = wav::sineBurst(kSampleRate, 220.0f, kDurationSec, 0.7f, 40.0f, 300.0f);

    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    const int segmentLen = kTotalSamples / 3;

    struct TapeSetting {
        const char* name;
        float saturation;
        float tapeSpeed;
        float wowFlutter;
        float headWear;
        float tapeAge;
        bool  enableTransformer;
        float transformerDrive;
    };
    TapeSetting settings[] = {
        {"Clean tape (15 ips)",     0.2f, 1.0f, 0.0f, 0.0f, 0.0f, false, 0.0f},
        {"Hot tape + Transformer",  0.7f, 1.0f, 0.05f, 0.0f, 0.0f, true, 3.0f},
        {"Lo-fi (7.5 ips, worn)",   0.5f, 0.5f, 0.3f, 0.6f, 0.5f, true, 1.0f},
    };

    for (int seg = 0; seg < 3; ++seg)
    {
        engine.reset();
        auto& s = settings[seg];

        patina::TapeMachineEngine::Params params;
        params.saturation         = s.saturation;
        params.biasAmount         = 0.5f;
        params.tapeSpeed          = s.tapeSpeed;
        params.wowFlutter         = s.wowFlutter;
        params.enableHeadBump     = true;
        params.enableHfRolloff    = true;
        params.headWear           = s.headWear;
        params.tapeAge            = s.tapeAge;
        params.enableTransformer  = s.enableTransformer;
        params.transformerDrive   = s.transformerDrive;
        params.transformerSat     = 0.4f;
        params.tone               = 0.5f;
        params.mix                = 1.0f;

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
    }

    if (wav::write("output_tape.wav", output, kSampleRate))
        std::printf("=> output_tape.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, kDurationSec);

    return 0;
}
