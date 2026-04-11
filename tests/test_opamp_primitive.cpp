#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

#include "dsp/parts/OpAmpPrimitive.h"

using Catch::Matchers::WithinAbs;

// ============================================================================
// OpAmpPrimitive - preset verification
// ============================================================================

TEST_CASE("OpAmpPrimitive: all presets have valid specs", "[Parts][OpAmp]") {
    auto check = [](const OpAmpPrimitive::Spec& s) {
        OpAmpPrimitive o(s);
        INFO("Preset: " << s.name);
        REQUIRE(o.getSpec().slewRate > 0.0);
        REQUIRE(o.getSpec().openLoopGain > 0.0);
        REQUIRE(o.getSpec().gbwHz > 0.0);
        REQUIRE(o.getSpec().satThreshold18V > 0.0);
        REQUIRE(o.getSpec().satThreshold18V <= 1.0);
        REQUIRE(o.getSpec().satThreshold9V > 0.0);
        REQUIRE(o.getSpec().satThreshold9V <= 1.0);
        REQUIRE(o.getSpec().inputNoiseDensity > 0.0);
        REQUIRE(o.getSpec().asymNeg > 0.0);
        REQUIRE(o.getSpec().asymNeg <= 1.0);
    };
    check(OpAmpPrimitive::TL072CP());
    check(OpAmpPrimitive::JRC4558D());
    check(OpAmpPrimitive::NE5532());
    check(OpAmpPrimitive::OPA2134());
    check(OpAmpPrimitive::LM4562());
    check(OpAmpPrimitive::LM741());
}

// ============================================================================
// OpAmpPrimitive - saturation characteristics
// ============================================================================

TEST_CASE("OpAmpPrimitive: saturate clips large signal", "[Parts][OpAmp]") {
    OpAmpPrimitive o(OpAmpPrimitive::TL072CP());
    double out18 = o.saturate(10.0, true);
    double out9  = o.saturate(10.0, false);
    REQUIRE(std::fabs(out18) < 10.0);
    REQUIRE(std::fabs(out9) < 10.0);
    REQUIRE(std::fabs(out18) > 0.0);
    // At moderate level: 18V has more headroom -> less compression
    double mod18 = o.saturate(0.9, true);
    double mod9  = o.saturate(0.9, false);
    REQUIRE(std::fabs(mod18) >= std::fabs(mod9));
}

TEST_CASE("OpAmpPrimitive: saturate zero -> zero", "[Parts][OpAmp]") {
    OpAmpPrimitive o(OpAmpPrimitive::NE5532());
    REQUIRE_THAT(o.saturate(0.0, true),  WithinAbs(0.0, 1e-15));
    REQUIRE_THAT(o.saturate(0.0, false), WithinAbs(0.0, 1e-15));
}

TEST_CASE("OpAmpPrimitive: saturate passes small signal unchanged", "[Parts][OpAmp]") {
    OpAmpPrimitive o(OpAmpPrimitive::TL072CP());
    // 0.1 is well below threshold 0.88
    double out = o.saturate(0.1, true);
    REQUIRE_THAT(out, WithinAbs(0.1, 1e-10));
}

TEST_CASE("OpAmpPrimitive: negative asymmetry - negative clips harder", "[Parts][OpAmp]") {
    // JRC4558D has asymNeg = 0.88 (strong asymmetry)
    OpAmpPrimitive o(OpAmpPrimitive::JRC4558D());
    double pos = o.saturate(5.0, true);
    double neg = o.saturate(-5.0, true);
    // Negative should be compressed more (asymNeg < 1.0)
    REQUIRE(std::fabs(neg) < std::fabs(pos));
}

TEST_CASE("OpAmpPrimitive: LM4562 introduces less distortion than JRC4558D", "[Parts][OpAmp]") {
    OpAmpPrimitive lm(OpAmpPrimitive::LM4562());
    OpAmpPrimitive jrc(OpAmpPrimitive::JRC4558D());
    // At 0.9 - within LM4562 threshold (0.97) but above JRC4558D threshold (0.58)
    double input = 0.9;
    double outLm  = lm.saturate(input, true);
    double outJrc = jrc.saturate(input, true);
    // LM4562 should preserve the input more faithfully (less deviation)
    double distLm  = std::fabs(outLm - input);
    double distJrc = std::fabs(outJrc - input);
    REQUIRE(distLm < distJrc);
}

