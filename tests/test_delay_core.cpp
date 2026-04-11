// ============================================================================
// BbdClock unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/circuits/bbd/BbdClock.h"

using Catch::Approx;

TEST_CASE("BbdClock: effectiveDelaySamples minimum is 1.5", "[BbdClock]") {
    double d = BbdClock::effectiveDelaySamples(0.5, 0.0, 0.0);
    REQUIRE(d >= 1.5);
}

TEST_CASE("BbdClock: effectiveDelaySamples with no chorus is base", "[BbdClock]") {
    double d = BbdClock::effectiveDelaySamples(100.0, 0.0, 0.0);
    REQUIRE(d == Approx(100.0));
}

TEST_CASE("BbdClock: effectiveDelaySamples with chorus modulates", "[BbdClock]") {
    double d1 = BbdClock::effectiveDelaySamples(100.0, 10.0, 1.0);
    double d2 = BbdClock::effectiveDelaySamples(100.0, 10.0, -1.0);
    REQUIRE(d1 == Approx(110.0));
    REQUIRE(d2 == Approx(90.0));
}

TEST_CASE("BbdClock: stepWithClock returns base when chorusEff=0", "[BbdClock]") {
    double s = BbdClock::stepWithClock(2.0, 0.5, 0.0f, 0.6);
    REQUIRE(s == Approx(2.0));
}

TEST_CASE("BbdClock: stepWithClock modulates with chorus", "[BbdClock]") {
    double s1 = BbdClock::stepWithClock(2.0, 1.0, 1.0f, 0.6);
    double s2 = BbdClock::stepWithClock(2.0, -1.0, 1.0f, 0.6);
    // With different triVal signs, steps should differ
    REQUIRE(s1 != Approx(s2));
}

TEST_CASE("BbdClock: stepWithClock minimum step is 1.0", "[BbdClock]") {
    double s = BbdClock::stepWithClock(0.5, 1.0, 1.0f, 0.6);
    REQUIRE(s >= 1.0);
}

TEST_CASE("BbdClock: stepWithClock clamped to 0.4-1.6 range", "[BbdClock]") {
    // Even with extreme values, clockScale is clamped
    double s = BbdClock::stepWithClock(4.0, 1.0, 1.0f, 1.0);
    REQUIRE(std::isfinite(s));
    REQUIRE(s >= 1.0);
}

TEST_CASE("BbdClock: chorusDepthSamples zero chorus = zero depth", "[BbdClock]") {
    double d = BbdClock::chorusDepthSamples(0.0, 100.0, 44100.0, 5.0);
    REQUIRE(d == 0.0);
}

TEST_CASE("BbdClock: chorusDepthSamples fraction cap prevents over-modulation", "[BbdClock]") {
    // With cap at 45%, depth cannot exceed 45% of delay time
    double timeMs = 10.0;
    double depthMax = 100.0; // Way too large
    double d = BbdClock::chorusDepthSamples(1.0, timeMs, 44100.0, depthMax, 0.45);
    double maxAllowed = timeMs * 0.45 * 0.001 * 44100.0;
    REQUIRE(d <= maxAllowed + 0.001);
}

TEST_CASE("BbdClock: chorusDepthSamplesDynamic adapts to time", "[BbdClock]") {
    // Short delay should have shallower depth
    double dShort = BbdClock::chorusDepthSamplesDynamic(1.0, 20.0, 0.0, 44100.0, 5.0);
    double dLong = BbdClock::chorusDepthSamplesDynamic(1.0, 500.0, 0.0, 44100.0, 5.0);
    REQUIRE(dShort < dLong);
}

TEST_CASE("BbdClock: chorusDepthSamplesDynamic reduces with high feedback", "[BbdClock]") {
    double dLowFb = BbdClock::chorusDepthSamplesDynamic(1.0, 200.0, 0.0, 44100.0, 5.0);
    double dHighFb = BbdClock::chorusDepthSamplesDynamic(1.0, 200.0, 0.9, 44100.0, 5.0);
    REQUIRE(dHighFb <= dLowFb);
}

TEST_CASE("BbdClock: chorusDepthSamplesDynamic zero chorus = zero", "[BbdClock]") {
    double d = BbdClock::chorusDepthSamplesDynamic(0.0, 200.0, 0.5, 44100.0, 5.0);
    REQUIRE(d == 0.0);
}
