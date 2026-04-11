// ============================================================================
// NoiseGate unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/compander/NoiseGate.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("NoiseGate: prepare and reset without crash", "[Gate]") {
    NoiseGate ng;
    ng.prepare(2, kSR);
    ng.reset();
    REQUIRE(true);
}

TEST_CASE("NoiseGate: prepare with ProcessSpec", "[Gate]") {
    NoiseGate ng;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    ng.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("NoiseGate: silence in -> gate closed, silence out", "[Gate]") {
    NoiseGate ng;
    ng.prepare(1, kSR);
    NoiseGate::Params params;
    params.thresholdDb = -40.0f;
    params.range = 1.0f;

    // Feed silence
    for (int i = 0; i < 4000; ++i)
        ng.process(0, 0.0f, params);

    REQUIRE_FALSE(ng.isGateOpen(0));
}

TEST_CASE("NoiseGate: loud signal opens gate", "[Gate]") {
    NoiseGate ng;
    ng.prepare(1, kSR);
    NoiseGate::Params params;
    params.thresholdDb = -40.0f;
    params.attackMs = 0.5f;
    params.range = 1.0f;

    // Feed loud signal
    for (int i = 0; i < 4000; ++i) {
        float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        ng.process(0, in, params);
    }
    REQUIRE(ng.isGateOpen(0));
}

TEST_CASE("NoiseGate: gate closes after signal stops", "[Gate]") {
    NoiseGate ng;
    ng.prepare(1, kSR);
    NoiseGate::Params params;
    params.thresholdDb = -40.0f;
    params.attackMs = 0.5f;
    params.holdMs = 10.0f;
    params.releaseMs = 20.0f;
    params.range = 1.0f;

    // Open gate with loud signal
    for (int i = 0; i < 4000; ++i) {
        float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        ng.process(0, in, params);
    }
    REQUIRE(ng.isGateOpen(0));

    // Stop signal, wait for release + hold
    for (int i = 0; i < 8000; ++i)
        ng.process(0, 0.0f, params);
    REQUIRE_FALSE(ng.isGateOpen(0));
}

TEST_CASE("NoiseGate: range controls attenuation depth", "[Gate]") {
    float outFullRange = 0.0f, outPartialRange = 0.0f;

    NoiseGate::Params params;
    params.thresholdDb = -10.0f;  // set high so gate stays closed for quiet signal
    params.holdMs = 0.0f;
    params.releaseMs = 1.0f;

    // Full range (complete silence when closed)
    {
        NoiseGate ng;
        ng.prepare(1, kSR);
        params.range = 1.0f;
        // Feed quiet signal below threshold
        for (int i = 0; i < 4000; ++i)
            outFullRange = ng.process(0, 0.01f, params);
    }
    // Partial range
    {
        NoiseGate ng;
        ng.prepare(1, kSR);
        params.range = 0.5f;
        for (int i = 0; i < 4000; ++i)
            outPartialRange = ng.process(0, 0.01f, params);
    }
    // Partial range should let more signal through
    REQUIRE(std::fabs(outPartialRange) >= std::fabs(outFullRange));
}

TEST_CASE("NoiseGate: hysteresis prevents flutter", "[Gate]") {
    NoiseGate ng;
    ng.prepare(1, kSR);
    NoiseGate::Params params;
    params.thresholdDb = -20.0f;
    params.hysteresisDb = 6.0f;
    params.attackMs = 0.5f;
    params.holdMs = 10.0f;
    params.releaseMs = 50.0f;
    params.range = 1.0f;

    // Open gate
    for (int i = 0; i < 2000; ++i) {
        float in = 0.3f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        ng.process(0, in, params);
    }

    // Feed signal just below threshold but above hysteresis - gate should stay open
    // (threshold = -20dB, hysteresis = 6dB, so close threshold = -26dB)
    // Amplitude at -23 dB ~ 0.07
    for (int i = 0; i < 1000; ++i) {
        float in = 0.07f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        ng.process(0, in, params);
    }
    // Should still be open due to hysteresis
    REQUIRE(ng.isGateOpen(0));
}

TEST_CASE("NoiseGate: processBlock produces finite output", "[Gate]") {
    const int N = 512;
    NoiseGate ng;
    ng.prepare(1, kSR);

    NoiseGate::Params params;
    params.thresholdDb = -40.0f;

    std::vector<float> buf(N);
    for (int i = 0; i < N; ++i)
        buf[i] = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);

    float* ch[] = { buf.data() };
    ng.processBlock(ch, 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(buf[i]));
}

TEST_CASE("NoiseGate: multichannel independence", "[Gate]") {
    NoiseGate ng;
    ng.prepare(2, kSR);
    NoiseGate::Params params;
    params.thresholdDb = -40.0f;
    params.attackMs = 0.5f;

    // Open gate on channel 0 only
    for (int i = 0; i < 4000; ++i) {
        ng.process(0, 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR), params);
        ng.process(1, 0.0f, params);
    }
    REQUIRE(ng.isGateOpen(0));
    REQUIRE_FALSE(ng.isGateOpen(1));
}

TEST_CASE("NoiseGate: output finite for extreme inputs", "[Gate]") {
    NoiseGate ng;
    ng.prepare(1, kSR);
    NoiseGate::Params params;

    for (int i = 0; i < 200; ++i) {
        float out = ng.process(0, 10.0f, params);
        REQUIRE(std::isfinite(out));
    }
}