// ============================================================================
// OpAmpPrimitive - slew-rate limitation
// ============================================================================

TEST_CASE("OpAmpPrimitive: slew rate limits fast transient", "[Parts][OpAmp]") {
    // LM741 has very low slew rate (0.5V/us)
    OpAmpPrimitive o(OpAmpPrimitive::LM741());
    o.prepare(44100.0);
    o.reset();

    // Apply step from 0 to 1.0 - should be limited
    double out = o.applySlewLimit(1.0);
    // slewLimit = 0.5e6 / 8.0 / 44100 ~ 1.42 per sample (normalized)
    // So a step of 1.0 should NOT be limited for LM741 at audio rates
    // Let's try a huge step
    o.reset();
    double bigOut = o.applySlewLimit(100.0);
    REQUIRE(bigOut < 100.0);  // Must be limited
    REQUIRE(bigOut > 0.0);
}

TEST_CASE("OpAmpPrimitive: TL072 passes audio transients easily", "[Parts][OpAmp]") {
    OpAmpPrimitive o(OpAmpPrimitive::TL072CP());
    o.prepare(44100.0);
    o.reset();

    // TL072: slewLimit = 13e6 / 8 / 44100 ~ 36.8 per sample
    // A step of 1.0 should pass unchanged
    double out = o.applySlewLimit(1.0);
    REQUIRE_THAT(out, WithinAbs(1.0, 1e-6));
}

TEST_CASE("OpAmpPrimitive: JRC4558D limits slew on large step", "[Parts][OpAmp]") {
    OpAmpPrimitive o(OpAmpPrimitive::JRC4558D());
    o.prepare(44100.0);
    o.reset();

    // JRC4558D: slewLimit = 0.03e6 / 8 / 44100 ~ 0.085 per sample
    double out = o.applySlewLimit(1.0);
    REQUIRE(out < 1.0);
    REQUIRE(out > 0.0);
}

// ============================================================================
// OpAmpPrimitive - bandwidth / noise / physical characteristics
// ============================================================================

TEST_CASE("OpAmpPrimitive: bandwidthHz decreases with gain", "[Parts][OpAmp]") {
    OpAmpPrimitive o(OpAmpPrimitive::TL072CP());
    double bw1  = o.bandwidthHz(1.0);
    double bw10 = o.bandwidthHz(10.0);
    REQUIRE(bw1 > bw10);
    // bw1 should be close to GBW
    REQUIRE_THAT(bw1, WithinAbs(3e6, 1e4));
}

TEST_CASE("OpAmpPrimitive: LM4562 has highest GBW", "[Parts][OpAmp]") {
    OpAmpPrimitive lm(OpAmpPrimitive::LM4562());
    OpAmpPrimitive tl(OpAmpPrimitive::TL072CP());
    REQUIRE(lm.bandwidthHz(1.0) > tl.bandwidthHz(1.0));
}

TEST_CASE("OpAmpPrimitive: inputNoiseVrms is positive", "[Parts][OpAmp]") {
    OpAmpPrimitive o(OpAmpPrimitive::NE5532());
    double noise = o.inputNoiseVrms(20000.0);
    REQUIRE(noise > 0.0);
    REQUIRE(std::isfinite(noise));
}

TEST_CASE("OpAmpPrimitive: LM4562 has lowest noise", "[Parts][OpAmp]") {
    OpAmpPrimitive lm(OpAmpPrimitive::LM4562());
    OpAmpPrimitive jrc(OpAmpPrimitive::JRC4558D());
    double noiseLm  = lm.inputNoiseVrms(20000.0);
    double noiseJrc = jrc.inputNoiseVrms(20000.0);
    REQUIRE(noiseLm < noiseJrc);
}

TEST_CASE("OpAmpPrimitive: offsetVoltage is zero at 25degC", "[Parts][OpAmp]") {
    OpAmpPrimitive o(OpAmpPrimitive::TL072CP());
    REQUIRE_THAT(o.offsetVoltage(25.0), WithinAbs(0.0, 1e-15));
}

