// ============================================================================
// TremoloVCA unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/compander/TremoloVCA.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("TremoloVCA: prepare and reset without crash", "[Tremolo]") {
    TremoloVCA tv;
    tv.prepare(2, kSR);
    tv.reset();
    REQUIRE(true);
}

TEST_CASE("TremoloVCA: prepare with ProcessSpec", "[Tremolo]") {
    TremoloVCA tv;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    tv.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("TremoloVCA: silence in -> silence out", "[Tremolo]") {
    TremoloVCA tv;
    tv.prepare(1, kSR);
    TremoloVCA::Params params;

    float out = tv.process(0, 0.0f, params);
    REQUIRE(std::fabs(out) < 1e-6f);
}

TEST_CASE("TremoloVCA: depth=0 passes signal through", "[Tremolo]") {
    TremoloVCA tv;
    tv.prepare(1, kSR);
    TremoloVCA::Params params;
    params.depth = 0.0f;
    params.lfoValue = 0.5f;

    const int N = 4000;
    float maxDiff = 0.0f;
    for (int i = 0; i < N; ++i) {
        float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = tv.process(0, in, params);
        if (i > 2000) maxDiff = std::max(maxDiff, std::fabs(out - in));
    }
    REQUIRE(maxDiff < 0.01f);
}

TEST_CASE("TremoloVCA: LFO modulates amplitude", "[Tremolo]") {
    TremoloVCA tv;
    tv.prepare(1, kSR);
    TremoloVCA::Params params;
    params.depth = 1.0f;
    params.mode = (int)TremoloVCA::Mode::VCA;

    // LFO at minimum (should attenuate)
    params.lfoValue = -1.0f;
    float outMin = tv.process(0, 1.0f, params);

    tv.reset();

    // LFO at maximum (should pass)
    params.lfoValue = 1.0f;
    float outMax = tv.process(0, 1.0f, params);

    REQUIRE(std::fabs(outMax) > std::fabs(outMin));
}

TEST_CASE("TremoloVCA: Bias mode works", "[Tremolo]") {
    TremoloVCA tv;
    tv.prepare(1, kSR);
    TremoloVCA::Params params;
    params.depth = 0.8f;
    params.mode = (int)TremoloVCA::Mode::Bias;

    for (int i = 0; i < 4000; ++i) {
        params.lfoValue = std::sin(2.0 * M_PI * 5.0 * i / kSR);
        float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = tv.process(0, in, params);
        REQUIRE(std::isfinite(out));
    }
}

TEST_CASE("TremoloVCA: Optical mode works", "[Tremolo]") {
    TremoloVCA tv;
    tv.prepare(1, kSR);
    TremoloVCA::Params params;
    params.depth = 0.8f;
    params.mode = (int)TremoloVCA::Mode::Optical;

    for (int i = 0; i < 4000; ++i) {
        params.lfoValue = std::sin(2.0 * M_PI * 5.0 * i / kSR);
        float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = tv.process(0, in, params);
        REQUIRE(std::isfinite(out));
    }
}

TEST_CASE("TremoloVCA: VCA mode works", "[Tremolo]") {
    TremoloVCA tv;
    tv.prepare(1, kSR);
    TremoloVCA::Params params;
    params.depth = 0.8f;
    params.mode = (int)TremoloVCA::Mode::VCA;

    for (int i = 0; i < 4000; ++i) {
        params.lfoValue = std::sin(2.0 * M_PI * 5.0 * i / kSR);
        float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = tv.process(0, in, params);
        REQUIRE(std::isfinite(out));
    }
}

TEST_CASE("TremoloVCA: stereo phase invert", "[Tremolo]") {
    TremoloVCA tv;
    tv.prepare(2, kSR);
    TremoloVCA::Params params;
    params.depth = 1.0f;
    params.lfoValue = 0.5f;
    params.mode = (int)TremoloVCA::Mode::VCA;
    params.stereoPhaseInvert = true;

    float outL = tv.process(0, 1.0f, params);
    float outR = tv.process(1, 1.0f, params);

    // With stereo invert, left and right should get different modulation
    // (exact relationship depends on implementation)
    REQUIRE(std::isfinite(outL));
    REQUIRE(std::isfinite(outR));
}

TEST_CASE("TremoloVCA: processBlock produces finite output", "[Tremolo]") {
    const int N = 512;
    TremoloVCA tv;
    tv.prepare(1, kSR);

    TremoloVCA::Params params;
    params.depth = 0.7f;
    params.lfoValue = 0.3f;
    params.mode = (int)TremoloVCA::Mode::Bias;

    std::vector<float> buf(N);
    for (int i = 0; i < N; ++i)
        buf[i] = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);

    float* ch[] = { buf.data() };
    tv.processBlock(ch, 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(buf[i]));
}

TEST_CASE("TremoloVCA: output finite for extreme inputs", "[Tremolo]") {
    TremoloVCA tv;
    tv.prepare(1, kSR);
    TremoloVCA::Params params;
    params.depth = 1.0f;
    params.lfoValue = 1.0f;

    for (int i = 0; i < 200; ++i) {
        float out = tv.process(0, 10.0f, params);
        REQUIRE(std::isfinite(out));
    }
}
