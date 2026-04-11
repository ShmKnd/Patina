// ============================================================================
// StateVariableFilter unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/filters/StateVariableFilter.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("SVF: prepare and reset without crash", "[SVF]") {
    StateVariableFilter svf;
    svf.prepare(2, kSR);
    svf.reset();
    REQUIRE(true);
}

TEST_CASE("SVF: prepare with ProcessSpec", "[SVF]") {
    StateVariableFilter svf;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    svf.prepare(spec);
    svf.reset();
    REQUIRE(true);
}

TEST_CASE("SVF: silence in -> silence out", "[SVF]") {
    StateVariableFilter svf;
    svf.prepare(1, kSR);
    svf.setCutoffHz(1000.0f);
    svf.setResonance(0.5f);

    float out = svf.process(0, 0.0f, 0);
    REQUIRE(std::fabs(out) < 1e-6f);
}

TEST_CASE("SVF: LP passes DC", "[SVF]") {
    StateVariableFilter svf;
    svf.prepare(1, kSR);
    svf.setCutoffHz(5000.0f);
    svf.setResonance(0.0f);

    float out = 0.0f;
    for (int i = 0; i < 4000; ++i)
        out = svf.process(0, 1.0f, (int)StateVariableFilter::Type::LowPass);
    REQUIRE(out == Approx(1.0f).margin(0.05f));
}

TEST_CASE("SVF: HP blocks DC", "[SVF]") {
    StateVariableFilter svf;
    svf.prepare(1, kSR);
    svf.setCutoffHz(200.0f);
    svf.setResonance(0.0f);

    float out = 0.0f;
    for (int i = 0; i < 4000; ++i)
        out = svf.process(0, 1.0f, (int)StateVariableFilter::Type::HighPass);
    REQUIRE(std::fabs(out) < 0.05f);
}

TEST_CASE("SVF: LP attenuates high frequencies", "[SVF]") {
    StateVariableFilter svf;
    svf.prepare(1, kSR);
    svf.setCutoffHz(500.0f);
    svf.setResonance(0.0f);

    double sumLow = 0.0, sumHigh = 0.0;
    const int N = 8000;
    const int skip = 2000;

    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 100.0 * i / kSR);
        float out = svf.process(0, in, (int)StateVariableFilter::Type::LowPass);
        if (i >= skip) sumLow += out * out;
    }
    svf.reset();
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 5000.0 * i / kSR);
        float out = svf.process(0, in, (int)StateVariableFilter::Type::LowPass);
        if (i >= skip) sumHigh += out * out;
    }
    REQUIRE(sumLow > sumHigh * 10.0);
}

TEST_CASE("SVF: HP attenuates low frequencies", "[SVF]") {
    StateVariableFilter svf;
    svf.prepare(1, kSR);
    svf.setCutoffHz(2000.0f);
    svf.setResonance(0.0f);

    double sumLow = 0.0, sumHigh = 0.0;
    const int N = 8000;
    const int skip = 2000;

    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 100.0 * i / kSR);
        float out = svf.process(0, in, (int)StateVariableFilter::Type::HighPass);
        if (i >= skip) sumLow += out * out;
    }
    svf.reset();
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 10000.0 * i / kSR);
        float out = svf.process(0, in, (int)StateVariableFilter::Type::HighPass);
        if (i >= skip) sumHigh += out * out;
    }
    REQUIRE(sumHigh > sumLow * 10.0);
}

TEST_CASE("SVF: processAll returns all 4 types simultaneously", "[SVF]") {
    StateVariableFilter svf;
    svf.prepare(1, kSR);
    svf.setCutoffHz(1000.0f);
    svf.setResonance(0.3f);

    // Feed some signal to build state
    for (int i = 0; i < 1000; ++i) {
        float in = std::sin(2.0 * M_PI * 1000.0 * i / kSR);
        svf.processAll(0, in);
    }

    float in = std::sin(2.0 * M_PI * 1000.0 * 1000 / kSR);
    auto out = svf.processAll(0, in);
    REQUIRE(std::isfinite(out.lp));
    REQUIRE(std::isfinite(out.hp));
    REQUIRE(std::isfinite(out.bp));
    REQUIRE(std::isfinite(out.notch));
}

TEST_CASE("SVF: resonance boost at cutoff", "[SVF]") {
    const float testFreq = 1000.0f;
    double energyLowQ = 0.0, energyHighQ = 0.0;
    const int N = 8000;
    const int skip = 2000;

    // Low resonance
    {
        StateVariableFilter svf;
        svf.prepare(1, kSR);
        svf.setCutoffHz(testFreq);
        svf.setResonance(0.0f);
        for (int i = 0; i < N; ++i) {
            float in = std::sin(2.0 * M_PI * testFreq * i / kSR);
            float out = svf.process(0, in, (int)StateVariableFilter::Type::BandPass);
            if (i >= skip) energyLowQ += out * out;
        }
    }
    // High resonance
    {
        StateVariableFilter svf;
        svf.prepare(1, kSR);
        svf.setCutoffHz(testFreq);
        svf.setResonance(0.9f);
        for (int i = 0; i < N; ++i) {
            float in = std::sin(2.0 * M_PI * testFreq * i / kSR);
            float out = svf.process(0, in, (int)StateVariableFilter::Type::BandPass);
            if (i >= skip) energyHighQ += out * out;
        }
    }
    REQUIRE(energyHighQ > energyLowQ);
}

TEST_CASE("SVF: processBlock matches sample-by-sample", "[SVF]") {
    const int N = 256;
    StateVariableFilter svf1, svf2;
    svf1.prepare(1, kSR);
    svf2.prepare(1, kSR);

    StateVariableFilter::Params params;
    params.cutoffHz = 2000.0f;
    params.resonance = 0.3f;
    params.type = (int)StateVariableFilter::Type::LowPass;

    std::vector<float> buf1(N), buf2(N);
    for (int i = 0; i < N; ++i)
        buf1[i] = buf2[i] = std::sin(2.0 * M_PI * 440.0 * i / kSR);

    // Sample-by-sample
    svf1.setCutoffHz(params.cutoffHz);
    svf1.setResonance(params.resonance);
    for (int i = 0; i < N; ++i)
        buf1[i] = svf1.process(0, buf1[i], params.type);

    // Block
    float* ch[] = { buf2.data() };
    svf2.processBlock(ch, 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(buf1[i] == Approx(buf2[i]).margin(1e-5f));
}

TEST_CASE("SVF: multichannel independence", "[SVF]") {
    StateVariableFilter svf;
    svf.prepare(2, kSR);
    svf.setCutoffHz(1000.0f);
    svf.setResonance(0.5f);

    // Feed signal only to channel 0
    for (int i = 0; i < 1000; ++i)
        svf.process(0, 0.5f, (int)StateVariableFilter::Type::LowPass);

    // Channel 1 should still be at zero
    float out = svf.process(1, 0.0f, (int)StateVariableFilter::Type::LowPass);
    REQUIRE(std::fabs(out) < 1e-6f);
}

TEST_CASE("SVF: output is finite for extreme inputs", "[SVF]") {
    StateVariableFilter svf;
    svf.prepare(1, kSR);
    svf.setCutoffHz(100.0f);
    svf.setResonance(0.99f);

    for (int i = 0; i < 100; ++i) {
        float out = svf.process(0, 10.0f, (int)StateVariableFilter::Type::LowPass);
        REQUIRE(std::isfinite(out));
    }
}
