// ============================================================================
// VocoderBand unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/vocoder/VocoderBand.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

// ============================================================================
// prepare / reset
// ============================================================================

TEST_CASE("VocoderBand: prepare and reset without crash", "[VocoderBand]") {
    VocoderBand band;
    band.prepare(2, kSR);
    band.reset();
    REQUIRE(true);
}

TEST_CASE("VocoderBand: prepare with ProcessSpec", "[VocoderBand]") {
    VocoderBand band;
    patina::ProcessSpec spec;
    spec.sampleRate   = kSR;
    spec.numChannels  = 2;
    spec.maxBlockSize = 512;
    band.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("VocoderBand: prepare with custom Spec", "[VocoderBand]") {
    VocoderBand::Spec s;
    s.otaSpec = OTA_Primitive::CA3080();
    s.R_att   = 2e3;
    s.R_rel   = 100e3;
    VocoderBand band(s);
    band.prepare(2, kSR);
    REQUIRE(true);
}

// ============================================================================
// 無音入力 → 無音出力
// ============================================================================

TEST_CASE("VocoderBand: silence in -> silence out", "[VocoderBand]") {
    VocoderBand band;
    band.prepare(1, kSR);

    VocoderBand::Params p;
    p.centerHz = 1000.0f;
    p.q        = 4.0f;

    float out = 0.0f;
    for (int i = 0; i < 1000; ++i)
        out = band.process(0, 0.0f, 0.0f, p);
    REQUIRE(std::fabs(out) < 1e-6f);
}

// ============================================================================
// [FIX 3] リリース放電
// モジュレーター信号を止めた後、エンベロープが 0 に向かって減衰すること
// ============================================================================

TEST_CASE("VocoderBand: envelope decays after modulator stops", "[VocoderBand]") {
    VocoderBand band;
    band.prepare(1, kSR);

    VocoderBand::Params p;
    p.centerHz  = 1000.0f;
    p.q         = 4.0f;
    p.envAmount = 1.0f;

    // エンベロープを立ち上げる
    for (int i = 0; i < 2000; ++i) {
        float mod = 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
        band.process(0, mod, 0.5f, p);
    }
    double envAfterSignal = band.getEnvelope(0);
    REQUIRE(envAfterSignal > 0.01);

    // 無音でリリース区間を走らせる
    for (int i = 0; i < 5000; ++i)
        band.process(0, 0.0f, 0.5f, p);

    double envAfterRelease = band.getEnvelope(0);

    // FIX 前（releaseRC.processLPF(env)）ではほぼ変化しない
    // FIX 後（releaseRC.processLPF(0.0)）では半分以下に減衰
    REQUIRE(envAfterRelease < envAfterSignal * 0.5);
}

// ============================================================================
// BPF の帯域選択性
// 中心周波数付近のキャリアが遠い周波数より大きく通過すること
// ============================================================================

TEST_CASE("VocoderBand: BPF passes carrier near center frequency", "[VocoderBand]") {
    const int N = 8000, skip = 2000;

    auto runBand = [&](float carFreq) {
        VocoderBand band;
        band.prepare(1, kSR);
        VocoderBand::Params p;
        p.centerHz  = 1000.0f;
        p.q         = 8.0f;
        p.envAmount = 1.0f;
        double sum = 0.0;
        for (int i = 0; i < N; ++i) {
            float mod = 0.8f * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
            float car = 0.5f * std::sin(2.0 * M_PI * (double)carFreq * i / kSR);
            float out = band.process(0, mod, car, p);
            if (i >= skip) sum += out * out;
        }
        return sum;
    };

    double sumIn  = runBand(1000.0f);
    double sumFar = runBand(8000.0f);
    REQUIRE(sumIn > sumFar * 2.0);
}

// ============================================================================
// [FIX 2] 係数キャッシュ — params 変化で係数が更新されること
// ============================================================================

TEST_CASE("VocoderBand: coefficient updates when params change", "[VocoderBand]") {
    const int N = 2048;

    auto runBand = [&](float centerHz) {
        VocoderBand band;
        band.prepare(1, kSR);
        VocoderBand::Params p;
        p.centerHz  = centerHz;
        p.q         = 4.0f;
        p.envAmount = 1.0f;
        float out = 0.0f;
        for (int i = 0; i < N; ++i) {
            float mod = 0.5f * std::sin(2.0 * M_PI * (double)centerHz * i / kSR);
            float car = 0.5f * std::sin(2.0 * M_PI * (double)centerHz * i / kSR);
            out = band.process(0, mod, car, p);
        }
        return out;
    };

    float out500  = runBand(500.0f);
    float out4000 = runBand(4000.0f);
    REQUIRE(std::fabs(out500 - out4000) > 1e-4f);
}

// ============================================================================
// [FIX 1] processBlock のループ順
// processBlock の出力が per-sample process() と一致すること
// ============================================================================

TEST_CASE("VocoderBand: processBlock matches per-sample", "[VocoderBand]") {
    VocoderBand f1, f2;
    f1.prepare(2, kSR);
    f2.prepare(2, kSR);

    VocoderBand::Params p;
    p.centerHz  = 1000.0f;
    p.q         = 4.0f;
    p.envAmount = 1.0f;

    const int N = 256;
    std::vector<float> mod0(N), mod1(N), car0(N), car1(N);
    for (int i = 0; i < N; ++i) {
        mod0[i] = 0.4f * std::sin(2.0 * M_PI * 300.0 * i / kSR);
        mod1[i] = 0.35f * std::sin(2.0 * M_PI * 300.0 * i / kSR);
        car0[i] = 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
        car1[i] = 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
    }

    // per-sample
    std::vector<float> ref0(N), ref1(N);
    for (int i = 0; i < N; ++i) {
        ref0[i] = f1.process(0, mod0[i], car0[i], p);
        ref1[i] = f1.process(1, mod1[i], car1[i], p);
    }

    // processBlock
    std::vector<float> outB0(N), outB1(N);
    const float* modPtrs[] = { mod0.data(), mod1.data() };
    const float* carPtrs[] = { car0.data(), car1.data() };
    float* outPtrs[]       = { outB0.data(), outB1.data() };
    f2.processBlock(modPtrs, carPtrs, outPtrs, 2, N, p);

    for (int i = 0; i < N; ++i) {
        REQUIRE(outB0[i] == Approx(ref0[i]).margin(1e-6f));
        REQUIRE(outB1[i] == Approx(ref1[i]).margin(1e-6f));
    }
}

// ============================================================================
// 出力クリップ — 大入力でも ±1 を超えないこと
// ============================================================================

TEST_CASE("VocoderBand: output stays bounded", "[VocoderBand]") {
    VocoderBand band;
    band.prepare(1, kSR);

    VocoderBand::Params p;
    p.centerHz  = 1000.0f;
    p.q         = 4.0f;
    p.envAmount = 1.0f;

    float maxAbs = 0.0f;
    for (int i = 0; i < 4000; ++i) {
        float mod = 5.0f * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
        float car = 5.0f * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
        float out = band.process(0, mod, car, p);
        maxAbs = std::max(maxAbs, std::fabs(out));
    }
    REQUIRE(maxAbs <= 1.0f);
    REQUIRE(std::isfinite(maxAbs));
}

// ============================================================================
// attackMs / releaseMs アクセサ
// ============================================================================

TEST_CASE("VocoderBand: attackMs and releaseMs match spec", "[VocoderBand]") {
    VocoderBand::Spec s;
    s.R_att = 5e3;
    s.R_rel = 50e3;
    s.C_env = 1e-6;
    VocoderBand band(s);

    REQUIRE(band.attackMs()  == Approx(5.0).margin(0.01));
    REQUIRE(band.releaseMs() == Approx(50.0).margin(0.01));
}

// ============================================================================
// チャンネル独立性
// ============================================================================

TEST_CASE("VocoderBand: multichannel independence", "[VocoderBand]") {
    VocoderBand band;
    band.prepare(2, kSR);

    VocoderBand::Params p;
    p.centerHz  = 1000.0f;
    p.q         = 4.0f;
    p.envAmount = 1.0f;

    for (int i = 0; i < 4000; ++i) {
        float mod = 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
        band.process(0, mod,  0.5f, p);
        band.process(1, 0.0f, 0.5f, p);
    }

    double env0 = band.getEnvelope(0);
    double env1 = band.getEnvelope(1);
    REQUIRE(env0 > 0.01);
    REQUIRE(env1 < 0.001);
}

// ============================================================================
// 温度変化で gm がスケールされること
// ============================================================================

TEST_CASE("VocoderBand: temperature affects output level", "[VocoderBand]") {
    const int N = 8000, skip = 2000;

    auto runBand = [&](float temperature) {
        VocoderBand band;
        band.prepare(1, kSR);
        VocoderBand::Params p;
        p.centerHz    = 1000.0f;
        p.q           = 4.0f;
        p.envAmount   = 1.0f;
        p.temperature = temperature;
        double sum = 0.0;
        for (int i = 0; i < N; ++i) {
            float mod = 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
            float car = 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
            float out = band.process(0, mod, car, p);
            if (i >= skip) sum += out * out;
        }
        return sum;
    };

    double eCold = runBand(10.0f);
    double eHot  = runBand(80.0f);
    REQUIRE(std::fabs(eCold - eHot) > 0.001);
}

// ============================================================================
// getEnvelope が有効範囲を返すこと
// ============================================================================

TEST_CASE("VocoderBand: getEnvelope returns valid range", "[VocoderBand]") {
    VocoderBand band;
    band.prepare(1, kSR);

    VocoderBand::Params p;
    p.centerHz  = 1000.0f;
    p.q         = 4.0f;
    p.envAmount = 1.0f;

    REQUIRE(band.getEnvelope(0) == Approx(0.0).margin(1e-10));

    for (int i = 0; i < 4000; ++i) {
        float mod = 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
        band.process(0, mod, 0.5f, p);
    }

    double env = band.getEnvelope(0);
    REQUIRE(env >= 0.0);
    REQUIRE(env <= 1.0);
    REQUIRE(std::isfinite(env));
}
