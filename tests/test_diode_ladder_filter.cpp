// ============================================================================
// DiodeLadderFilter unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/filters/DiodeLadderFilter.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("DiodeLadder: prepare and reset without crash", "[DiodeLadder]") {
    DiodeLadderFilter f;
    f.prepare(2, kSR);
    f.reset();
    REQUIRE(true);
}

TEST_CASE("DiodeLadder: prepare with ProcessSpec", "[DiodeLadder]") {
    DiodeLadderFilter f;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    f.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("DiodeLadder: silence in -> silence out", "[DiodeLadder]") {
    DiodeLadderFilter f;
    f.prepare(1, kSR);
    float out = 0.0f;
    for (int i = 0; i < 1000; ++i)
        out = f.process(0, 0.0f);
    REQUIRE(std::fabs(out) < 1e-6f);
}

TEST_CASE("DiodeLadder: LP attenuates high frequencies (3-pole)", "[DiodeLadder]") {
    DiodeLadderFilter f;
    f.prepare(1, kSR);
    f.setCutoffHz(1000.0f);
    f.setResonance(0.0f);

    double sumLow = 0.0, sumHigh = 0.0;
    const int N = 8000, skip = 2000;

    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 200.0 * i / kSR);
        float out = f.process(0, in);
        if (i >= skip) sumLow += out * out;
    }
    f.reset();
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 8000.0 * i / kSR);
        float out = f.process(0, in);
        if (i >= skip) sumHigh += out * out;
    }
    // 3-pole rolloff: strong attenuation above cutoff
    REQUIRE(sumLow > sumHigh * 20.0);
}

TEST_CASE("DiodeLadder: resonance boosts near cutoff", "[DiodeLadder]") {
    const float fc = 2000.0f;
    const int N = 8000, skip = 2000;

    auto measureEnergy = [&](float reso) {
        DiodeLadderFilter f;
        f.prepare(1, kSR);
        f.setCutoffHz(fc);
        f.setResonance(reso);
        double sum = 0.0;
        for (int i = 0; i < N; ++i) {
            float in = 0.3f * std::sin(2.0 * M_PI * (double)fc * i / kSR);
            float out = f.process(0, in);
            if (i >= skip) sum += out * out;
        }
        return sum;
    };

    double eNoReso = measureEnergy(0.0f);
    double eHiReso = measureEnergy(0.7f);
    REQUIRE(eHiReso > eNoReso * 1.2);
}

TEST_CASE("DiodeLadder: drive adds diode distortion", "[DiodeLadder]") {
    DiodeLadderFilter f;
    f.prepare(1, kSR);
    f.setCutoffHz(5000.0f);
    f.setResonance(0.0f);

    const int N = 4000, skip = 1000;

    // Clean
    f.reset();
    double sumClean = 0.0;
    for (int i = 0; i < N; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = f.process(0, in, 0.0f);
        if (i >= skip) sumClean += out * out;
    }

    // Driven
    f.reset();
    double sumDriven = 0.0;
    for (int i = 0; i < N; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = f.process(0, in, 1.0f);
        if (i >= skip) sumDriven += out * out;
    }

    REQUIRE(std::fabs(sumClean - sumDriven) > 0.01);
}

TEST_CASE("DiodeLadder: temperature changes response", "[DiodeLadder]") {
    const int N = 8000, skip = 2000;

    auto measureEnergy = [&](float temp) {
        DiodeLadderFilter f;
        f.prepare(1, kSR);
        DiodeLadderFilter::Params p;
        p.cutoffHz = 2000.0f;
        p.resonance = 0.0f;
        p.temperature = temp;
        double sum = 0.0;
        for (int i = 0; i < N; ++i) {
            float in = std::sin(2.0 * M_PI * 2500.0 * i / kSR);
            float out = f.process(0, in, p);
            if (i >= skip) sum += out * out;
        }
        return sum;
    };

    double e25 = measureEnergy(25.0f);
    double e55 = measureEnergy(55.0f);
    REQUIRE(std::fabs(e25 - e55) > 0.001);
}

TEST_CASE("DiodeLadder: processBlock matches per-sample", "[DiodeLadder]") {
    DiodeLadderFilter f1, f2;
    f1.prepare(1, kSR);
    f2.prepare(1, kSR);

    DiodeLadderFilter::Params p;
    p.cutoffHz = 1500.0f;
    p.resonance = 0.3f;
    p.drive = 0.2f;

    const int N = 256;
    std::vector<float> buf(N);
    for (int i = 0; i < N; ++i)
        buf[i] = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);

    std::vector<float> ref(N);
    for (int i = 0; i < N; ++i)
        ref[i] = f1.process(0, buf[i], p);

    float* ptr = buf.data();
    f2.processBlock(&ptr, 1, N, p);

    for (int i = 0; i < N; ++i)
        REQUIRE(buf[i] == Approx(ref[i]).margin(1e-6f));
}

TEST_CASE("DiodeLadder: output stays bounded with high resonance", "[DiodeLadder]") {
    DiodeLadderFilter f;
    f.prepare(1, kSR);
    f.setCutoffHz(1000.0f);
    f.setResonance(0.95f);

    float maxAbs = 0.0f;
    for (int i = 0; i < 16000; ++i) {
        float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = f.process(0, in);
        maxAbs = std::max(maxAbs, std::fabs(out));
    }
    // Should not explode with high resonance
    REQUIRE(maxAbs < 20.0f);
}

TEST_CASE("DiodeLadder: multichannel independence", "[DiodeLadder]") {
    DiodeLadderFilter f;
    f.prepare(2, kSR);
    f.setCutoffHz(1000.0f);
    f.setResonance(0.0f);

    for (int i = 0; i < 4000; ++i) {
        float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        f.process(0, in);
        f.process(1, 0.0f);
    }
    float out0 = f.process(0, 0.3f);
    float out1 = f.process(1, 0.0f);
    REQUIRE(std::fabs(out0) > 0.01f);
    REQUIRE(std::fabs(out1) < 1e-4f);
}
