/*
    example_reverb_engine.cpp — ReverbEngine sample
    ============================================================
    Comparison of spring-style and plate-style reverbs

    First half: spring reverb (with 'drip' effect)
    Second half: plate reverb (predelay + high diffusion)

    Build:
        c++ -std=c++17 -O2 -I.. example_reverb_engine.cpp -o example_reverb
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
    constexpr float kDurationSec  = 4.0f;
    constexpr int   kTotalSamples = (int)(kSampleRate * kDurationSec);

    patina::ReverbEngine engine;
    patina::ProcessSpec spec{(double)kSampleRate, kBlockSize, kChannels};
    engine.prepare(spec);

    // Percussive impulse signal (snap)
    auto testSignal = wav::sineBurst(kSampleRate, 800.0f, kDurationSec, 0.7f, 5.0f, 1500.0f);

    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    const int halfLen = kTotalSamples / 2;

    // ===== Section 1: Spring Reverb =====
    {
        std::printf("  Section 1: Spring Reverb (Accutronics)\n");

        patina::ReverbEngine::Params params;
        params.type       = patina::ReverbEngine::Spring;
        params.decay      = 0.65f;
        params.tone       = 0.5f;
        params.mix        = 0.45f;
        params.tension    = 0.5f;
        params.dripAmount = 0.5f;    // spring 'drip' effect
        params.numSprings = 3;

        for (int pos = 0; pos < halfLen; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, halfLen - pos);
            const float* inPtrs[2] = { &testSignal[(size_t)pos],
                                       &testSignal[(size_t)pos] };
            float* outPtrs[2] = { &output[0][(size_t)pos],
                                  &output[1][(size_t)pos] };
            engine.processBlock(inPtrs, outPtrs, kChannels, n, params);
        }
    }

    // Reset to clear reverb tail
    engine.reset();

    // ===== Section 2: Plate Reverb =====
    {
        std::printf("  Section 2: Plate Reverb (EMT 140 style)\n");

        patina::ReverbEngine::Params params;
        params.type       = patina::ReverbEngine::Plate;
        params.decay      = 0.75f;
        params.tone       = 0.45f;
        params.mix        = 0.5f;
        params.predelayMs = 25.0f;   // 25 ms predelay
        params.damping    = 0.4f;
        params.diffusion  = 0.8f;
        params.modDepth   = 0.15f;   // 軽い内部変調

        for (int pos = halfLen; pos < kTotalSamples; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, kTotalSamples - pos);
            const float* inPtrs[2] = { &testSignal[(size_t)pos],
                                       &testSignal[(size_t)pos] };
            float* outPtrs[2] = { &output[0][(size_t)pos],
                                  &output[1][(size_t)pos] };
            engine.processBlock(inPtrs, outPtrs, kChannels, n, params);
        }
    }

    if (wav::write("output_reverb.wav", output, kSampleRate))
        std::printf("=> output_reverb.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, kDurationSec);

    return 0;
}
