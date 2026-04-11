// ============================================================================
// FastMath.h unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <limits>
#include "dsp/core/FastMath.h"

using Catch::Approx;

TEST_CASE("fastTanh: zero input returns zero", "[FastMath]") {
    REQUIRE(FastMath::fastTanh(0.0) == 0.0);
}

TEST_CASE("fastTanh: matches std::tanh within tolerance for normal range", "[FastMath]") {
    // Test many points in the Pad-accurate range [-3.5, 3.5]
    for (double x = -3.5; x <= 3.5; x += 0.1) {
        double approx = FastMath::fastTanh(x);
        double ref = std::tanh(x);
        REQUIRE(approx == Approx(ref).margin(2e-7));
    }
}

TEST_CASE("fastTanh: clamps to +/-1 for large inputs", "[FastMath]") {
    REQUIRE(FastMath::fastTanh(5.0) == 1.0);
    REQUIRE(FastMath::fastTanh(-5.0) == -1.0);
    REQUIRE(FastMath::fastTanh(100.0) == 1.0);
    REQUIRE(FastMath::fastTanh(-100.0) == -1.0);
}

TEST_CASE("fastTanh: blend region [3.5, 4.0] is monotonic", "[FastMath]") {
    double prev = FastMath::fastTanh(3.5);
    for (double x = 3.6; x <= 4.0; x += 0.05) {
        double cur = FastMath::fastTanh(x);
        REQUIRE(cur >= prev);
        prev = cur;
    }
    // Negative side
    prev = FastMath::fastTanh(-3.5);
    for (double x = -3.6; x >= -4.0; x -= 0.05) {
        double cur = FastMath::fastTanh(x);
        REQUIRE(cur <= prev);
        prev = cur;
    }
}

TEST_CASE("fastTanh: odd symmetry", "[FastMath]") {
    for (double x = 0.1; x <= 4.0; x += 0.3) {
        REQUIRE(FastMath::fastTanh(x) == Approx(-FastMath::fastTanh(-x)).margin(1e-12));
    }
}

TEST_CASE("fastTanh: handles NaN/Inf", "[FastMath]") {
    REQUIRE(FastMath::fastTanh(std::numeric_limits<double>::infinity()) == 1.0);
    REQUIRE(FastMath::fastTanh(-std::numeric_limits<double>::infinity()) == -1.0);
    // NaN -> should return 1.0 or -1.0 (implementation defined, just check finite)
    double nanResult = FastMath::fastTanh(std::numeric_limits<double>::quiet_NaN());
    REQUIRE(std::isfinite(nanResult));
}

TEST_CASE("fastTanhf: float variant works", "[FastMath]") {
    float result = FastMath::fastTanhf(1.0f);
    REQUIRE(result == Approx(std::tanh(1.0f)).margin(1e-5f));
}

TEST_CASE("fastTanhf: zero returns zero", "[FastMath]") {
    REQUIRE(FastMath::fastTanhf(0.0f) == 0.0f);
}
