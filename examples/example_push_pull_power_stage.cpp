/*
    example_push_pull_power_stage.cpp — PushPullPowerStage sample
    ============================================================
    Demo of tube power amp output stages (PowerPentode ×2 + TransformerPrimitive):
        Section 1: EL34-style push-pull — British crunch character
        Section 2: 6L6-style push-pull — clean headroom
        Section 3: EL84-style — hot bias, near Class A, chimey tone
        Section 4: 6V6-style — sweet breakup
        Section 5: EL34-style — power sag comparison (0 vs 1.0)

    Each section is 2 seconds (total 10 seconds).
    Builds a full amp-head chain: TubePreamp → PushPullPowerStage.

    Build:
        c++ -std=c++17 -O2 -I.. example_push_pull_power_stage.cpp -o example_push_pull
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
    constexpr float kSectionSec   = 2.0f;
    constexpr int   kSectionLen   = (int)(kSampleRate * kSectionSec);
    constexpr int   kNumSections  = 5;
    constexpr float kDurationSec  = kSectionSec * kNumSections;
    constexpr int   kTotalSamples = kSectionLen * kNumSections;

    std::vector<std::vector<float>> output(kChannels,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    // Test signal: guitar-like sine burst (330 Hz = E4)
    auto testSig = wav::sineBurst(kSampleRate, 330.0f, kDurationSec, 0.7f, 40.0f, 200.0f);

    // Preamp (shared across all sections)
    TubePreamp preamp;
    preamp.prepare(1, kSampleRate);

    TubePreamp::Params preParams;
    preParams.drive = 0.6f;
    preParams.bias  = 0.5f;
    preParams.outputLevel = 0.8f;
    preParams.enableGridConduction = true;

    std::printf("=== PushPullPowerStage Example ===\n");
    std::printf("Signal chain: TubePreamp (12AX7) → PushPullPowerStage → WAV\n\n");

    // ================================================================
    // Section 1: Marshall50W — EL34 ブリティッシュ・クランチ
    // ================================================================
    {
        std::printf("Section 1: EL34-style push-pull — British crunch\n");

        auto amp = PushPullPowerStage::Marshall50W();
        amp.prepare(1, kSampleRate);
        preamp.reset();

        PushPullPowerStage::Params p;
        p.inputGainDb      = 6.0f;
        p.bias             = 0.65f;
        p.negativeFeedback = 0.3f;
        p.sagAmount        = 0.5f;
        p.outputLevel      = 0.7f;

        for (int i = 0; i < kSectionLen; ++i)
        {
            float in  = testSig[(size_t)i];
            float pre = preamp.process(0, in, preParams);
            float out = amp.process(0, pre, p);
            output[0][(size_t)i] = out;
            output[1][(size_t)i] = out;
        }

        std::printf("  Thermal state: %.6f\n", amp.getAverageThermalState());
        std::printf("  Tubes: %s × 2\n\n", amp.getTubeA().getSpec().name);
    }

    // ================================================================
    // Section 2: FenderTwin — 6L6GC クリーンヘッドルーム
    // ================================================================
    {
        std::printf("Section 2: 6L6-style push-pull — Clean headroom\n");

        auto amp = PushPullPowerStage::FenderTwin();
        amp.prepare(1, kSampleRate);
        preamp.reset();

        PushPullPowerStage::Params p;
        p.inputGainDb      = 3.0f;   // lower gain emphasizes clean
        p.bias             = 0.7f;
        p.negativeFeedback = 0.6f;   // 重い NFB → クリーン
        p.sagAmount        = 0.2f;   // レギュレートされた電源
        p.outputLevel      = 0.7f;

        const int offset = kSectionLen;
        for (int i = 0; i < kSectionLen; ++i)
        {
            float in  = testSig[(size_t)(offset + i)];
            float pre = preamp.process(0, in, preParams);
            float out = amp.process(0, pre, p);
            output[0][(size_t)(offset + i)] = out;
            output[1][(size_t)(offset + i)] = out;
        }

        std::printf("  NFB: %.1f (heavy — clean sound)\n", p.negativeFeedback);
        std::printf("  Tubes: %s × 2\n\n", amp.getTubeA().getSpec().name);
    }

    // ================================================================
    // Section 3: VoxAC30 — EL84 ニア class A、チャイミー
    // ================================================================
    {
        std::printf("Section 3: EL84-style — Chimey, near Class A\n");

        auto amp = PushPullPowerStage::VoxAC30();
        amp.prepare(1, kSampleRate);
        preamp.reset();

        PushPullPowerStage::Params p;
        p.inputGainDb      = 9.0f;   // drive harder to induce breakup
        p.bias             = 0.9f;   // hot bias (near Class A)
        p.negativeFeedback = 0.1f;   // minimal NFB -> open sound
        p.sagAmount        = 0.7f;
        p.outputLevel      = 0.65f;
        p.presenceHz       = 4000.0f;

        const int offset = kSectionLen * 2;
        for (int i = 0; i < kSectionLen; ++i)
        {
            float in  = testSig[(size_t)(offset + i)];
            float pre = preamp.process(0, in, preParams);
            float out = amp.process(0, pre, p);
            output[0][(size_t)(offset + i)] = out;
            output[1][(size_t)(offset + i)] = out;
        }

        std::printf("  Bias: %.1f (hot — near Class A)\n", p.bias);
        std::printf("  Tubes: %s × 2\n\n", amp.getTubeA().getSpec().name);
    }

    // ================================================================
    // Section 4: FenderDeluxe — 6V6GT スウィート・ブレイクアップ
    // ================================================================
    {
        std::printf("Section 4: 6V6-style — Sweet breakup\n");

        auto amp = PushPullPowerStage::FenderDeluxe();
        amp.prepare(1, kSampleRate);
        preamp.reset();

        // Increase drive to elicit the 6V6's sweet breakup
        TubePreamp::Params hotPre = preParams;
        hotPre.drive = 0.8f;

        PushPullPowerStage::Params p;
        p.inputGainDb      = 12.0f;  // パワーステージへのプッシュ
        p.bias             = 0.6f;
        p.negativeFeedback = 0.25f;
        p.sagAmount        = 0.6f;
        p.outputLevel      = 0.6f;

        const int offset = kSectionLen * 3;
        for (int i = 0; i < kSectionLen; ++i)
        {
            float in  = testSig[(size_t)(offset + i)];
            float pre = preamp.process(0, in, hotPre);
            float out = amp.process(0, pre, p);
            output[0][(size_t)(offset + i)] = out;
            output[1][(size_t)(offset + i)] = out;
        }

        std::printf("  Input gain: %.0f dB (pushing into breakup)\n", p.inputGainDb);
        std::printf("  Tubes: %s × 2\n\n", amp.getTubeA().getSpec().name);
    }

    // ================================================================
    // Section 5: Marshall50W — Power sag comparison
    //   L: sag=0 (regulated power) — tight and punchy
    //   R: sag=1 (vintage rectifier) — compressed, spongy
    // ================================================================
    {
        std::printf("Section 5: EL34-style — Sag comparison\n");

        auto ampTight  = PushPullPowerStage::Marshall50W();
        auto ampSpongy = PushPullPowerStage::Marshall50W();
        ampTight .prepare(1, kSampleRate);
        ampSpongy.prepare(1, kSampleRate);

        PushPullPowerStage::Params pTight;
        pTight.inputGainDb      = 12.0f;
        pTight.bias             = 0.65f;
        pTight.negativeFeedback = 0.3f;
        pTight.sagAmount        = 0.0f;   // レギュレート
        pTight.outputLevel      = 0.65f;

        PushPullPowerStage::Params pSpongy = pTight;
        pSpongy.sagAmount = 1.0f;          // フル・サグ

        TubePreamp preampL, preampR;
        preampL.prepare(1, kSampleRate);
        preampR.prepare(1, kSampleRate);

        const int offset = kSectionLen * 4;
        for (int i = 0; i < kSectionLen; ++i)
        {
            float in = testSig[(size_t)(offset + i)];
            float preL = preampL.process(0, in, preParams);
            float preR = preampR.process(0, in, preParams);

            output[0][(size_t)(offset + i)] = ampTight .process(0, preL, pTight);
            output[1][(size_t)(offset + i)] = ampSpongy.process(0, preR, pSpongy);
        }

        std::printf("  L: sag=0 (regulated, tight)    thermal=%.6f\n",
                    ampTight.getAverageThermalState());
        std::printf("  R: sag=1 (vintage, spongy)     thermal=%.6f\n\n",
                    ampSpongy.getAverageThermalState());
    }

    // WAV 出力
    if (wav::write("output_push_pull_power_stage.wav", output, kSampleRate))
        std::printf("=> output_push_pull_power_stage.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, kDurationSec);
    else
        std::printf("ERROR: failed to write WAV\n");

    return 0;
}
