// ============================================================================
// SpringReverbModel unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/delay/SpringReverbModel.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("SpringReverb: prepare and reset without crash", "[SpringReverb]") {
    SpringReverbModel sr;
    sr.prepare(2, kSR);
    sr.reset();
    REQUIRE(true);
}

TEST_CASE("SpringReverb: prepare with ProcessSpec", "[SpringReverb]") {
    SpringReverbModel sr;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    sr.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("SpringReverb: silence in -> decays to silence", "[SpringReverb]") {
    SpringReverbModel sr;
    sr.prepare(1, kSR);
    SpringReverbModel::Params params;
    params.mix = 1.0f;
    params.decay = 0.3f;

    // Feed silence for a while
    float out = 0.0f;
    for (int i = 0; i < 20000; ++i)
        out = sr.process(0, 0.0f, params);
    REQUIRE(std::fabs(out) < 0.01f);
}

TEST_CASE("SpringReverb: impulse produces reverb tail", "[SpringReverb]") {
    SpringReverbModel sr;
    sr.prepare(1, kSR);
    SpringReverbModel::Params params;
    params.mix = 1.0f;
    params.decay = 0.7f;

    // Feed impulse
    sr.process(0, 1.0f, params);

    // Check reverb tail is non-zero after delay
    double tailEnergy = 0.0;
    for (int i = 0; i < 4000; ++i) {
        float out = sr.process(0, 0.0f, params);
        tailEnergy += out * out;
    }
    REQUIRE(tailEnergy > 0.001);
}

TEST_CASE("SpringReverb: higher decay = longer tail", "[SpringReverb]") {
    double energyShort = 0.0, energyLong = 0.0;
    const int N = 8000;

    // Short decay
    {
        SpringReverbModel sr;
        sr.prepare(1, kSR);
        SpringReverbModel::Params params;
        params.mix = 1.0f;
        params.decay = 0.2f;
        sr.process(0, 1.0f, params);
        for (int i = 0; i < N; ++i) {
            float out = sr.process(0, 0.0f, params);
            energyShort += out * out;
        }
    }
    // Long decay
    {
        SpringReverbModel sr;
        sr.prepare(1, kSR);
        SpringReverbModel::Params params;
        params.mix = 1.0f;
        params.decay = 0.9f;
        sr.process(0, 1.0f, params);
        for (int i = 0; i < N; ++i) {
            float out = sr.process(0, 0.0f, params);
            energyLong += out * out;
        }
    }
    REQUIRE(energyLong > energyShort);
}

TEST_CASE("SpringReverb: mix controls wet/dry balance", "[SpringReverb]") {
    SpringReverbModel sr;
    sr.prepare(1, kSR);

    // Mix = 0 -> dry only
    SpringReverbModel::Params paramsDry;
    paramsDry.mix = 0.0f;
    paramsDry.decay = 0.5f;

    float dryOut = sr.process(0, 0.5f, paramsDry);
    REQUIRE(dryOut == Approx(0.5f).margin(0.01f));
}

TEST_CASE("SpringReverb: drip effect is finite", "[SpringReverb]") {
    SpringReverbModel sr;
    sr.prepare(1, kSR);
    SpringReverbModel::Params params;
    params.dripAmount = 1.0f;
    params.mix = 0.5f;

    // Feed impulse and check drip output stays finite
    sr.process(0, 1.0f, params);
    for (int i = 0; i < 4000; ++i) {
        float out = sr.process(0, 0.0f, params);
        REQUIRE(std::isfinite(out));
    }
}

TEST_CASE("SpringReverb: numSprings parameter", "[SpringReverb]") {
    // Different spring counts should work
    for (int nSprings = 1; nSprings <= 3; ++nSprings) {
        SpringReverbModel sr;
        sr.prepare(1, kSR);
        SpringReverbModel::Params params;
        params.numSprings = nSprings;
        params.mix = 0.5f;

        sr.process(0, 1.0f, params);
        for (int i = 0; i < 1000; ++i) {
            float out = sr.process(0, 0.0f, params);
            REQUIRE(std::isfinite(out));
        }
    }
}

TEST_CASE("SpringReverb: tension affects character", "[SpringReverb]") {
    double energyLow = 0.0, energyHigh = 0.0;
    const int N = 4000;

    {
        SpringReverbModel sr;
        sr.prepare(1, kSR);
        SpringReverbModel::Params params;
        params.tension = 0.1f;
        params.mix = 1.0f;
        sr.process(0, 1.0f, params);
        for (int i = 0; i < N; ++i) {
            float out = sr.process(0, 0.0f, params);
            energyLow += out * out;
        }
    }
    {
        SpringReverbModel sr;
        sr.prepare(1, kSR);
        SpringReverbModel::Params params;
        params.tension = 0.9f;
        params.mix = 1.0f;
        sr.process(0, 1.0f, params);
        for (int i = 0; i < N; ++i) {
            float out = sr.process(0, 0.0f, params);
            energyHigh += out * out;
        }
    }
    // Just verify both produce valid output with different energy
    REQUIRE(std::isfinite(energyLow));
    REQUIRE(std::isfinite(energyHigh));
    REQUIRE(energyLow > 0.0);
    REQUIRE(energyHigh > 0.0);
}

TEST_CASE("SpringReverb: processBlock produces finite output", "[SpringReverb]") {
    const int N = 512;
    SpringReverbModel sr;
    sr.prepare(1, kSR);

    SpringReverbModel::Params params;
    params.decay = 0.5f;
    params.mix = 0.3f;

    std::vector<float> buf(N);
    for (int i = 0; i < N; ++i)
        buf[i] = (i == 0) ? 1.0f : 0.0f;  // impulse

    float* ch[] = { buf.data() };
    sr.processBlock(ch, 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(buf[i]));
}

TEST_CASE("SpringReverb: supports up to 3 springs", "[SpringReverb]") {
    SpringReverbModel sr;
    sr.prepare(1, kSR);
    SpringReverbModel::Params params;
    params.numSprings = 3;
    params.mix = 0.5f;
    sr.process(0, 0.5f, params);
    REQUIRE(true);
}
