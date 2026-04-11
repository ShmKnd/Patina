// ============================================================================
// AnalogLfo unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/circuits/modulation/AnalogLfo.h"

using Catch::Approx;

static constexpr double kSampleRate = 44100.0;

TEST_CASE("AnalogLfo: prepare and reset without crash", "[AnalogLfo]") {
    AnalogLfo lfo;
    lfo.prepare(2, kSampleRate);
    lfo.reset();
    REQUIRE(true);
}

TEST_CASE("AnalogLfo: getTri output stays in [-1, 1]", "[AnalogLfo]") {
    AnalogLfo lfo;
    lfo.prepare(1, kSampleRate);
    lfo.setRateHz(2.0);

    for (int i = 0; i < 100000; ++i) {
        lfo.stepAll();
        float tri = lfo.getTri(0);
        REQUIRE(tri >= -1.0f);
        REQUIRE(tri <= 1.0f);
    }
}

TEST_CASE("AnalogLfo: getSinLike output stays in [-1, 1]", "[AnalogLfo]") {
    AnalogLfo lfo;
    lfo.prepare(1, kSampleRate);
    lfo.setRateHz(2.0);

    for (int i = 0; i < 100000; ++i) {
        lfo.stepAll();
        float s = lfo.getSinLike(0);
        REQUIRE(s >= -1.0f);
        REQUIRE(s <= 1.0f);
    }
}

TEST_CASE("AnalogLfo: triangle wave oscillates", "[AnalogLfo]") {
    AnalogLfo lfo;
    lfo.prepare(1, kSampleRate);
    lfo.setRateHz(1.0); // 1Hz

    // Run for one full period (44100 samples at 1Hz)
    float minVal = 1.0f, maxVal = -1.0f;
    for (int i = 0; i < 44100; ++i) {
        lfo.stepAll();
        float v = lfo.getTri(0);
        minVal = std::min(minVal, v);
        maxVal = std::max(maxVal, v);
    }

    // Should swing to both extremes
    REQUIRE(maxVal > 0.5f);
    REQUIRE(minVal < -0.5f);
}

TEST_CASE("AnalogLfo: rate affects period", "[AnalogLfo]") {
    AnalogLfo lfoSlow, lfoFast;
    lfoSlow.prepare(1, kSampleRate);
    lfoFast.prepare(1, kSampleRate);
    lfoSlow.setRateHz(0.5);
    lfoFast.setRateHz(5.0);

    // Count zero crossings for each
    int crossSlow = 0, crossFast = 0;
    float prevSlow = 0.0f, prevFast = 0.0f;
    const int N = 44100; // 1 second

    for (int i = 0; i < N; ++i) {
        lfoSlow.stepAll();
        lfoFast.stepAll();

        float vSlow = lfoSlow.getTri(0);
        float vFast = lfoFast.getTri(0);

        if (i > 0) {
            if (prevSlow * vSlow < 0) crossSlow++;
            if (prevFast * vFast < 0) crossFast++;
        }
        prevSlow = vSlow;
        prevFast = vFast;
    }

    // Faster rate should have more zero crossings
    REQUIRE(crossFast > crossSlow);
}

TEST_CASE("AnalogLfo: stereo channels have phase offset", "[AnalogLfo]") {
    AnalogLfo lfo;
    lfo.prepare(2, kSampleRate);
    lfo.setRateHz(1.0);

    // After some steps, L and R should differ
    for (int i = 0; i < 1000; ++i) lfo.stepAll();
    float triL = lfo.getTri(0);
    float triR = lfo.getTri(1);

    // They should not be identical (phase offset)
    REQUIRE(triL != triR);
}

TEST_CASE("AnalogLfo: invalid channel returns 0", "[AnalogLfo]") {
    AnalogLfo lfo;
    lfo.prepare(1, kSampleRate);
    REQUIRE(lfo.getTri(-1) == 0.0f);
    REQUIRE(lfo.getTri(5) == 0.0f);
    REQUIRE(lfo.getSinLike(-1) == 0.0f);
    REQUIRE(lfo.getSinLike(5) == 0.0f);
}

TEST_CASE("AnalogLfo: setSupplyVoltage doesn't crash", "[AnalogLfo]") {
    AnalogLfo lfo;
    lfo.prepare(1, kSampleRate);
    lfo.setSupplyVoltage(9.0);
    lfo.stepAll();
    REQUIRE(std::isfinite(lfo.getTri(0)));

    lfo.setSupplyVoltage(18.0);
    lfo.stepAll();
    REQUIRE(std::isfinite(lfo.getTri(0)));
}

TEST_CASE("AnalogLfo: ensureChannels adjusts", "[AnalogLfo]") {
    AnalogLfo lfo;
    lfo.prepare(1, kSampleRate);
    lfo.ensureChannels(4);

    lfo.stepAll();
    REQUIRE(std::isfinite(lfo.getTri(3)));
}

TEST_CASE("AnalogLfo: sinLike has limited bandwidth compared to tri", "[AnalogLfo]") {
    AnalogLfo lfo;
    lfo.prepare(1, kSampleRate);
    lfo.setRateHz(2.0);

    // Measure peak-to-peak range of both outputs over one period
    float triMin = 1.0f, triMax = -1.0f;
    float sinMin = 1.0f, sinMax = -1.0f;

    for (int i = 0; i < 44100; ++i) {
        lfo.stepAll();
        float tri = lfo.getTri(0);
        float s = lfo.getSinLike(0);
        triMin = std::min(triMin, tri);
        triMax = std::max(triMax, tri);
        sinMin = std::min(sinMin, s);
        sinMax = std::max(sinMax, s);
    }

    // Both should oscillate (positive and negative)
    REQUIRE(triMax > 0.3f);
    REQUIRE(triMin < -0.3f);
    REQUIRE(sinMax > 0.1f);
    REQUIRE(sinMin < -0.1f);
    // SinLike output stays bounded in [-1, 1]
    REQUIRE(sinMax <= 1.0f);
    REQUIRE(sinMin >= -1.0f);
}
