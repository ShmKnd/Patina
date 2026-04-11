/*
    example_passive_lc_filter.cpp — PassiveLCFilter sample
    ============================================================
    Demo of passive LC filters (InductorPrimitive + RC_Element):
        Section 1: LPF — Halo inductor dark passive LP (cutoff sweep)
        Section 2: BPF — Wah inductor resonance (wah pedal style) with drive
        Section 3: HPF → Notch — linear HP via AirCore, then Notch for band removal

    Each section is 2 seconds (total 6 seconds).
    Inductor core saturation adds harmonics under large input;
    DCR losses reproduce the character of analog passive EQs.

    Build:
        c++ -std=c++17 -O2 -I.. example_passive_lc_filter.cpp -o example_passive_lc_filter
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
    constexpr float kSectionSec   = 2.0f;
    constexpr int   kSectionLen   = (int)(kSampleRate * kSectionSec);
    constexpr int   kNumSections  = 3;
    constexpr float kDurationSec  = kSectionSec * kNumSections;
    constexpr int   kTotalSamples = kSectionLen * kNumSections;

    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    // Test signal: sawtooth (rich in harmonics so filter differences are audible)
    auto sawSignal = [&](int i, float freq) -> float {
        float phase = std::fmod((float)i * freq / (float)kSampleRate, 1.0f);
        return (2.0f * phase - 1.0f) * 0.6f;
    };

    // サインバースト (テスト信号として併用)
    auto testSig = wav::sineBurst(kSampleRate, 220.0f, kDurationSec, 0.7f, 50.0f, 150.0f);

    std::printf("=== PassiveLCFilter Example ===\n\n");

    // ================================================================
    // Section 1: LPF — Halo インダクタ (Pultec 風パッシブ LP)
    //   カットオフ 2kHz → 500Hz へスイープ
    //   L: ドライブ低 (クリーン)  R: ドライブ高 (飽和倍音)
    // ================================================================
    {
        std::printf("Section 1: Halo LPF — Cutoff sweep (2kHz → 500Hz)\n");

        PassiveLCFilter filterL(InductorPrimitive::HaloInductor());
        PassiveLCFilter filterR(InductorPrimitive::HaloInductor());
        filterL.prepare(1, kSampleRate);
        filterR.prepare(1, kSampleRate);

        for (int i = 0; i < kSectionLen; ++i)
        {
            float t = (float)i / (float)kSectionLen;
            float cutoff = 2000.0f * (1.0f - t) + 500.0f * t;  // sweep

            float in = sawSignal(i, 220.0f);

            // L: クリーン (ドライブ 0)
            output[0][(size_t)i] = filterL.process(0, in, PassiveLCFilter::LPF, 0.0f);

            // R: saturated (drive 0.8) — inductor core saturation adds harmonics
            output[1][(size_t)i] = filterR.process(0, in, PassiveLCFilter::LPF, 0.8f);

            filterL.setCutoffHz(cutoff);
            filterR.setCutoffHz(cutoff);
        }

        std::printf("  L: drive=0 (clean)  R: drive=0.8 (saturated)\n");
        std::printf("  Inductor SRF: %.0f Hz\n",
                    filterL.getInductor().getSRF());
        std::printf("  Effective Q:  %.1f\n\n",
                    filterL.getEffectiveQ());
    }

    // ================================================================
    // Section 2: BPF — Fasel ワウインダクタ (ワウペダル・レゾナンス)
    //   カットオフ 400Hz → 2kHz → 400Hz (ワウスイープ)
    //   高レゾナンス + ドライブでインダクタ飽和のグリット感
    // ================================================================
    {
        std::printf("Section 2: Wah BPF — Resonant sweep (400 → 2k → 400 Hz)\n");

        PassiveLCFilter wahFilter(InductorPrimitive::WahInductor());
        wahFilter.prepare(1, kSampleRate);

        PassiveLCFilter::Params p;
        p.resonance = 0.85f;
        p.drive     = 0.5f;
        p.filterType = PassiveLCFilter::BPF;
        p.temperature = 30.0f;

        for (int i = 0; i < kSectionLen; ++i)
        {
            float t = (float)i / (float)kSectionLen;
            // Triangle LFO: 0→1→0 (wah-pedal style sweep)
            float lfo = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f);
            p.cutoffHz = 400.0f + lfo * 1600.0f;

            float in = testSig[(size_t)(kSectionLen + i)];
            // Mix in sawtooth for richer input
            in += sawSignal(i, 110.0f) * 0.4f;

            float out = wahFilter.process(0, in, p);
            output[0][(size_t)(kSectionLen + i)] = out;
            output[1][(size_t)(kSectionLen + i)] = out;
        }

        std::printf("  Wah inductor SRF: %.0f Hz\n",
                    wahFilter.getInductor().getSRF());
        std::printf("  Resonance: %.1f, Drive: %.1f\n\n",
                    p.resonance, p.drive);
    }

    // ================================================================
    // Section 3: HPF + Notch — AirCore リニアフィルタ
    //   前半 1 秒: HPF (200Hz) — ランブル除去
    //   後半 1 秒: Notch (1kHz) — 帯域除去
    //   AirCore インダクタ = コア飽和なし、純粋な LC 特性
    // ================================================================
    {
        std::printf("Section 3: AirCore HPF (200Hz) + Notch (1kHz)\n");

        PassiveLCFilter filter(InductorPrimitive::AirCore());
        filter.prepare(1, kSampleRate);

        const int halfSection = kSectionLen / 2;
        const int secStart = kSectionLen * 2;

            // First half: HPF
        {
            filter.setCutoffHz(200.0f);
            filter.setResonance(0.4f);

            for (int i = 0; i < halfSection; ++i)
            {
                float in = testSig[(size_t)(secStart + i)];
                in += sawSignal(i, 80.0f) * 0.5f;  // Add low content to confirm HPF effect

                float out = filter.process(0, in, PassiveLCFilter::HPF, 0.0f);
                output[0][(size_t)(secStart + i)] = out;
                output[1][(size_t)(secStart + i)] = out;
            }
        }

        // 後半: Notch
        {
            filter.reset();
            filter.setCutoffHz(1000.0f);
            filter.setResonance(0.7f);

            for (int i = halfSection; i < kSectionLen; ++i)
            {
                float in = testSig[(size_t)(secStart + i)];
                in += sawSignal(i, 440.0f) * 0.3f;

                float out = filter.process(0, in, PassiveLCFilter::Notch, 0.0f);
                output[0][(size_t)(secStart + i)] = out;
                output[1][(size_t)(secStart + i)] = out;
            }
        }

        std::printf("  AirCore Q at 1kHz: %.1f\n",
                    filter.getInductor().qAtFreq(1000.0));
        std::printf("  No core saturation (linear LC)\n\n");
    }

    // WAV 出力
    if (wav::write("output_passive_lc_filter.wav", output, kSampleRate))
        std::printf("=> output_passive_lc_filter.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, kDurationSec);
    else
        std::printf("ERROR: failed to write WAV\n");

    return 0;
}
