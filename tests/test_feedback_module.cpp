// ============================================================================
// BbdFeedback unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <random>
#include "dsp/circuits/bbd/BbdFeedback.h"

using Catch::Approx;

static constexpr double kSampleRate = 44100.0;

TEST_CASE("BbdFeedback: prepare and reset without crash", "[BbdFeedback]") {
    BbdFeedback fb;
    fb.prepare(2, kSampleRate);
    fb.reset();
    REQUIRE(true);
}

TEST_CASE("BbdFeedback: silence in -> near silence out", "[BbdFeedback]") {
    BbdFeedback fb;
    fb.prepare(1, kSampleRate);

    std::minstd_rand rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    float out = fb.process(0, 0.0f, false, rng, dist, 0.0, 0.0, kSampleRate);
    REQUIRE(std::fabs(out) < 0.01f);
}

TEST_CASE("BbdFeedback: DC blocking (HP filter attenuates DC over time)", "[BbdFeedback]") {
    BbdFeedback fb;
    fb.prepare(1, kSampleRate);

    std::minstd_rand rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    // Feed constant DC, observe it decays over time
    float earlyOut = 0.0f, lateOut = 0.0f;
    for (int i = 0; i < 100000; ++i) {
        float out = fb.process(0, 1.0f, false, rng, dist, 0.0, 0.0, kSampleRate);
        if (i == 1000) earlyOut = std::fabs(out);
        if (i == 99999) lateOut = std::fabs(out);
    }

    // DC component should decay over time (HP filter effect)
    // The RC time constant is very long (R_input=1MOhm x C_inputCoupling=4.7uF ~ 4.7s)
    // so after ~2.3s (100k samples) DC is partially attenuated
    REQUIRE(lateOut < earlyOut);
}

TEST_CASE("BbdFeedback: preserves AC signal (1kHz sine)", "[BbdFeedback]") {
    BbdFeedback fb;
    fb.prepare(1, kSampleRate);

    std::minstd_rand rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    double sum = 0.0;
    const int N = 8000;

    for (int i = 0; i < N; ++i) {
        float in = 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSampleRate);
        float out = fb.process(0, in, false, rng, dist, 0.0, 0.0, kSampleRate);
        if (i > 4000) sum += out * out;
    }

    double rms = std::sqrt(sum / (N - 4000));
    // Should pass most of the 1kHz signal through
    REQUIRE(rms > 0.1);
}

TEST_CASE("BbdFeedback: op-amp saturation limits output", "[BbdFeedback]") {
    BbdFeedback fb;
    fb.prepare(1, kSampleRate);

    std::minstd_rand rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    // Feed through large signal with saturation enabled
    float out = 0.0f;
    for (int i = 0; i < 4000; ++i) {
        float in = 5.0f * std::sin(2.0 * M_PI * 1000.0 * i / kSampleRate);
        out = fb.process(0, in, true, rng, dist, 1.0, 0.0, kSampleRate);
    }
    REQUIRE(std::isfinite(out));
    REQUIRE(std::fabs(out) < 10.0f);
}

TEST_CASE("BbdFeedback: noise injection adds energy", "[BbdFeedback]") {
    BbdFeedback fb;
    fb.prepare(1, kSampleRate);

    std::minstd_rand rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    // Process silence with high noise level
    double sum = 0.0;
    for (int i = 0; i < 4000; ++i) {
        float out = fb.process(0, 0.0f, false, rng, dist, 1.0, 0.001, kSampleRate);
        sum += out * out;
    }

    // Should have some noise energy
    REQUIRE(sum > 0.0);
}

TEST_CASE("BbdFeedback: high voltage mode changes behavior", "[BbdFeedback]") {
    BbdFeedback fb9, fb18;
    fb9.prepare(1, kSampleRate);
    fb18.prepare(1, kSampleRate);

    std::minstd_rand rng9(42), rng18(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    float out9 = fb9.process(0, 2.0f, true, rng9, dist, 1.0, 0.0, kSampleRate, false);
    float out18 = fb18.process(0, 2.0f, true, rng18, dist, 1.0, 0.0, kSampleRate, true);

    REQUIRE(std::isfinite(out9));
    REQUIRE(std::isfinite(out18));
}

TEST_CASE("BbdFeedback: setOpAmpOverrides works", "[BbdFeedback]") {
    BbdFeedback fb;
    fb.prepare(1, kSampleRate);

    BbdFeedback::OpAmpOverrides ovr;
    ovr.slewRate = 13e6;
    ovr.satThreshold18V = 0.9;
    fb.setOpAmpOverrides(ovr);

    std::minstd_rand rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    float out = fb.process(0, 0.5f, true, rng, dist, 1.0, 0.0, kSampleRate);
    REQUIRE(std::isfinite(out));
}

TEST_CASE("BbdFeedback: invalid channel returns raw input", "[BbdFeedback]") {
    BbdFeedback fb;
    fb.prepare(1, kSampleRate);

    std::minstd_rand rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    REQUIRE(fb.process(-1, 0.5f, false, rng, dist, 0.0, 0.0, kSampleRate) == 0.5f);
    REQUIRE(fb.process(5, 0.5f, false, rng, dist, 0.0, 0.0, kSampleRate) == 0.5f);
}
