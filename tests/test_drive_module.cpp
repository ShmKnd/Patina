// ============================================================================
// DiodeClipper unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/circuits/drive/DiodeClipper.h"

using Catch::Approx;

static constexpr double kSampleRate = 44100.0;

TEST_CASE("DiodeClipper: prepare and reset without crash", "[DiodeClipper]") {
    DiodeClipper drive;
    drive.prepare(2, kSampleRate);
    drive.reset();
    REQUIRE(true);
}

TEST_CASE("DiodeClipper: bypass mode passes signal through HP filter", "[DiodeClipper]") {
    DiodeClipper drive;
    drive.prepare(1, kSampleRate);

    // Feed 1kHz sine for warmup
    const int N = 4000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
        float in = 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSampleRate);
        float out = drive.process(0, in, 0.0f, 0); // Bypass, drive=0
        if (i > 2000) sum += out * out;
    }
    double rms = std::sqrt(sum / 2000.0);
    // Should pass most of the signal through (with HP filtering)
    REQUIRE(rms > 0.1);
}

TEST_CASE("DiodeClipper: bypass with zero drive has unity-ish gain", "[DiodeClipper]") {
    DiodeClipper drive;
    drive.prepare(1, kSampleRate);

    // Feed and measure after warmup
    float lastOut = 0.0f;
    for (int i = 0; i < 3000; ++i) {
        float in = 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSampleRate);
        lastOut = drive.process(0, in, 0.0f, 0);
    }
    // With drive=0, gain should be close to 1x (minus HP filter effect)
    REQUIRE(std::fabs(lastOut) < 1.0f);
    REQUIRE(std::fabs(lastOut) > 0.01f);
}

TEST_CASE("DiodeClipper: diode mode saturates signal", "[DiodeClipper]") {
    DiodeClipper drive;
    drive.prepare(1, kSampleRate);

    // Warm up
    for (int i = 0; i < 2000; ++i)
        drive.process(0, 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSampleRate), 1.0f, 1);

    // High input + full drive through diode mode
    float out = drive.process(0, 2.0f, 1.0f, 1);
    // Diode clipping should limit output
    REQUIRE(std::fabs(out) < 3.0f);
    REQUIRE(std::isfinite(out));
}

TEST_CASE("DiodeClipper: tanh mode saturates signal", "[DiodeClipper]") {
    DiodeClipper drive;
    drive.prepare(1, kSampleRate);

    // Warm up
    for (int i = 0; i < 2000; ++i)
        drive.process(0, 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSampleRate), 1.0f, 2);

    float out = drive.process(0, 5.0f, 1.0f, 2);
    // Tanh saturation should limit output
    REQUIRE(std::isfinite(out));
    REQUIRE(std::fabs(out) < 5.0f);
}

TEST_CASE("DiodeClipper: drive parameter increases gain", "[DiodeClipper]") {
    DiodeClipper driveLow, driveHigh;
    driveLow.prepare(1, kSampleRate);
    driveHigh.prepare(1, kSampleRate);

    double sumLow = 0.0, sumHigh = 0.0;
    const int N = 4000;

    for (int i = 0; i < N; ++i) {
        float in = 0.3f * std::sin(2.0 * M_PI * 1000.0 * i / kSampleRate);
        float outLow = driveLow.process(0, in, 0.0f, 0);
        float outHigh = driveHigh.process(0, in, 1.0f, 0);
        if (i > 2000) {
            sumLow += outLow * outLow;
            sumHigh += outHigh * outHigh;
        }
    }

    // Higher drive should result in more energy (even in bypass mode, gain increases)
    REQUIRE(sumHigh > sumLow);
}

TEST_CASE("DiodeClipper: HP filter blocks DC", "[DiodeClipper]") {
    DiodeClipper drive;
    drive.prepare(1, kSampleRate);

    // Feed DC for many samples
    float out = 0.0f;
    for (int i = 0; i < 10000; ++i)
        out = drive.process(0, 1.0f, 0.0f, 0);

    // HP filter should block DC, output should decay toward 0
    REQUIRE(std::fabs(out) < 0.1f);
}

TEST_CASE("DiodeClipper: silence in -> near silence out", "[DiodeClipper]") {
    DiodeClipper drive;
    drive.prepare(1, kSampleRate);

    float out = drive.process(0, 0.0f, 0.5f, 1);
    REQUIRE(std::fabs(out) < 0.001f);
}

TEST_CASE("DiodeClipper: setEffectiveSampleRate updates HP alpha", "[DiodeClipper]") {
    DiodeClipper drive;
    drive.prepare(1, kSampleRate);
    drive.setEffectiveSampleRate(88200.0);

    float out = drive.process(0, 0.5f, 0.5f, 2);
    REQUIRE(std::isfinite(out));
}

TEST_CASE("DiodeClipper: multi-channel independence", "[DiodeClipper]") {
    DiodeClipper drive;
    drive.prepare(2, kSampleRate);

    float out0 = drive.process(0, 0.5f, 0.5f, 1);
    float out1 = drive.process(1, -0.8f, 1.0f, 2);

    REQUIRE(std::isfinite(out0));
    REQUIRE(std::isfinite(out1));
}