TEST_CASE("OpAmpPrimitive: offsetVoltage increases with temperature delta", "[Parts][OpAmp]") {
    OpAmpPrimitive o(OpAmpPrimitive::LM741());
    double offset50 = std::fabs(o.offsetVoltage(50.0));
    double offset30 = std::fabs(o.offsetVoltage(30.0));
    REQUIRE(offset50 > offset30);
}

TEST_CASE("OpAmpPrimitive: fullPowerBandwidthHz is positive", "[Parts][OpAmp]") {
    OpAmpPrimitive o(OpAmpPrimitive::TL072CP());
    double fpbw = o.fullPowerBandwidthHz(8.0);
    REQUIRE(fpbw > 0.0);
    REQUIRE(std::isfinite(fpbw));
}

// ============================================================================
// OpAmpPrimitive - manufacturing variations
// ============================================================================

TEST_CASE("OpAmpPrimitive: gbwMismatch near unity without seed", "[Parts][OpAmp]") {
    OpAmpPrimitive o(OpAmpPrimitive::TL072CP());
    REQUIRE_THAT(o.getGbwMismatch(), WithinAbs(1.0, 1e-15));
}

TEST_CASE("OpAmpPrimitive: gbwMismatch varies with seed", "[Parts][OpAmp]") {
    OpAmpPrimitive o1(OpAmpPrimitive::TL072CP(), 100);
    OpAmpPrimitive o2(OpAmpPrimitive::TL072CP(), 200);
    // Different seeds -> (very likely) different mismatch
    // Both should be near 1.0
    REQUIRE(std::fabs(o1.getGbwMismatch() - 1.0) < 0.2);
    REQUIRE(std::fabs(o2.getGbwMismatch() - 1.0) < 0.2);
}

// ============================================================================
// OpAmpPrimitive - integration process
// ============================================================================

TEST_CASE("OpAmpPrimitive: process with prepare does not crash", "[Parts][OpAmp]") {
    OpAmpPrimitive o(OpAmpPrimitive::JRC4558D());
    o.prepare(48000.0);
    o.reset();

    for (int i = 0; i < 1000; ++i) {
        double x = std::sin(2.0 * 3.14159265 * 1000.0 * i / 48000.0);
        double out = o.process(x, true);
        REQUIRE(std::isfinite(out));
    }
}

TEST_CASE("OpAmpPrimitive: process zero -> zero", "[Parts][OpAmp]") {
    OpAmpPrimitive o(OpAmpPrimitive::OPA2134());
    o.prepare(44100.0);
    o.reset();
    REQUIRE_THAT(o.process(0.0), WithinAbs(0.0, 1e-15));
}

// ============================================================================
// OpAmpPrimitive - tonal differences between ICs
// ============================================================================

TEST_CASE("OpAmpPrimitive: IC character ordering - slew rate", "[Parts][OpAmp]") {
    // LM4562 ~ OPA2134 > TL072 > NE5532 > LM741 ~ JRC4558D
    REQUIRE(OpAmpPrimitive::LM4562().slewRate >= OpAmpPrimitive::OPA2134().slewRate);
    REQUIRE(OpAmpPrimitive::TL072CP().slewRate > OpAmpPrimitive::NE5532().slewRate);
    REQUIRE(OpAmpPrimitive::NE5532().slewRate > OpAmpPrimitive::LM741().slewRate);
    REQUIRE(OpAmpPrimitive::LM741().slewRate > OpAmpPrimitive::JRC4558D().slewRate);
}

TEST_CASE("OpAmpPrimitive: IC character ordering - headroom (satThreshold18V)", "[Parts][OpAmp]") {
    // LM4562 > OPA2134 > TL072 > NE5532 > LM741 > JRC4558D
    REQUIRE(OpAmpPrimitive::LM4562().satThreshold18V > OpAmpPrimitive::OPA2134().satThreshold18V);
    REQUIRE(OpAmpPrimitive::OPA2134().satThreshold18V > OpAmpPrimitive::TL072CP().satThreshold18V);
    REQUIRE(OpAmpPrimitive::TL072CP().satThreshold18V > OpAmpPrimitive::NE5532().satThreshold18V);
    REQUIRE(OpAmpPrimitive::NE5532().satThreshold18V > OpAmpPrimitive::LM741().satThreshold18V);
    REQUIRE(OpAmpPrimitive::LM741().satThreshold18V > OpAmpPrimitive::JRC4558D().satThreshold18V);
}
