// ============================================================================
// TubePreamp unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/saturation/TubePreamp.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("TubePreamp: prepare and reset without crash", "[Tube]") {
    TubePreamp tp;
    tp.prepare(2, kSR);
    tp.reset();
    REQUIRE(true);
}

TEST_CASE("TubePreamp: prepare with ProcessSpec", "[Tube]") {
    TubePreamp tp;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    tp.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("TubePreamp: silence in -> silence out", "[Tube]") {
    TubePreamp tp;
    tp.prepare(1, kSR);
    TubePreamp::Params params;

    float out = tp.process(0, 0.0f, params);
    REQUIRE(std::fabs(out) < 0.01f);
}

TEST_CASE("TubePreamp: soft saturation (output bounded)", "[Tube]") {
    TubePreamp tp;
    tp.prepare(1, kSR);
    TubePreamp::Params params;
    params.drive = 1.0f;
    params.bias = 0.5f;
    params.outputLevel = 1.0f;

    float maxOut = 0.0f;
    for (int i = 0; i < 4000; ++i) {
        float in = 2.0f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = tp.process(0, in, params);
        maxOut = std::max(maxOut, std::fabs(out));
    }
    // Tube saturation should limit output despite large input
    REQUIRE(maxOut < 5.0f);
    REQUIRE(maxOut > 0.0f);
}

TEST_CASE("TubePreamp: asymmetric distortion (even harmonics)", "[Tube]") {
    TubePreamp tp;
    tp.prepare(1, kSR);
    TubePreamp::Params params;
    params.drive = 0.8f;
    params.bias = 0.5f;
    params.outputLevel = 0.7f;
    params.enableGridConduction = true;

    // Feed a sine and measure asymmetry (positive vs negative peak)
    float maxPos = 0.0f, maxNeg = 0.0f;
    for (int i = 0; i < 4000; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = tp.process(0, in, params);
        if (i > 2000) {
            if (out > maxPos) maxPos = out;
            if (out < maxNeg) maxNeg = out;
        }
    }
    REQUIRE(std::isfinite(maxPos));
    REQUIRE(std::isfinite(maxNeg));
    // Tube amp should create some asymmetry
    REQUIRE(std::fabs(maxPos + maxNeg) > 0.001f);
}

TEST_CASE("TubePreamp: drive increases harmonic energy", "[Tube]") {
    double energyLow = 0.0, energyHigh = 0.0;
    const int N = 4000;
    const int skip = 2000;

    TubePreamp::Params params;
    params.outputLevel = 0.7f;

    // Low drive
    {
        TubePreamp tp;
        tp.prepare(1, kSR);
        params.drive = 0.1f;
        for (int i = 0; i < N; ++i) {
            float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            float out = tp.process(0, in, params);
            if (i >= skip) energyLow += out * out;
        }
    }
    // High drive
    {
        TubePreamp tp;
        tp.prepare(1, kSR);
        params.drive = 1.0f;
        for (int i = 0; i < N; ++i) {
            float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            float out = tp.process(0, in, params);
            if (i >= skip) energyHigh += out * out;
        }
    }
    REQUIRE(energyHigh > energyLow);
}

TEST_CASE("TubePreamp: grid conduction toggle", "[Tube]") {
    TubePreamp tp;
    tp.prepare(1, kSR);

    TubePreamp::Params paramsOn, paramsOff;
    paramsOn.drive = 0.8f;
    paramsOn.enableGridConduction = true;
    paramsOff.drive = 0.8f;
    paramsOff.enableGridConduction = false;

    double sumOn = 0.0, sumOff = 0.0;
    const int N = 4000;
    for (int i = 0; i < N; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = tp.process(0, in, paramsOn);
        if (i > 2000) sumOn += out * out;
    }
    tp.reset();
    for (int i = 0; i < N; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = tp.process(0, in, paramsOff);
        if (i > 2000) sumOff += out * out;
    }
    // Should produce different output
    REQUIRE(std::fabs(sumOn - sumOff) > 0.01);
}

TEST_CASE("TubePreamp: processBlock produces finite output", "[Tube]") {
    const int N = 512;
    TubePreamp tp;
    tp.prepare(1, kSR);

    TubePreamp::Params params;
    params.drive = 0.6f;

    std::vector<float> buf(N);
    for (int i = 0; i < N; ++i)
        buf[i] = std::sin(2.0 * M_PI * 440.0 * i / kSR);

    float* ch[] = { buf.data() };
    tp.processBlock(ch, 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(buf[i]));
}

TEST_CASE("TubePreamp: output finite for extreme inputs", "[Tube]") {
    TubePreamp tp;
    tp.prepare(1, kSR);
    TubePreamp::Params params;
    params.drive = 1.0f;

    for (int i = 0; i < 200; ++i) {
        float out = tp.process(0, 50.0f, params);
        REQUIRE(std::isfinite(out));
    }
}
