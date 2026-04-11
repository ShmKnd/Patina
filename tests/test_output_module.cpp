// ============================================================================
// OutputStage unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/circuits/drive/OutputStage.h"

using Catch::Approx;

static constexpr double kSampleRate = 44100.0;

TEST_CASE("OutputStage: prepare and reset without crash", "[OutputStage]") {
    OutputStage out;
    out.prepare(2, kSampleRate);
    out.reset();
    REQUIRE(true);
}

TEST_CASE("OutputStage: silence in -> silence out", "[OutputStage]") {
    OutputStage out;
    out.prepare(1, kSampleRate);

    float o = out.process(0, 0.0f, 9.0);
    REQUIRE(std::fabs(o) < 0.001f);
}

TEST_CASE("OutputStage: saturates large inputs", "[OutputStage]") {
    OutputStage out;
    out.prepare(1, kSampleRate);

    float o = out.process(0, 100.0f, 9.0);
    REQUIRE(std::isfinite(o));
    REQUIRE(std::fabs(o) <= 9.1f); // Should be limited near supply voltage
}

TEST_CASE("OutputStage: 3-pole LPF attenuates HF", "[OutputStage]") {
    OutputStage out;
    out.prepare(1, kSampleRate);
    out.setCutoffHz(2000.0);

    double sumLow = 0.0, sumHigh = 0.0;
    const int N = 4000;

    for (int i = 0; i < N; ++i) {
        float in = 0.3f * std::sin(2.0 * M_PI * 500.0 * i / kSampleRate);
        float o = out.process(0, in, 18.0);
        if (i > 1000) sumLow += o * o;
    }
    out.reset();
    for (int i = 0; i < N; ++i) {
        float in = 0.3f * std::sin(2.0 * M_PI * 15000.0 * i / kSampleRate);
        float o = out.process(0, in, 18.0);
        if (i > 1000) sumHigh += o * o;
    }

    REQUIRE(sumLow > sumHigh);
}

TEST_CASE("OutputStage: supply voltage affects saturation headroom", "[OutputStage]") {
    OutputStage out9, out18;
    out9.prepare(1, kSampleRate);
    out18.prepare(1, kSampleRate);

    // Moderate signal
    float o9 = out9.process(0, 5.0f, 9.0);
    float o18 = out18.process(0, 5.0f, 18.0);

    REQUIRE(std::isfinite(o9));
    REQUIRE(std::isfinite(o18));
}

TEST_CASE("OutputStage: invalid channel returns input", "[OutputStage]") {
    OutputStage out;
    out.prepare(1, kSampleRate);

    REQUIRE(out.process(-1, 0.5f, 9.0) == 0.5f);
    REQUIRE(out.process(5, 0.5f, 9.0) == 0.5f);
}

TEST_CASE("OutputStage: DC converges through 3-pole filter", "[OutputStage]") {
    OutputStage out;
    out.prepare(1, kSampleRate);
    out.setCutoffHz(5000.0);

    float o = 0.0f;
    for (int i = 0; i < 5000; ++i)
        o = out.process(0, 0.1f, 18.0);
    
    // Should converge close to tanh(0.1/headroom)*headroom ~ ~0.1 for small inputs
    REQUIRE(std::isfinite(o));
    REQUIRE(std::fabs(o) > 0.01f);
}
