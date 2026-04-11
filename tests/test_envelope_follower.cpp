// ============================================================================
// EnvelopeFollower unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/compander/EnvelopeFollower.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("EnvFollower: prepare and reset without crash", "[EnvFollow]") {
    EnvelopeFollower ef;
    ef.prepare(2, kSR);
    ef.reset();
    REQUIRE(true);
}

TEST_CASE("EnvFollower: prepare with ProcessSpec", "[EnvFollow]") {
    EnvelopeFollower ef;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    ef.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("EnvFollower: silence in -> zero envelope", "[EnvFollow]") {
    EnvelopeFollower ef;
    ef.prepare(1, kSR);
    EnvelopeFollower::Params params;

    float env = ef.process(0, 0.0f, params);
    REQUIRE(env >= 0.0f);
    REQUIRE(env < 0.01f);
}

TEST_CASE("EnvFollower: loud signal -> high envelope", "[EnvFollow]") {
    EnvelopeFollower ef;
    ef.prepare(1, kSR);
    EnvelopeFollower::Params params;
    params.attackMs = 1.0f;
    params.releaseMs = 50.0f;
    params.sensitivity = 1.0f;
    params.mode = (int)EnvelopeFollower::DetectionMode::Peak;

    float env = 0.0f;
    for (int i = 0; i < 4000; ++i) {
        float in = 0.9f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        env = ef.process(0, in, params);
    }
    REQUIRE(env > 0.3f);
}

TEST_CASE("EnvFollower: envelope decays after signal stops", "[EnvFollow]") {
    EnvelopeFollower ef;
    ef.prepare(1, kSR);
    EnvelopeFollower::Params params;
    params.attackMs = 1.0f;
    params.releaseMs = 20.0f;
    params.sensitivity = 1.0f;
    params.mode = (int)EnvelopeFollower::DetectionMode::Peak;

    // Build up envelope
    for (int i = 0; i < 4000; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        ef.process(0, in, params);
    }
    float envPeak = ef.getEnvelope(0);

    // Let it decay with silence
    for (int i = 0; i < 4000; ++i)
        ef.process(0, 0.0f, params);
    float envDecayed = ef.getEnvelope(0);

    REQUIRE(envPeak > envDecayed);
}

TEST_CASE("EnvFollower: sensitivity scales output", "[EnvFollow]") {
    double envLow = 0.0, envHigh = 0.0;

    EnvelopeFollower::Params params;
    params.attackMs = 1.0f;
    params.releaseMs = 50.0f;
    params.mode = (int)EnvelopeFollower::DetectionMode::Peak;

    {
        EnvelopeFollower ef;
        ef.prepare(1, kSR);
        params.sensitivity = 0.3f;
        for (int i = 0; i < 4000; ++i) {
            float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            ef.process(0, in, params);
        }
        envLow = ef.getEnvelope(0);
    }
    {
        EnvelopeFollower ef;
        ef.prepare(1, kSR);
        params.sensitivity = 2.0f;
        for (int i = 0; i < 4000; ++i) {
            float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            ef.process(0, in, params);
        }
        envHigh = ef.getEnvelope(0);
    }
    REQUIRE(envHigh > envLow);
}

TEST_CASE("EnvFollower: Peak vs RMS mode", "[EnvFollow]") {
    double envPeak = 0.0, envRMS = 0.0;

    EnvelopeFollower::Params params;
    params.attackMs = 1.0f;
    params.releaseMs = 50.0f;
    params.sensitivity = 1.0f;

    {
        EnvelopeFollower ef;
        ef.prepare(1, kSR);
        params.mode = (int)EnvelopeFollower::DetectionMode::Peak;
        for (int i = 0; i < 4000; ++i) {
            float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            ef.process(0, in, params);
        }
        envPeak = ef.getEnvelope(0);
    }
    {
        EnvelopeFollower ef;
        ef.prepare(1, kSR);
        params.mode = (int)EnvelopeFollower::DetectionMode::RMS;
        for (int i = 0; i < 4000; ++i) {
            float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            ef.process(0, in, params);
        }
        envRMS = ef.getEnvelope(0);
    }
    // Peak should be > RMS for a sine wave
    REQUIRE(envPeak > envRMS);
}

TEST_CASE("EnvFollower: getEnvelope returns current value", "[EnvFollow]") {
    EnvelopeFollower ef;
    ef.prepare(1, kSR);
    EnvelopeFollower::Params params;
    params.attackMs = 1.0f;

    ef.process(0, 0.0f, params);
    float env = ef.getEnvelope(0);
    REQUIRE(env >= 0.0f);
    REQUIRE(std::isfinite(env));
}

TEST_CASE("EnvFollower: envelope stays in [0, 1] range", "[EnvFollow]") {
    EnvelopeFollower ef;
    ef.prepare(1, kSR);
    EnvelopeFollower::Params params;
    params.attackMs = 1.0f;
    params.sensitivity = 1.0f;

    for (int i = 0; i < 8000; ++i) {
        float in = 2.0f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float env = ef.process(0, in, params);
        REQUIRE(env >= 0.0f);
        REQUIRE(env <= 1.01f);  // small margin for float rounding
    }
}

TEST_CASE("EnvFollower: processBlock writes control output", "[EnvFollow]") {
    const int N = 512;
    EnvelopeFollower ef;
    ef.prepare(1, kSR);

    EnvelopeFollower::Params params;
    params.attackMs = 1.0f;
    params.sensitivity = 1.0f;

    std::vector<float> input(N), output(N), control(N);
    for (int i = 0; i < N; ++i)
        input[i] = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);

    const float* inPtrs[] = { input.data() };
    float* outPtrs[] = { output.data() };
    ef.processBlock(inPtrs, outPtrs, control.data(), 1, N, params);

    // Control output should have some non-zero values
    float maxCtrl = 0.0f;
    for (int i = 0; i < N; ++i) {
        REQUIRE(std::isfinite(control[i]));
        maxCtrl = std::max(maxCtrl, control[i]);
    }
    REQUIRE(maxCtrl > 0.0f);
}
