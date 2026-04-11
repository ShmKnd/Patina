// ============================================================================
// AnalogAllPass unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/filters/AnalogAllPass.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("AllPass: prepare and reset without crash", "[AllPass]") {
    AnalogAllPass f;
    f.prepare(2, kSR);
    f.reset();
    REQUIRE(true);
}

TEST_CASE("AllPass: prepare with ProcessSpec", "[AllPass]") {
    AnalogAllPass f;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    f.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("AllPass: silence in -> silence out", "[AllPass]") {
    AnalogAllPass f;
    f.prepare(1, kSR);
    float out = 0.0f;
    for (int i = 0; i < 1000; ++i)
        out = f.process(0, 0.0f);
    REQUIRE(std::fabs(out) < 1e-6f);
}

TEST_CASE("AllPass 1st order: flat magnitude response", "[AllPass]") {
    AnalogAllPass f;
    f.prepare(1, kSR);
    f.setCutoffHz(2000.0f);
    f.setOrder(1);

    const int N = 8000, skip = 2000;

    auto measureRms = [&](double freq) {
        f.reset();
        double sum = 0.0;
        for (int i = 0; i < N; ++i) {
            float in = 0.5f * std::sin(2.0 * M_PI * freq * i / kSR);
            float out = f.process(0, in);
            if (i >= skip) sum += out * out;
        }
        return std::sqrt(sum / (N - skip));
    };

    double rms200  = measureRms(200.0);
    double rms2000 = measureRms(2000.0);
    double rms8000 = measureRms(8000.0);

    // All-pass: magnitude should be approximately equal
    REQUIRE(rms200  == Approx(rms2000).epsilon(0.25));
    REQUIRE(rms200  == Approx(rms8000).epsilon(0.25));
}

TEST_CASE("AllPass 2nd order: flat magnitude response", "[AllPass]") {
    AnalogAllPass f;
    f.prepare(1, kSR);
    f.setCutoffHz(2000.0f);
    f.setQ(0.707f);
    f.setOrder(2);

    const int N = 8000, skip = 2000;

    auto measureRms = [&](double freq) {
        f.reset();
        double sum = 0.0;
        for (int i = 0; i < N; ++i) {
            float in = 0.5f * std::sin(2.0 * M_PI * freq * i / kSR);
            float out = f.process(0, in);
            if (i >= skip) sum += out * out;
        }
        return std::sqrt(sum / (N - skip));
    };

    double rms200  = measureRms(200.0);
    double rms2000 = measureRms(2000.0);
    double rms8000 = measureRms(8000.0);

    REQUIRE(rms200  == Approx(rms2000).epsilon(0.3));
    REQUIRE(rms200  == Approx(rms8000).epsilon(0.3));
}

TEST_CASE("AllPass 1st order: phase shifts at cutoff", "[AllPass]") {
    AnalogAllPass f;
    f.prepare(1, kSR);
    f.setCutoffHz(1000.0f);
    f.setOrder(1);

    double phase = f.getPhaseAtFreq(1000.0);
    // 1st order AP: phase at fc should be negative (phase shift occurs)
    REQUIRE(phase < 0.0);
}

TEST_CASE("AllPass: getPhaseAtFreq varies with frequency", "[AllPass]") {
    AnalogAllPass f;
    f.prepare(1, kSR);
    f.setCutoffHz(2000.0f);
    f.setOrder(1);

    double phaseLow  = f.getPhaseAtFreq(200.0);
    double phaseMid  = f.getPhaseAtFreq(2000.0);
    double phaseHigh = f.getPhaseAtFreq(10000.0);

    // Phase should be monotonically decreasing
    REQUIRE(phaseLow > phaseMid);
    REQUIRE(phaseMid > phaseHigh);
}

TEST_CASE("AllPass: setQ changes 2nd-order behavior", "[AllPass]") {
    AnalogAllPass f;
    f.prepare(1, kSR);
    f.setCutoffHz(2000.0f);
    f.setOrder(2);

    f.setQ(0.5f);
    double phaseQ05 = f.getPhaseAtFreq(1500.0);
    f.setQ(5.0f);
    double phaseQ5 = f.getPhaseAtFreq(1500.0);

    REQUIRE(std::fabs(phaseQ05 - phaseQ5) > 0.01);
}

TEST_CASE("AllPass: processBlock matches per-sample", "[AllPass]") {
    AnalogAllPass f1, f2;
    f1.prepare(1, kSR);
    f2.prepare(1, kSR);

    AnalogAllPass::Params p;
    p.cutoffHz = 1500.0f;
    p.q = 0.707f;
    p.order = 1;

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

TEST_CASE("AllPass: multichannel independence", "[AllPass]") {
    AnalogAllPass f;
    f.prepare(2, kSR);
    f.setCutoffHz(1000.0f);
    f.setOrder(1);

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

TEST_CASE("AllPass: order switch does not crash", "[AllPass]") {
    AnalogAllPass f;
    f.prepare(1, kSR);
    f.setOrder(1);
    float out1 = f.process(0, 0.5f);
    f.setOrder(2);
    float out2 = f.process(0, 0.5f);
    f.setOrder(1);
    float out3 = f.process(0, 0.5f);
    // Just check no crash / NaN
    REQUIRE(std::isfinite(out1));
    REQUIRE(std::isfinite(out2));
    REQUIRE(std::isfinite(out3));
}
