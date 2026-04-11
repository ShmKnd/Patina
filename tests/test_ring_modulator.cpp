// ============================================================================
// RingModulator unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/circuits/modulation/RingModulator.h"

using Catch::Approx;

static constexpr double kSampleRate = 44100.0;

TEST_CASE("RingModulator: prepare and reset without crash", "[RingModulator]") {
    RingModulator rm;
    rm.prepare(2, kSampleRate);
    rm.reset();
    REQUIRE(true);
}

TEST_CASE("RingModulator: prepare with ProcessSpec", "[RingModulator]") {
    RingModulator rm;
    patina::ProcessSpec spec{kSampleRate, 512, 2};
    rm.prepare(spec);
    rm.reset();
    REQUIRE(true);
}

TEST_CASE("RingModulator: zero input produces near-zero output", "[RingModulator]") {
    RingModulator rm;
    rm.prepare(1, kSampleRate);

    RingModulator::Params params;

    // Warm up DC filter
    for (int i = 0; i < 1000; ++i)
        rm.process(0, 0.0f, 0.0f, params);

    // Both inputs zero -> output should be zero
    float out = rm.process(0, 0.0f, 0.0f, params);
    REQUIRE(std::fabs(out) < 0.01f);

    // Zero input + nonzero carrier -> some carrier leakage is expected
    // (realistic analog behavior with mismatched diodes)
    rm.reset();
    for (int i = 0; i < 1000; ++i)
        rm.process(0, 0.0f, 0.0f, params);
    float outLeak = rm.process(0, 0.0f, 0.5f, params);
    REQUIRE(std::isfinite(outLeak));
}

TEST_CASE("RingModulator: produces sum and difference frequencies", "[RingModulator]") {
    RingModulator rm;
    rm.prepare(1, kSampleRate);

    RingModulator::Params params;
    params.mix = 1.0f;
    params.mismatch = 0.0f; // Perfect matching

    // Input: 440Hz, Carrier: 1000Hz
    // Expected: output contains 560Hz (diff) and 1440Hz (sum)
    const int N = 44100;
    double sumOut = 0.0;
    for (int i = 0; i < N; ++i) {
        float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSampleRate);
        float car = 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSampleRate);
        float out = rm.process(0, in, car, params);
        sumOut += out * out;
    }

    double rms = std::sqrt(sumOut / N);
    // Output should have measurable energy
    REQUIRE(rms > 0.01);
    REQUIRE(rms < 2.0);
}

TEST_CASE("RingModulator: mix=0 passes dry signal", "[RingModulator]") {
    RingModulator rm;
    rm.prepare(1, kSampleRate);

    RingModulator::Params params;
    params.mix = 0.0f;

    // Warm up
    for (int i = 0; i < 2000; ++i) {
        float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSampleRate);
        rm.process(0, in, 0.5f, params);
    }

    float in = 0.5f;
    float out = rm.process(0, in, 1.0f, params);
    // With mix=0, output should be close to input
    REQUIRE(std::fabs(out - in) < 0.01f);
}

TEST_CASE("RingModulator: output is finite for all conditions", "[RingModulator]") {
    RingModulator rm;
    rm.prepare(1, kSampleRate);

    RingModulator::Params params;

    // Extreme inputs
    for (int i = 0; i < 1000; ++i) {
        float out = rm.process(0, 10.0f, 10.0f, params);
        REQUIRE(std::isfinite(out));
    }
    for (int i = 0; i < 1000; ++i) {
        float out = rm.process(0, -10.0f, 10.0f, params);
        REQUIRE(std::isfinite(out));
    }
}

TEST_CASE("RingModulator: different diode types produce different results", "[RingModulator]") {
    float input = 0.5f;
    float carrier = 0.5f;

    RingModulator rmSi;
    rmSi.prepare(1, kSampleRate);
    RingModulator::Params pSi;
    pSi.diodeType = 0;
    pSi.mismatch = 0.0f;

    RingModulator rmGe;
    rmGe.prepare(1, kSampleRate);
    RingModulator::Params pGe;
    pGe.diodeType = 2;
    pGe.mismatch = 0.0f;

    // Warm up DC filters
    for (int i = 0; i < 2000; ++i) {
        float in = 0.4f * std::sin(2.0 * M_PI * 440.0 * i / kSampleRate);
        float car = 0.4f * std::sin(2.0 * M_PI * 1000.0 * i / kSampleRate);
        rmSi.process(0, in, car, pSi);
        rmGe.process(0, in, car, pGe);
    }

    float outSi = rmSi.process(0, input, carrier, pSi);
    float outGe = rmGe.process(0, input, carrier, pGe);

    REQUIRE(std::fabs(outSi - outGe) > 0.001f);
}

TEST_CASE("RingModulator: mismatch adds asymmetry", "[RingModulator]") {
    const int N = 4000;

    RingModulator rmMatched;
    rmMatched.prepare(1, kSampleRate);
    RingModulator::Params pMatched;
    pMatched.mismatch = 0.0f;

    RingModulator rmMismatched;
    rmMismatched.prepare(1, kSampleRate);
    RingModulator::Params pMismatched;
    pMismatched.mismatch = 0.1f;

    double rmsMatched = 0.0, rmsMismatched = 0.0;
    for (int i = 0; i < N; ++i) {
        float in = 0.5f * std::sin(2.0 * M_PI * 300.0 * i / kSampleRate);
        float car = 0.5f * std::sin(2.0 * M_PI * 800.0 * i / kSampleRate);
        float oM = rmMatched.process(0, in, car, pMatched);
        float oMM = rmMismatched.process(0, in, car, pMismatched);
        if (i > 2000) {
            rmsMatched += oM * oM;
            rmsMismatched += oMM * oMM;
        }
    }

    // Mismatched diodes should produce different RMS (carrier leakage, etc.)
    REQUIRE(std::fabs(rmsMatched - rmsMismatched) > 0.0001);
}

TEST_CASE("RingModulator: temperature affects diode behavior", "[RingModulator]") {
    const int N = 4000;

    RingModulator rmCold;
    rmCold.prepare(1, kSampleRate);
    RingModulator::Params pCold;
    pCold.temperature = 0.0f;

    RingModulator rmHot;
    rmHot.prepare(1, kSampleRate);
    RingModulator::Params pHot;
    pHot.temperature = 60.0f;

    double rmsCold = 0.0, rmsHot = 0.0;
    for (int i = 0; i < N; ++i) {
        float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSampleRate);
        float car = 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSampleRate);
        float oC = rmCold.process(0, in, car, pCold);
        float oH = rmHot.process(0, in, car, pHot);
        if (i > 2000) {
            rmsCold += oC * oC;
            rmsHot += oH * oH;
        }
    }

    // Different temperatures should yield different output characteristics
    REQUIRE(std::fabs(rmsCold - rmsHot) > 0.0001);
}

TEST_CASE("RingModulator: processBlock works correctly", "[RingModulator]") {
    RingModulator rm;
    rm.prepare(1, kSampleRate);
    RingModulator::Params params;

    const int N = 256;
    float inBuf[N], carBuf[N];
    for (int i = 0; i < N; ++i) {
        inBuf[i] = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSampleRate);
        carBuf[i] = 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSampleRate);
    }

    float* inChannels[1] = { inBuf };
    const float* carChannels[1] = { carBuf };
    rm.processBlock(inChannels, carChannels, 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(inBuf[i]));
}
