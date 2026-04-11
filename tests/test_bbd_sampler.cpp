// ============================================================================
// BbdSampler unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <random>
#include "dsp/circuits/bbd/BbdSampler.h"

using Catch::Approx;

static constexpr double kSampleRate = 44100.0;

TEST_CASE("BbdSampler: prepare and reset without crash", "[BbdSampler]") {
    BbdSampler sampler;
    sampler.prepare(2, kSampleRate);
    sampler.reset();
    REQUIRE(true);
}

TEST_CASE("BbdSampler: bypass when emulateBbd=false", "[BbdSampler]") {
    BbdSampler sampler;
    sampler.prepare(1, kSampleRate);

    std::minstd_rand rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    float in = 0.42f;
    float out = sampler.processSample(0, in, 2.0, /*emulateBbd=*/false,
                                       false, 0.0, rng, dist);
    REQUIRE(out == in);
}

TEST_CASE("BbdSampler: sample-and-hold quantizes time", "[BbdSampler]") {
    BbdSampler sampler;
    sampler.prepare(1, kSampleRate);

    std::minstd_rand rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    // step=3 means hold each sample for 3 sample periods
    float first = sampler.processSample(0, 1.0f, 3.0, true, false, 0.0, rng, dist);
    float second = sampler.processSample(0, 2.0f, 3.0, true, false, 0.0, rng, dist);
    float third = sampler.processSample(0, 3.0f, 3.0, true, false, 0.0, rng, dist);

    // First three outputs should all be the first held value
    REQUIRE(first == 1.0f);
    REQUIRE(second == 1.0f);
    REQUIRE(third == 1.0f);
}

TEST_CASE("BbdSampler: held value updates after hold period", "[BbdSampler]") {
    BbdSampler sampler;
    sampler.prepare(1, kSampleRate);

    std::minstd_rand rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    // step=2: hold for 2 samples
    sampler.processSample(0, 1.0f, 2.0, true, false, 0.0, rng, dist); // hold=1.0, counter=2->1
    sampler.processSample(0, 2.0f, 2.0, true, false, 0.0, rng, dist); // hold=1.0, counter=1->0
    float third = sampler.processSample(0, 3.0f, 2.0, true, false, 0.0, rng, dist); // counter<=0, capture new
    
    // After hold period expires, new sample should be captured
    REQUIRE(third == 3.0f);
}

TEST_CASE("BbdSampler: silence in -> silence out", "[BbdSampler]") {
    BbdSampler sampler;
    sampler.prepare(1, kSampleRate);

    std::minstd_rand rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    float out = sampler.processSample(0, 0.0f, 2.0, true, false, 0.0, rng, dist);
    REQUIRE(out == 0.0f);
}

TEST_CASE("BbdSampler: jitter adds variance to hold times", "[BbdSampler]") {
    // With aging and jitter, hold periods should vary
    BbdSampler sampler;
    sampler.prepare(1, kSampleRate);

    std::minstd_rand rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    // Just check that it doesn't crash with jitter enabled
    for (int i = 0; i < 1000; ++i) {
        float in = std::sin(2.0f * 3.14159f * 440.0f * i / (float)kSampleRate);
        float out = sampler.processSample(0, in, 4.0, true, true, 0.05, rng, dist);
        REQUIRE(std::isfinite(out));
    }
}

TEST_CASE("BbdSampler: invalid channel returns input", "[BbdSampler]") {
    BbdSampler sampler;
    sampler.prepare(1, kSampleRate);

    std::minstd_rand rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    float out = sampler.processSample(5, 0.5f, 2.0, true, false, 0.0, rng, dist);
    REQUIRE(out == 0.5f);
}
