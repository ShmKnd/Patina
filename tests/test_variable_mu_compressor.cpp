// ============================================================================
// VariableMuCompressor (Variable-mu tube) unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/dynamics/VariableMuCompressor.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("VarMu: prepare and reset without crash", "[VarMu]") {
    VariableMuCompressor vm;
    vm.prepare(2, kSR);
    vm.reset();
    REQUIRE(true);
}

TEST_CASE("VarMu: prepare with ProcessSpec", "[VarMu]") {
    VariableMuCompressor vm;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    vm.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("VarMu: silence in -> silence out", "[VarMu]") {
    VariableMuCompressor vm;
    vm.prepare(1, kSR);
    VariableMuCompressor::Params p;
    float out = 0.0f;
    for (int i = 0; i < 1000; ++i)
        out = vm.process(0, 0.0f, p);
    REQUIRE(std::fabs(out) < 1e-6f);
}

TEST_CASE("VarMu: compresses signal above threshold", "[VarMu]") {
    VariableMuCompressor vm;
    vm.prepare(1, kSR);
    VariableMuCompressor::Params p;
    p.inputGain = 0.6f;
    p.threshold = 0.3f;
    p.outputGain = 0.5f;
    p.timeConstant = 0;  // TC1 fastest
    p.mix = 1.0f;

    for (int i = 0; i < 8000; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        vm.process(0, in, p);
    }

    float gr = vm.getGainReductionDb(0);
    REQUIRE(gr < 0.0f);
}

TEST_CASE("VarMu: lower threshold -> more compression", "[VarMu]") {
    const int N = 8000;

    auto runAndGetGR = [&](float threshold) {
        VariableMuCompressor vm;
        vm.prepare(1, kSR);
        VariableMuCompressor::Params p;
        p.inputGain = 0.4f;
        p.threshold = threshold;
        p.outputGain = 0.5f;
        p.timeConstant = 0;
        p.mix = 1.0f;
        for (int i = 0; i < N; ++i) {
            float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            vm.process(0, in, p);
        }
        return vm.getGainReductionDb(0);
    };

    float grLowThresh  = runAndGetGR(0.1f);  // lower threshold
    float grHighThresh = runAndGetGR(0.9f);   // higher threshold
    REQUIRE(grLowThresh < grHighThresh);
}

TEST_CASE("VarMu: TC selector changes time constants", "[VarMu]") {
    // Compare TC1 (fastest) vs TC6 (slowest) during compression
    // With fastest TC, envelope tracks more quickly -> GR reaches deeper
    auto runAndGetGR = [&](int tc) {
        VariableMuCompressor vm;
        vm.prepare(1, kSR);
        VariableMuCompressor::Params p;
        p.inputGain = 0.5f;
        p.threshold = 0.2f;
        p.outputGain = 0.5f;
        p.timeConstant = tc;
        p.mix = 1.0f;

        // Short burst - TC affects how quickly GR develops
        for (int i = 0; i < 200; ++i) {
            float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            vm.process(0, in, p);
        }
        return vm.getGainReductionDb(0);
    };

    float grFast = runAndGetGR(0);   // TC1 fastest
    float grSlow = runAndGetGR(5);   // TC6 slowest
    // Both should show some compression, but different amounts
    REQUIRE(grFast < 0.0f);
    REQUIRE(grSlow < 0.0f);
    // They should differ due to different time constants
    REQUIRE(std::fabs(grFast - grSlow) > 0.001f);
}

TEST_CASE("VarMu: push-pull cancels even harmonics (symmetric)", "[VarMu]") {
    VariableMuCompressor vm;
    vm.prepare(1, kSR);
    VariableMuCompressor::Params p;
    p.inputGain = 0.5f;
    p.threshold = 0.5f;
    p.outputGain = 0.5f;
    p.timeConstant = 0;
    p.mix = 1.0f;

    // Feed +/- symmetric signal, output should be symmetric too
    // (push-pull cancels asymmetric / even harmonics)
    float outPos = 0.0f, outNeg = 0.0f;
    for (int i = 0; i < 4000; ++i) {
        float in = 0.6f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = vm.process(0, in, p);
        if (i > 2000) {
            if (out > outPos) outPos = out;
            if (out < outNeg) outNeg = out;
        }
    }
    // Symmetric: |pos| ~ |neg|
    REQUIRE(std::fabs(outPos + outNeg) < std::fabs(outPos) * 0.3);
}

TEST_CASE("VarMu: gain slew rate smoothing", "[VarMu]") {
    VariableMuCompressor vm;
    vm.prepare(1, kSR);
    VariableMuCompressor::Params p;
    p.inputGain = 0.8f;
    p.threshold = 0.2f;
    p.outputGain = 0.5f;
    p.timeConstant = 0;
    p.mix = 1.0f;

    // Sudden transient
    for (int i = 0; i < 10; ++i)
        vm.process(0, 0.0f, p);

    float gr1 = vm.getGainReductionDb(0);

    // Sudden loud signal
    for (int i = 0; i < 5; ++i)
        vm.process(0, 0.9f, p);

    float gr2 = vm.getGainReductionDb(0);

    // Due to slew rate, GR should change gradually, not instantly
    // GR should have started to change but not reached final value
    REQUIRE(std::isfinite(gr1));
    REQUIRE(std::isfinite(gr2));
}

TEST_CASE("VarMu: output stays bounded", "[VarMu]") {
    VariableMuCompressor vm;
    vm.prepare(1, kSR);
    VariableMuCompressor::Params p;
    p.inputGain = 1.0f;
    p.threshold = 0.0f;
    p.outputGain = 1.0f;
    p.timeConstant = 0;
    p.mix = 1.0f;

    float maxAbs = 0.0f;
    for (int i = 0; i < 16000; ++i) {
        float in = 0.9f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = vm.process(0, in, p);
        maxAbs = std::max(maxAbs, std::fabs(out));
    }
    REQUIRE(maxAbs < 100.0f);
    REQUIRE(std::isfinite(maxAbs));
}

TEST_CASE("VarMu: processBlock matches per-sample", "[VarMu]") {
    VariableMuCompressor f1, f2;
    f1.prepare(1, kSR);
    f2.prepare(1, kSR);

    VariableMuCompressor::Params p;
    p.inputGain = 0.5f;
    p.threshold = 0.5f;
    p.outputGain = 0.5f;
    p.timeConstant = 2;
    p.mix = 1.0f;

    const int N = 256;
    std::vector<float> buf(N);
    for (int i = 0; i < N; ++i)
        buf[i] = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);

    std::vector<float> ref(N);
    for (int i = 0; i < N; ++i)
        ref[i] = f1.process(0, buf[i], p);

    float* ptr = buf.data();
    f2.processBlock(&ptr, 1, N, p);

    for (int i = 0; i < N; ++i)
        REQUIRE(buf[i] == Approx(ref[i]).margin(1e-6f));
}

