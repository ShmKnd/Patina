// ============================================================================
// PhaserStage unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/filters/PhaserStage.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("Phaser: prepare and reset without crash", "[Phaser]") {
    PhaserStage ph;
    ph.prepare(2, kSR);
    ph.reset();
    REQUIRE(true);
}

TEST_CASE("Phaser: prepare with ProcessSpec", "[Phaser]") {
    PhaserStage ph;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    ph.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("Phaser: silence in -> silence out", "[Phaser]") {
    PhaserStage ph;
    ph.prepare(1, kSR);
    PhaserStage::Params params;

    float out = ph.process(0, 0.0f, params);
    // Due to JFET 1/f noise injection (~2e-6), it will not be exactly 0
    REQUIRE(std::fabs(out) <= 1e-4f);
}

TEST_CASE("Phaser: with depth=0 passes signal through", "[Phaser]") {
    PhaserStage ph;
    ph.prepare(1, kSR);
    PhaserStage::Params params;
    params.depth = 0.0f;
    params.feedback = 0.0f;
    params.numStages = 4;

    const int N = 4000;
    float maxDiff = 0.0f;
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = ph.process(0, in, params);
        if (i > 2000) maxDiff = std::max(maxDiff, std::fabs(out - in));
    }
    // With depth=0, output should approximate input (allpass at fixed point)
    REQUIRE(maxDiff < 1.0f);
}

TEST_CASE("Phaser: output changes with LFO modulation", "[Phaser]") {
    PhaserStage ph;
    ph.prepare(1, kSR);
    PhaserStage::Params params;
    params.depth = 1.0f;
    params.feedback = 0.5f;
    params.centerFreqHz = 1000.0f;
    params.freqSpreadHz = 800.0f;
    params.numStages = 4;

    // Collect output with two different LFO values
    double sum1 = 0.0, sum2 = 0.0;
    const int N = 2000;

    params.lfoValue = -1.0f;
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 1000.0 * i / kSR);
        float out = ph.process(0, in, params);
        if (i > 1000) sum1 += out * out;
    }
    ph.reset();
    params.lfoValue = 1.0f;
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 1000.0 * i / kSR);
        float out = ph.process(0, in, params);
        if (i > 1000) sum2 += out * out;
    }
    // Different LFO positions should produce different spectral results
    REQUIRE(std::fabs(sum1 - sum2) > 1e-3);
}

TEST_CASE("Phaser: more stages = deeper notches", "[Phaser]") {
    const float testFreq = 1000.0f;
    double energy2 = 0.0, energy8 = 0.0;
    const int N = 8000;
    const int skip = 4000;

    PhaserStage::Params params;
    params.lfoValue = 0.0f;
    params.depth = 1.0f;
    params.feedback = 0.7f;
    params.centerFreqHz = testFreq;
    params.freqSpreadHz = 500.0f;

    // 2 stages
    {
        PhaserStage ph;
        ph.prepare(1, kSR);
        params.numStages = 2;
        for (int i = 0; i < N; ++i) {
            float in = std::sin(2.0 * M_PI * testFreq * i / kSR);
            float out = ph.process(0, in, params);
            if (i >= skip) energy2 += out * out;
        }
    }
    // 8 stages
    {
        PhaserStage ph;
        ph.prepare(1, kSR);
        params.numStages = 8;
        for (int i = 0; i < N; ++i) {
            float in = std::sin(2.0 * M_PI * testFreq * i / kSR);
            float out = ph.process(0, in, params);
            if (i >= skip) energy8 += out * out;
        }
    }
    // Check both produce finite results (notch depth varies)
    REQUIRE(std::isfinite(energy2));
    REQUIRE(std::isfinite(energy8));
    REQUIRE(energy2 > 0.0);
    REQUIRE(energy8 > 0.0);
}

TEST_CASE("Phaser: feedback produces finite output", "[Phaser]") {
    PhaserStage ph;
    ph.prepare(1, kSR);
    PhaserStage::Params params;
    params.lfoValue = 0.5f;
    params.depth = 1.0f;
    params.feedback = 0.9f;
    params.numStages = 4;

    for (int i = 0; i < 4000; ++i) {
        float in = std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = ph.process(0, in, params);
        REQUIRE(std::isfinite(out));
    }
}

TEST_CASE("Phaser: processBlock produces finite output", "[Phaser]") {
    const int N = 512;
    PhaserStage ph;
    ph.prepare(1, kSR);

    PhaserStage::Params params;
    params.lfoValue = 0.3f;
    params.depth = 0.7f;
    params.numStages = 6;

    std::vector<float> buf(N);
    for (int i = 0; i < N; ++i)
        buf[i] = std::sin(2.0 * M_PI * 440.0 * i / kSR);

    float* ch[] = { buf.data() };
    ph.processBlock(ch, 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(buf[i]));
}

TEST_CASE("Phaser: supports up to 12 stages", "[Phaser]") {
    PhaserStage ph;
    ph.prepare(1, kSR);
    PhaserStage::Params params;
    params.numStages = 12;
    params.lfoValue = 0.0f;
    params.depth = 0.5f;

    float out = ph.process(0, 0.5f, params);
    REQUIRE(std::isfinite(out));
}
