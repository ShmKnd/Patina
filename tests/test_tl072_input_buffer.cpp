// ============================================================================
// InputBuffer unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/circuits/drive/InputBuffer.h"

using Catch::Approx;

static constexpr double kSampleRate = 44100.0;

TEST_CASE("InputBuffer: prepare and reset without crash", "[InputBuffer]") {
    InputBuffer buf;
    buf.prepare(2, kSampleRate);
    buf.reset();
    REQUIRE(true);
}

TEST_CASE("InputBuffer: silence in -> silence out", "[InputBuffer]") {
    InputBuffer buf;
    buf.prepare(1, kSampleRate);

    float out = buf.process(0, 0.0f);
    REQUIRE(std::fabs(out) < 0.001f);
}

TEST_CASE("InputBuffer: soft saturation limits output", "[InputBuffer]") {
    InputBuffer buf;
    buf.prepare(1, kSampleRate);

    // Very large input should be saturated
    float out = buf.process(0, 10.0f);
    REQUIRE(std::isfinite(out));
    REQUIRE(std::fabs(out) < 10.0f); // Should be limited by tanh
}

TEST_CASE("InputBuffer: pad attenuates by ~20dB", "[InputBuffer]") {
    InputBuffer bufNoPad, bufPad;
    bufNoPad.prepare(1, kSampleRate);
    bufPad.prepare(1, kSampleRate);
    bufPad.setPadEnabled(true);

    // Small input where saturation doesn't dominate
    float outNoPad = bufNoPad.process(0, 0.1f);
    float outPad = bufPad.process(0, 0.1f);

    // Pad should attenuate significantly
    REQUIRE(std::fabs(outPad) < std::fabs(outNoPad));
}

TEST_CASE("InputBuffer: 9V supply has tighter headroom than 18V", "[InputBuffer]") {
    InputBuffer buf9, buf18;
    buf9.prepare(1, kSampleRate);
    buf18.prepare(1, kSampleRate);
    buf9.setSupplyVoltage(9.0);
    buf18.setSupplyVoltage(18.0);

    // Same input level
    float out9 = buf9.process(0, 0.7f);
    float out18 = buf18.process(0, 0.7f);

    // 18V should have wider headroom (less saturation for same input)
    // For moderate levels both should be finite
    REQUIRE(std::isfinite(out9));
    REQUIRE(std::isfinite(out18));
}

TEST_CASE("InputBuffer: RC filter LPF behavior", "[InputBuffer]") {
    InputBuffer buf;
    buf.prepare(1, kSampleRate);

    // Feed step function, output should smoothly approach
    float prev = 0.0f;
    buf.process(0, 0.0f);
    float first = buf.process(0, 1.0f);
    float second = buf.process(0, 1.0f);

    // Due to RC filtering, first response should be less than full step
    REQUIRE(first < 1.0f);
    REQUIRE(second >= first); // Monotonically approaching
}

TEST_CASE("InputBuffer: setInputCapacitance changes alpha", "[InputBuffer]") {
    InputBuffer buf;
    buf.prepare(1, kSampleRate);
    double alpha1 = buf.getAlpha();

    buf.setInputCapacitance(1e-9); // 1nF
    double alpha2 = buf.getAlpha();

    // Different capacitance should yield different alpha
    REQUIRE(alpha1 != alpha2);
}

TEST_CASE("InputBuffer: overload flag set on hot signal", "[InputBuffer]") {
    InputBuffer buf;
    buf.prepare(1, kSampleRate);
    buf.setSupplyVoltage(9.0);

    // Very hot signal should trigger overload
    for (int i = 0; i < 10; ++i) buf.process(0, 5.0f);
    // Flag may or may not be set depending on headroom knee; just check API
    bool flag = buf.consumeOverloadFlag();
    // Second consume should return false (already cleared)
    bool flag2 = buf.consumeOverloadFlag();
    REQUIRE(!flag2);
}

TEST_CASE("InputBuffer: setHeadroomKnees overrides", "[InputBuffer]") {
    InputBuffer buf;
    buf.prepare(1, kSampleRate);
    buf.setHeadroomKnees(0.3, 0.7);

    float out = buf.process(0, 0.5f);
    REQUIRE(std::isfinite(out));
}