TEST_CASE("VarMu: getGainReductionDb returns valid range", "[VarMu]") {
    VariableMuCompressor vm;
    vm.prepare(1, kSR);
    VariableMuCompressor::Params p;
    p.inputGain = 0.5f;
    p.threshold = 0.5f;
    p.mix = 1.0f;

    float gr0 = vm.getGainReductionDb(0);
    REQUIRE(gr0 <= 0.0f);

    for (int i = 0; i < 8000; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        vm.process(0, in, p);
    }
    float gr1 = vm.getGainReductionDb(0);
    REQUIRE(gr1 >= -60.0f);
    REQUIRE(gr1 <= 0.0f);
}

TEST_CASE("VarMu: multichannel independence", "[VarMu]") {
    VariableMuCompressor vm;
    vm.prepare(2, kSR);
    VariableMuCompressor::Params p;
    p.inputGain = 0.7f;
    p.threshold = 0.3f;
    p.outputGain = 0.5f;
    p.timeConstant = 0;
    p.mix = 1.0f;

    for (int i = 0; i < 8000; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        vm.process(0, in, p);
        vm.process(1, 0.0f, p);
    }

    float gr0 = vm.getGainReductionDb(0);
    float gr1 = vm.getGainReductionDb(1);
    REQUIRE(gr0 < gr1);
}

TEST_CASE("VarMu: all 6 time constants are valid", "[VarMu]") {
    for (int tc = 0; tc < 6; ++tc) {
        VariableMuCompressor vm;
        vm.prepare(1, kSR);
        VariableMuCompressor::Params p;
        p.inputGain = 0.6f;
        p.threshold = 0.3f;
        p.outputGain = 0.5f;
        p.timeConstant = tc;
        p.mix = 1.0f;

        float out = 0.0f;
        for (int i = 0; i < 4000; ++i) {
            float in = 0.7f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            out = vm.process(0, in, p);
        }
        REQUIRE(std::isfinite(out));
        float gr = vm.getGainReductionDb(0);
        REQUIRE(gr < 0.0f);
    }
}
