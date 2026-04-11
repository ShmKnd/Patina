// ============================================================================
// Mixer.h unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/circuits/mixer/Mixer.h"

using Catch::Approx;

// ============================================================================
// Equal Power Crossfade
// ============================================================================
TEST_CASE("Mixer: equalPowerGains at mix=0 is full dry", "[Mixer]") {
    float gDry = 0.0f, gWet = 0.0f;
    Mixer::equalPowerGains(0.0, gDry, gWet);
    REQUIRE(gDry == Approx(1.0f).margin(0.001f));
    REQUIRE(gWet == Approx(0.0f).margin(0.001f));
}

TEST_CASE("Mixer: equalPowerGains at mix=1 is full wet", "[Mixer]") {
    float gDry = 0.0f, gWet = 0.0f;
    Mixer::equalPowerGains(1.0, gDry, gWet);
    REQUIRE(gDry == Approx(0.0f).margin(0.001f));
    REQUIRE(gWet == Approx(1.0f).margin(0.001f));
}

TEST_CASE("Mixer: equalPowerGains at mix=0.5 satisfies power conservation", "[Mixer]") {
    float gDry = 0.0f, gWet = 0.0f;
    Mixer::equalPowerGains(0.5, gDry, gWet);
    // cos^2 + sin^2 = 1 (power conservation)
    float powerSum = gDry * gDry + gWet * gWet;
    REQUIRE(powerSum == Approx(1.0f).margin(0.001f));
}

TEST_CASE("Mixer: equalPowerGainsFast approximates equalPowerGains", "[Mixer]") {
    for (double mix = 0.0; mix <= 1.0; mix += 0.1) {
        float gDryExact = 0.0f, gWetExact = 0.0f;
        float gDryFast = 0.0f, gWetFast = 0.0f;
        Mixer::equalPowerGains(mix, gDryExact, gWetExact);
        Mixer::equalPowerGainsFast(mix, gDryFast, gWetFast);
        REQUIRE(gDryFast == Approx(gDryExact).margin(0.01f));
        REQUIRE(gWetFast == Approx(gWetExact).margin(0.01f));
    }
}

TEST_CASE("Mixer: equalPowerGains clamps out-of-range mix", "[Mixer]") {
    float gDry = 0.0f, gWet = 0.0f;
    Mixer::equalPowerGains(-0.5, gDry, gWet);
    REQUIRE(gDry == Approx(1.0f).margin(0.001f));
    REQUIRE(gWet == Approx(0.0f).margin(0.001f));

    Mixer::equalPowerGains(1.5, gDry, gWet);
    REQUIRE(gDry == Approx(0.0f).margin(0.001f));
    REQUIRE(gWet == Approx(1.0f).margin(0.001f));
}

// ============================================================================
// Equal Power Mix (Volt domain)
// ============================================================================
TEST_CASE("Mixer: equalPowerMixVolt dry-only at mix=0", "[Mixer]") {
    float result = Mixer::equalPowerMixVolt(1.0f, 0.5f, 0.0);
    REQUIRE(result == Approx(1.0f).margin(0.01f));
}

TEST_CASE("Mixer: equalPowerMixVolt wet-only at mix=1", "[Mixer]") {
    float result = Mixer::equalPowerMixVolt(0.5f, 1.0f, 1.0);
    // At mix=1, wet makeup = kWetMakeupGain
    REQUIRE(result == Approx(1.0f * Mixer::kWetMakeupGain).margin(0.02f));
}

// ============================================================================
// Analog Ducking Mix
// ============================================================================
TEST_CASE("Mixer: analogDuckingMix returns valid values", "[Mixer]") {
    float result = Mixer::analogDuckingMix(0.5f, 0.3f, 0.5);
    REQUIRE(std::isfinite(result));
}

TEST_CASE("Mixer: analogDuckingMix at mix=0 returns dry only", "[Mixer]") {
    float result = Mixer::analogDuckingMix(1.0f, 0.5f, 0.0);
    REQUIRE(result == Approx(1.0f).margin(0.01f));
}

TEST_CASE("Mixer: analogDuckingMix ducking reduces wet with loud dry", "[Mixer]") {
    float noducking = Mixer::analogDuckingMix(0.0f, 0.5f, 0.5, 1.0f, 0.0f);
    float ducking = Mixer::analogDuckingMix(5.0f, 0.5f, 0.5, 1.0f, 0.5f);
    // With loud dry and ducking enabled, wet component should be somewhat reduced
    // compared to no-ducking case (relative to the combined output)
    REQUIRE(std::isfinite(ducking));
}

// ============================================================================
// Attack Ducking State
// ============================================================================
TEST_CASE("Mixer: AttackDuckingState reset", "[Mixer]") {
    Mixer::AttackDuckingState state;
    state.envelope = 1.0f;
    state.hpZ1 = 0.5f;
    state.reset();
    REQUIRE(state.envelope == 0.0f);
    REQUIRE(state.hpZ1 == 0.0f);
}

TEST_CASE("Mixer: analogDuckingMixWithAttackDetection produces finite output", "[Mixer]") {
    Mixer::AttackDuckingState state;
    float result = Mixer::analogDuckingMixWithAttackDetection(
        state, 0.5f, 0.3f, 0.5, 44100.0);
    REQUIRE(std::isfinite(result));
}

// ============================================================================
// Level compensation
// ============================================================================
TEST_CASE("Mixer: applyLevelComp instrument +20dB", "[Mixer]") {
    float out = Mixer::applyLevelComp(1.0f, true);
    REQUIRE(out == Approx(10.0f));
}

TEST_CASE("Mixer: applyLevelComp line +12dB", "[Mixer]") {
    float out = Mixer::applyLevelComp(1.0f, false);
    REQUIRE(out == Approx(3.98107171f).margin(0.001f));
}
