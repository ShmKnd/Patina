/*
    example_ringmod.cpp — RingModulator sample
    ============================================================
    Demo of a 4-diode-bridge ring modulator:
        1. Si diode (500Hz carrier) — crisp ring modulation
        2. Schottky diode (800Hz carrier) — bright bell-like tones
        3. Ge diode (300Hz carrier) — warm with more carrier leakage

    Build:
        c++ -std=c++17 -O2 -I.. example_ringmod.cpp -o example_ringmod
*/

#include "include/patina.h"
#include "examples/wav_writer.h"

#include <cstdio>
#include <cmath>
#include <vector>

int main()
{
    constexpr int   kSampleRate   = 48000;
    constexpr int   kChannels     = 2;
    constexpr float kDurationSec  = 9.0f;  // 3 sec × 3 sections
    constexpr int   kTotalSamples = (int)(kSampleRate * kDurationSec);

    RingModulator ringMod;
    patina::ProcessSpec spec{(double)kSampleRate, 256, kChannels};
    ringMod.prepare(spec);

    // Input signal: 330Hz (E) sine burst
    auto testSignal = wav::sineBurst(kSampleRate, 330.0f, kDurationSec, 0.5f, 60.0f, 300.0f);

    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    const int segmentLen = kTotalSamples / 3;

    // キャリア生成用のフェーズ
    double carrierPhase = 0.0;

    auto generateCarrier = [&](double freqHz) -> float {
        float c = (float)std::sin(2.0 * M_PI * carrierPhase);
        carrierPhase += freqHz / (double)kSampleRate;
        if (carrierPhase >= 1.0) carrierPhase -= 1.0;
        return c;
    };

    // ===== Section 1: Si diode × 500Hz sine carrier =====
    {
        std::printf("  Section 1: Si diode x 500 Hz carrier — crisp ring mod\n");
        ringMod.reset();
        carrierPhase = 0.0;

        RingModulator::Params rmP;
        rmP.mix       = 0.8f;
        rmP.diodeType = 0;  // Si
        rmP.mismatch  = 0.02f;

        for (int i = 0; i < segmentLen; ++i)
        {
            float carrier = generateCarrier(500.0);
            float in  = testSignal[(size_t)i];
            float out = ringMod.process(0, in, carrier, rmP);
            output[0][(size_t)i] = out;
            output[1][(size_t)i] = out;
        }
    }

    // ===== Section 2: Schottky diode × 800Hz sine carrier =====
    {
        std::printf("  Section 2: Schottky diode x 800 Hz carrier — bright bell tones\n");
        ringMod.reset();
        carrierPhase = 0.0;

        RingModulator::Params rmP;
        rmP.mix       = 1.0f;
        rmP.diodeType = 1;  // Schottky
        rmP.mismatch  = 0.05f;

        const int start = segmentLen;
        const int end   = segmentLen * 2;
        for (int i = start; i < end; ++i)
        {
            float carrier = generateCarrier(800.0);
            float in  = testSignal[(size_t)i];
            float out = ringMod.process(0, in, carrier, rmP);
            output[0][(size_t)i] = out;
            output[1][(size_t)i] = out;
        }
    }

    // ===== Section 3: Ge diode × 300Hz sine carrier =====
    {
        std::printf("  Section 3: Ge diode x 300 Hz carrier — warm leaky ring mod\n");
        ringMod.reset();
        carrierPhase = 0.0;

        RingModulator::Params rmP;
        rmP.mix         = 0.7f;
        rmP.diodeType   = 2;  // Ge
        rmP.mismatch    = 0.08f;
        rmP.temperature = 30.0f;

        const int start = segmentLen * 2;
        for (int i = start; i < kTotalSamples; ++i)
        {
            float carrier = generateCarrier(300.0);
            float in  = testSignal[(size_t)i];
            float out = ringMod.process(0, in, carrier, rmP);
            output[0][(size_t)i] = out;
            output[1][(size_t)i] = out;
        }
    }

    if (wav::write("output_ringmod.wav", output, kSampleRate))
        std::printf("=> output_ringmod.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, kDurationSec);
    else
        std::printf("ERROR: failed to write WAV\n");

    return 0;
}
