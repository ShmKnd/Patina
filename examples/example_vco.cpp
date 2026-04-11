/*
    example_vco.cpp — AnalogVCO sample
    ============================================================
    Demo of an analog VCO using a BJT differential-pair current source + RC integrator:
        1. Saw  — classic analog synth sawtooth
        2. Pulse — rectangular wave + PWM sweep (tone variation via pulse-width modulation)
        3. Tri  — triangle wave with large temperature drift for vintage character

    Build:
        c++ -std=c++17 -O2 -I.. example_vco.cpp -o example_vco
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

    AnalogVCO vco;
    patina::ProcessSpec spec{(double)kSampleRate, 256, kChannels};
    vco.prepare(spec);

    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    const int segmentLen = kTotalSamples / 3;

    // ===== Section 1: Saw 440Hz — classic analog sawtooth =====
    {
        std::printf("  Section 1: Saw 440 Hz — classic analog sawtooth\n");
        vco.reset();

        AnalogVCO::Spec s;
        s.freqHz    = 440.0;
        s.waveform  = 0;  // Saw
        s.drift     = 0.001;

        for (int i = 0; i < segmentLen; ++i)
        {
            float out = vco.process(0, s) * 0.7f;
            output[0][(size_t)i] = out;
            output[1][(size_t)i] = out;
        }
    }

    // ===== Section 2: Pulse 330Hz — PWM sweep =====
    {
        std::printf("  Section 2: Pulse 330 Hz — PWM sweep 0.1 -> 0.9\n");
        vco.reset();

        AnalogVCO::Spec s;
        s.freqHz    = 330.0;
        s.waveform  = 2;  // Pulse
        s.drift     = 0.002;

        const int start = segmentLen;
        const int end   = segmentLen * 2;
        for (int i = start; i < end; ++i)
        {
            // Sweep pulse width from 0.1 → 0.9
            float t = (float)(i - start) / (float)(end - start);
            s.pulseWidth = 0.1 + 0.8 * (double)t;

            float out = vco.process(0, s) * 0.7f;
            output[0][(size_t)i] = out;
            output[1][(size_t)i] = out;
        }
    }

    // ===== Section 3: Tri 220Hz — large vintage-style drift =====
    {
        std::printf("  Section 3: Tri 220 Hz — large drift, vintage feel\n");
        vco.reset();

        AnalogVCO::Spec s;
        s.freqHz      = 220.0;
        s.waveform    = 1;  // Tri
        s.temperature = 35.0;   // 高めの温度
        s.drift       = 0.008;  // 大きなドリフト

        const int start = segmentLen * 2;
        for (int i = start; i < kTotalSamples; ++i)
        {
            float out = vco.process(0, s) * 0.7f;
            output[0][(size_t)i] = out;
            output[1][(size_t)i] = out;
        }
    }

    if (wav::write("output_vco.wav", output, kSampleRate))
        std::printf("=> output_vco.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, kDurationSec);
    else
        std::printf("ERROR: failed to write WAV\n");

    return 0;
}
