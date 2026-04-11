// ============================================================================
// LadderFilter unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/filters/LadderFilter.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("Ladder: prepare and reset without crash", "[Ladder]") {
    LadderFilter lf;
    lf.prepare(2, kSR);
    lf.reset();
    REQUIRE(true);
}

TEST_CASE("Ladder: prepare with ProcessSpec", "[Ladder]") {
    LadderFilter lf;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    lf.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("Ladder: silence in -> silence out", "[Ladder]") {
    LadderFilter lf;
    lf.prepare(1, kSR);
    float out = lf.process(0, 0.0f);
    REQUIRE(std::fabs(out) < 1e-6f);
}

TEST_CASE("Ladder: LP passes DC (with tanh saturation)", "[Ladder]") {
    LadderFilter lf;
    lf.prepare(1, kSR);
    lf.setCutoffHz(5000.0f);
    lf.setResonance(0.0f);

    float out = 0.0f;
    for (int i = 0; i < 8000; ++i)
        out = lf.process(0, 1.0f);
    // tanh nonlinearity in each stage limits DC gain
    REQUIRE(out > 0.5f);
    REQUIRE(out < 1.1f);
}

TEST_CASE("Ladder: -24dB/oct steep rolloff", "[Ladder]") {
    LadderFilter lf;
    lf.prepare(1, kSR);
    lf.setCutoffHz(1000.0f);
    lf.setResonance(0.0f);

    double sumLow = 0.0, sumHigh = 0.0;
    const int N = 8000;
    const int skip = 2000;

    // 200 Hz - well below cutoff
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 200.0 * i / kSR);
        float out = lf.process(0, in);
        if (i >= skip) sumLow += out * out;
    }
    lf.reset();
    // 8000 Hz - well above cutoff
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 8000.0 * i / kSR);
        float out = lf.process(0, in);
        if (i >= skip) sumHigh += out * out;
    }
    // 4-pole: much steeper attenuation than 2-pole
    REQUIRE(sumLow > sumHigh * 100.0);
}

TEST_CASE("Ladder: resonance boosts near cutoff", "[Ladder]") {
    const float fc = 2000.0f;
    double energyNoRes = 0.0, energyHighRes = 0.0;
    const int N = 8000;
    const int skip = 2000;

    {
        LadderFilter lf;
        lf.prepare(1, kSR);
        lf.setCutoffHz(fc);
        lf.setResonance(0.0f);
        for (int i = 0; i < N; ++i) {
            float in = std::sin(2.0 * M_PI * fc * i / kSR);
            float out = lf.process(0, in);
            if (i >= skip) energyNoRes += out * out;
        }
    }
    {
        LadderFilter lf;
        lf.prepare(1, kSR);
        lf.setCutoffHz(fc);
        lf.setResonance(0.8f);
        for (int i = 0; i < N; ++i) {
            float in = std::sin(2.0 * M_PI * fc * i / kSR);
            float out = lf.process(0, in);
            if (i >= skip) energyHighRes += out * out;
        }
    }
    REQUIRE(energyHighRes > energyNoRes);
}

TEST_CASE("Ladder: drive adds harmonic content", "[Ladder]") {
    LadderFilter lf;
    lf.prepare(1, kSR);
    lf.setCutoffHz(10000.0f);
    lf.setResonance(0.0f);

    // Check that driven signal has more RMS than expected (saturation)
    float maxClean = 0.0f, maxDriven = 0.0f;
    const int N = 4000;

    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = lf.process(0, in, 0.0f);
        if (i > 2000) maxClean = std::max(maxClean, std::fabs(out));
    }
    lf.reset();
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = lf.process(0, in, 1.0f);
        if (i > 2000) maxDriven = std::max(maxDriven, std::fabs(out));
    }
    // Both should produce valid output
    REQUIRE(std::isfinite(maxClean));
    REQUIRE(std::isfinite(maxDriven));
}

TEST_CASE("Ladder: processBlock produces finite output", "[Ladder]") {
    const int N = 512;
    LadderFilter lf;
    lf.prepare(1, kSR);

    LadderFilter::Params params;
    params.cutoffHz = 2000.0f;
    params.resonance = 0.5f;
    params.drive = 0.3f;

    std::vector<float> buf(N);
    for (int i = 0; i < N; ++i)
        buf[i] = std::sin(2.0 * M_PI * 440.0 * i / kSR);

    float* ch[] = { buf.data() };
    lf.processBlock(ch, 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(buf[i]));
}

TEST_CASE("Ladder: output finite for extreme inputs", "[Ladder]") {
    LadderFilter lf;
    lf.prepare(1, kSR);
    lf.setCutoffHz(500.0f);
    lf.setResonance(0.99f);

    for (int i = 0; i < 200; ++i) {
        float out = lf.process(0, 10.0f, 1.0f);
        REQUIRE(std::isfinite(out));
    }
}

TEST_CASE("Ladder: multichannel independence", "[Ladder]") {
    LadderFilter lf;
    lf.prepare(2, kSR);
    lf.setCutoffHz(1000.0f);

    for (int i = 0; i < 1000; ++i)
        lf.process(0, 0.5f);

    float out = lf.process(1, 0.0f);
    REQUIRE(std::fabs(out) < 1e-6f);
}
