// ============================================================================
// StereoImage.h unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/circuits/modulation/StereoImage.h"

using Catch::Approx;

TEST_CASE("StereoImage: depth=0 is no-op", "[StereoImage]") {
    float L = 0.8f, R = -0.3f;
    float origL = L, origR = R;
    StereoImage::widenEqualPowerSIMD(L, R, 0.0f);
    REQUIRE(L == origL);
    REQUIRE(R == origR);
}

TEST_CASE("StereoImage: mono signal stays mono at any depth", "[StereoImage]") {
    float L = 0.5f, R = 0.5f;
    StereoImage::widenEqualPowerSIMD(L, R, 1.0f);
    // For a mono signal (L==R), Side=0, so widening should have no effect
    REQUIRE(L == Approx(R).margin(0.001f));
}

TEST_CASE("StereoImage: stereo widening increases stereo width", "[StereoImage]") {
    float L1 = 0.7f, R1 = -0.3f;
    float L2 = L1, R2 = R1;
    
    // No widening
    StereoImage::widenEqualPowerSIMD(L1, R1, 0.01f);
    // Full widening
    StereoImage::widenEqualPowerSIMD(L2, R2, 1.0f);
    
    // Check that the stereo difference (L-R) increases with depth
    float diff1 = std::fabs(L1 - R1);
    float diff2 = std::fabs(L2 - R2);
    REQUIRE(diff2 >= diff1);
}

TEST_CASE("StereoImage: output is finite", "[StereoImage]") {
    float L = 1.0f, R = -1.0f;
    StereoImage::widenEqualPowerSIMD(L, R, 0.5f);
    REQUIRE(std::isfinite(L));
    REQUIRE(std::isfinite(R));
}

TEST_CASE("StereoImage: vector variant works", "[StereoImage]") {
    std::vector<float> stereo = {0.6f, -0.4f};
    StereoImage::widenEqualPower(stereo, 0.5f);
    REQUIRE(std::isfinite(stereo[0]));
    REQUIRE(std::isfinite(stereo[1]));
}

TEST_CASE("StereoImage: vector variant no-op for mono buffer", "[StereoImage]") {
    std::vector<float> mono = {0.5f};
    float orig = mono[0];
    StereoImage::widenEqualPower(mono, 1.0f);
    REQUIRE(mono[0] == orig); // Mono buffer should be untouched
}
