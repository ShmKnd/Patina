/*
    example_eq_engine.cpp — EqEngine sample
    ============================================================
    3-band parametric EQ (Low Shelf / Mid Bell / High Shelf)
    OTA-SVF based analog-console style equalization.

    This sample applies three EQ settings in sequence and
    writes their tonal differences into a single WAV.

    Build:
        c++ -std=c++17 -O2 -I.. example_eq_engine.cpp -o example_eq
*/

#include "include/patina.h"
#include "examples/wav_writer.h"

#include <cstdio>
#include <vector>

int main()
{
    constexpr int    kSampleRate   = 48000;
    constexpr int    kBlockSize    = 256;
    constexpr int    kChannels     = 2;
    constexpr float  kDurationSec  = 3.0f;  // 3 settings × 1 second each
    constexpr int    kTotalSamples = (int)(kSampleRate * kDurationSec);

    // ----- Engine initialization -----
    patina::EqEngine engine;
    patina::ProcessSpec spec{(double)kSampleRate, kBlockSize, kChannels};
    engine.prepare(spec);

    // ----- Test signal (white-noise burst — makes EQ effects obvious) -----
    auto testSignal = wav::sineBurst(kSampleRate, 440.0f, kDurationSec, 0.5f, 10.0f, 100.0f);

    // ----- Output buffer -----
    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    // ----- 3 sections: Low boost → Mid cut → High boost -----
    const int segmentLen = kTotalSamples / 3;
    const char* settingNames[] = {
        "Low Shelf +6dB @ 200Hz",
        "Mid Bell  -6dB @ 1kHz",
        "High Shelf +6dB @ 4kHz"
    };

    for (int seg = 0; seg < 3; ++seg)
    {
        patina::EqEngine::Params params;

        switch (seg)
        {
            case 0:  // Low boost
                params.lowGainDb   = 6.0f;
                params.lowFreqHz   = 200.0f;
                params.midGainDb   = 0.0f;
                params.highGainDb  = 0.0f;
                break;
            case 1:  // Mid cut
                params.lowGainDb   = 0.0f;
                params.midGainDb   = -6.0f;
                params.midFreqHz   = 1000.0f;
                params.midQ        = 0.5f;
                params.highGainDb  = 0.0f;
                break;
            case 2:  // High boost
                params.lowGainDb   = 0.0f;
                params.midGainDb   = 0.0f;
                params.highGainDb  = 6.0f;
                params.highFreqHz  = 4000.0f;
                break;
        }

        std::printf("  Section %d: %s\n", seg + 1, settingNames[seg]);

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

    // ----- WAV output -----
    if (wav::write("output_eq.wav", output, kSampleRate))
        std::printf("=> output_eq.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, kDurationSec);
    else
        std::fprintf(stderr, "Error: failed to write WAV\n");

    return 0;
}
