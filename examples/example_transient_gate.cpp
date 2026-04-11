/*
    example_transient_gate.cpp — EnvelopeGeneratorEngine transient shaper / noise gate sample
    ============================================================
    Demonstrates three effects using Auto trigger mode:
        1. Noise Gate       — complete mute below threshold (ADSR, Range=1.0)
        2. Soft Gate        — gentle attenuation below threshold (ADSR, Range=0.5)
        3. Transient Shaper — emphasize transients with very short Attack + short Decay (AD, Linear)

    Build:
        c++ -std=c++17 -O2 -I.. example_transient_gate.cpp -o example_transient_gate
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
    constexpr float kDurationSec  = 9.0f;   // 3 セクション × 3 秒
    constexpr int   kTotalSamples = (int)(kSampleRate * kDurationSec);
    constexpr float kPi           = 3.14159265f;

    patina::EnvelopeGeneratorEngine engine;
    patina::ProcessSpec spec{(double)kSampleRate, kBlockSize, kChannels};
    engine.prepare(spec);

    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    const int segmentLen = kTotalSamples / 3;

    // Test signal generator: drum-loop style intermittent bursts (loud + quiet + silence)
    auto makeTestSignal = [&](int length) -> std::vector<float> {
        std::vector<float> sig((size_t)length, 0.0f);
        for (int i = 0; i < length; ++i)
        {
            float t = (float)i / (float)kSampleRate;
            float period = std::fmod(t, 0.5f);  // 2 Hz 繰り返し

            float env = 0.0f;
            if (period < 0.05f)
            {
                // Loud transient hit (50ms)
                env = 0.9f * std::exp(-period * 30.0f);
            }
            else if (period >= 0.15f && period < 0.20f)
            {
                // Quiet ghost note (50ms)
                env = 0.08f * std::exp(-(period - 0.15f) * 40.0f);
            }
            // else silence — section that should be removed by the noise gate

            // 低域パルスっぽいサイン波
            float osc = std::sin(2.0f * kPi * 110.0f * t)
                      + 0.4f * std::sin(2.0f * kPi * 220.0f * t);
            sig[(size_t)i] = env * osc;
        }
        return sig;
    };

    // ===== Section 1: Noise Gate — full mute =====
    {
        std::printf("  Section 1: Noise Gate (Range=1.0, ADSR, RC curve)\n");
        engine.reset();

        auto sig = makeTestSignal(segmentLen);

        patina::EnvelopeGeneratorEngine::Params p;
        p.triggerMode     = patina::EnvelopeGeneratorEngine::Auto;
        p.autoThresholdDb = -24.0f;    // Do not pass ghost notes
        p.attack          = 0.02f;     // 高速オープン
        p.decay           = 0.2f;      // ホールド
        p.sustain         = 0.0f;      // サステインなし → ゲート的動作
        p.release         = 0.15f;     // スムーズなクローズ
        p.envMode         = 0;         // ADSR
        p.curve           = 0;         // RC (指数)
        p.vcaDepth        = 1.0f;      // 閉時に完全ミュート
        p.outputGain      = 0.5f;
        p.mix             = 1.0f;

        for (int pos = 0; pos < segmentLen; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, segmentLen - pos);
            const float* in[2]  = { &sig[(size_t)pos], &sig[(size_t)pos] };
            float* out[2] = { &output[0][(size_t)pos], &output[1][(size_t)pos] };
            engine.processBlock(in, out, kChannels, n, p);
        }

        std::printf("    Final envelope: %.3f\n", engine.getEnvelope(0));
    }

    // ===== Section 2: Soft Gate — gentle attenuation (Range=0.5) =====
    {
        std::printf("  Section 2: Soft Gate (Range=0.5, ADSR, RC curve)\n");
        engine.reset();

        auto sig = makeTestSignal(segmentLen);
        const int base = segmentLen;

        patina::EnvelopeGeneratorEngine::Params p;
        p.triggerMode     = patina::EnvelopeGeneratorEngine::Auto;
        p.autoThresholdDb = -30.0f;    // Lower threshold – allows ghost notes through
        p.attack          = 0.05f;     // やや遅めのオープン
        p.decay           = 0.3f;
        p.sustain         = 0.3f;      // ある程度サステイン維持
        p.release         = 0.3f;
        p.envMode         = 0;         // ADSR
        p.curve           = 0;         // RC
        p.vcaDepth        = 0.5f;      // At close, only about -6dB attenuation
        p.outputGain      = 0.5f;
        p.mix             = 1.0f;

        for (int pos = 0; pos < segmentLen; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, segmentLen - pos);
            const float* in[2]  = { &sig[(size_t)pos], &sig[(size_t)pos] };
            float* out[2] = { &output[0][(size_t)(base + pos)],
                              &output[1][(size_t)(base + pos)] };
            engine.processBlock(in, out, kChannels, n, p);
        }

        std::printf("    Final envelope: %.3f\n", engine.getEnvelope(0));
    }

    // ===== Section 3: Transient Shaper — attack emphasis =====
    {
        std::printf("  Section 3: Transient Shaper (AD, Linear, fast attack)\n");
        engine.reset();

        auto sig = makeTestSignal(segmentLen);
        const int base = segmentLen * 2;

        patina::EnvelopeGeneratorEngine::Params p;
        p.triggerMode     = patina::EnvelopeGeneratorEngine::Auto;
        p.autoThresholdDb = -20.0f;    // Detect only transients
        p.attack          = 0.01f;     // Very short attack → punch
        p.decay           = 0.08f;     // Short decay → quickly closes
        p.envMode         = 1;         // AD (ワンショット)
        p.curve           = 1;         // Linear — 歯切れの良いシェイプ
        p.vcaDepth        = 0.7f;      // Suppress non-attack parts by ~-10dB
        p.outputGain      = 0.6f;      // Make-up gain
        p.mix             = 0.7f;      // Parallel blend to add transients

        for (int pos = 0; pos < segmentLen; pos += kBlockSize)
        {
            const int n = std::min(kBlockSize, segmentLen - pos);
            const float* in[2]  = { &sig[(size_t)pos], &sig[(size_t)pos] };
            float* out[2] = { &output[0][(size_t)(base + pos)],
                              &output[1][(size_t)(base + pos)] };
            engine.processBlock(in, out, kChannels, n, p);
        }

        std::printf("    Final envelope: %.3f\n", engine.getEnvelope(0));
    }

    // === WAV output ===
    if (wav::write("output_transient_gate.wav", output, kSampleRate))
        std::printf("=> output_transient_gate.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, kDurationSec);

    return 0;
}
