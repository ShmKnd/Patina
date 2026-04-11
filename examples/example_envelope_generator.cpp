/*
    example_envelope_generator.cpp — EnvelopeGeneratorEngine sample
    ============================================================
    Demo of an analog ADSR envelope generator:
        1. ADSR (RC curve) — basic synth patch: AnalogVCO + ADSR
        2. AD (linear curve) — percussive one-shot envelope
        3. Auto trigger — automatic triggering based on input level

    Build:
        c++ -std=c++17 -O2 -I.. example_envelope_generator.cpp -o example_envgen
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
    constexpr float kDurationSec  = 6.0f;  // 3 sections × 2 sec
    constexpr int   kTotalSamples = (int)(kSampleRate * kDurationSec);

    patina::EnvelopeGeneratorEngine engine;
    AnalogVCO vco;

    patina::ProcessSpec spec{(double)kSampleRate, kBlockSize, kChannels};
    engine.prepare(spec);
    vco.prepare(spec);

    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    const int segmentLen = kTotalSamples / 3;

    // ===== Section 1: ADSR + VCO — classic synth patch =====
    {
        std::printf("  Section 1: ADSR (RC curve) + VCO 440Hz\n");
        engine.reset();
        vco.reset();

        AnalogVCO::Spec vcoSpec;
        vcoSpec.freqHz   = 440.0;
        vcoSpec.waveform = 0;  // Saw
        vcoSpec.drift    = 0.001;

        patina::EnvelopeGeneratorEngine::Params params;
        params.attack    = 0.2f;      // ~200 ms attack
        params.decay     = 0.3f;      // ~300 ms decay
        params.sustain   = 0.6f;      // 60% sustain
        params.release   = 0.3f;      // ~300 ms release
        params.envMode   = 0;         // ADSR
        params.curve     = 0;         // RC (指数)
        params.vcaDepth  = 1.0f;
        params.outputGain = 0.6f;
        params.mix       = 1.0f;

        // Generate VCO audio → apply envelope
        const int noteOnSample  = 0;
        const int noteOffSample = (int)(kSampleRate * 1.0f); // 1 秒後にゲートOFF

        for (int pos = 0; pos < segmentLen; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, segmentLen - pos);

            // VCO -> buffer
            float bufL[kBlockSize], bufR[kBlockSize];
            for (int i = 0; i < n; ++i)
            {
                float v = vco.process(0, vcoSpec) * 0.7f;
                bufL[i] = v;
                bufR[i] = v;
            }

            // Gate events
            if (pos <= noteOnSample && pos + n > noteOnSample)
                engine.gateOn();
            if (pos <= noteOffSample && pos + n > noteOffSample)
                engine.gateOff();

            const float* inputs[] = {bufL, bufR};
            float outL[kBlockSize], outR[kBlockSize];
            float* outputs_ptr[] = {outL, outR};

            engine.processBlock(inputs, outputs_ptr, kChannels, n, params);

            for (int i = 0; i < n; ++i)
            {
                output[0][(size_t)(pos + i)] = outL[i];
                output[1][(size_t)(pos + i)] = outR[i];
            }
        }
    }

    // ===== Section 2: AD (Linear) — percussive =====
    {
        std::printf("  Section 2: AD (linear curve) + VCO 220Hz — percussive\n");
        engine.reset();
        vco.reset();

        AnalogVCO::Spec vcoSpec;
        vcoSpec.freqHz   = 220.0;
        vcoSpec.waveform = 1;  // Tri
        vcoSpec.drift    = 0.002;

        patina::EnvelopeGeneratorEngine::Params params;
        params.attack    = 0.02f;     // ~20 ms very short attack
        params.decay     = 0.15f;     // ~150 ms decay
        params.envMode   = 1;         // AD
        params.curve     = 1;         // Linear
        params.vcaDepth  = 1.0f;
        params.outputGain = 0.6f;
        params.mix       = 1.0f;

        const int base = segmentLen;
        const int hitInterval = kSampleRate / 4; // 4 回/秒のトリガー

        for (int pos = 0; pos < segmentLen; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, segmentLen - pos);

            float bufL[kBlockSize], bufR[kBlockSize];
            for (int i = 0; i < n; ++i)
            {
                float v = vco.process(0, vcoSpec) * 0.7f;
                bufL[i] = v;
                bufR[i] = v;
            }

            // Periodically gate ON (one-shot)
            for (int i = 0; i < n; ++i)
            {
                if ((pos + i) % hitInterval == 0)
                {
                    engine.gateOn();
                }
            }

            const float* inputs[] = {bufL, bufR};
            float outL[kBlockSize], outR[kBlockSize];
            float* outputs_ptr[] = {outL, outR};

            engine.processBlock(inputs, outputs_ptr, kChannels, n, params);

            for (int i = 0; i < n; ++i)
            {
                output[0][(size_t)(base + pos + i)] = outL[i];
                output[1][(size_t)(base + pos + i)] = outR[i];
            }
        }
    }

    // ===== Section 3: Auto Trigger — automatic trigger by input level =====
    {
        std::printf("  Section 3: Auto trigger — amplitude-gated ADSR\n");
        engine.reset();

        patina::EnvelopeGeneratorEngine::Params params;
        params.attack          = 0.1f;
        params.decay           = 0.2f;
        params.sustain         = 0.5f;
        params.release         = 0.2f;
        params.envMode         = 0;   // ADSR
        params.triggerMode     = patina::EnvelopeGeneratorEngine::Auto;
        params.autoThresholdDb = -24.0f;
        params.vcaDepth        = 0.8f;
        params.outputGain      = 0.6f;
        params.mix             = 1.0f;

        const int base = segmentLen * 2;

        // Intermittent input signal (0.5 sec ON / 0.5 sec OFF)
        for (int pos = 0; pos < segmentLen; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, segmentLen - pos);

            float bufL[kBlockSize], bufR[kBlockSize];
            for (int i = 0; i < n; ++i)
            {
                float t = (float)(pos + i) / (float)kSampleRate;
                bool on = std::fmod(t, 1.0f) < 0.5f;
                float v = on ? 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * t) : 0.0f;
                bufL[i] = v;
                bufR[i] = v;
            }

            const float* inputs[] = {bufL, bufR};
            float outL[kBlockSize], outR[kBlockSize];
            float* outputs_ptr[] = {outL, outR};

            engine.processBlock(inputs, outputs_ptr, kChannels, n, params);

            for (int i = 0; i < n; ++i)
            {
                output[0][(size_t)(base + pos + i)] = outL[i];
                output[1][(size_t)(base + pos + i)] = outR[i];
            }
        }
    }

    // === WAV output ===
    std::vector<const float*> chPtrs(kChannels);
    for (int c = 0; c < kChannels; ++c)
        chPtrs[(size_t)c] = output[(size_t)c].data();

    const char* filename = "output_envelope_generator.wav";
    writeWav(filename, chPtrs.data(), kChannels, kTotalSamples, kSampleRate);
    std::printf("  Wrote %s (%d samples, %.1f sec)\n",
                filename, kTotalSamples, kDurationSec);

    return 0;
}
