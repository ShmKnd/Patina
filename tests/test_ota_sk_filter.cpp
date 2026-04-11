// ============================================================================
// OtaSKFilter unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/filters/OtaSKFilter.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("OtaSK: prepare and reset without crash", "[OtaSK]") {
    OtaSKFilter f;
    f.prepare(2, kSR);
    f.reset();
    REQUIRE(true);
}

TEST_CASE("OtaSK: prepare with ProcessSpec", "[OtaSK]") {
    OtaSKFilter f;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    f.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("OtaSK: silence in -> silence out (LP)", "[OtaSK]") {
    OtaSKFilter f;
    f.prepare(1, kSR);
    f.setMode(OtaSKFilter::Mode::LowPass);
    float out = 0.0f;
    for (int i = 0; i < 1000; ++i)
        out = f.process(0, 0.0f);
    REQUIRE(std::fabs(out) < 1e-6f);
}

TEST_CASE("OtaSK: LP passes low frequencies", "[OtaSK]") {
    OtaSKFilter f;
    f.prepare(1, kSR);
    f.setCutoffHz(4000.0f);
    f.setResonance(0.0f);
    f.setMode(OtaSKFilter::Mode::LowPass);

    double sumLow = 0.0, sumHigh = 0.0;
    const int N = 8000, skip = 2000;

    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 200.0 * i / kSR);
        float out = f.process(0, in);
        if (i >= skip) sumLow += out * out;
    }
    f.reset();
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 10000.0 * i / kSR);
        float out = f.process(0, in);
        if (i >= skip) sumHigh += out * out;
    }
    REQUIRE(sumLow > sumHigh * 10.0);
}

TEST_CASE("OtaSK: HP passes high frequencies", "[OtaSK]") {
    OtaSKFilter f;
    f.prepare(1, kSR);
    f.setCutoffHz(2000.0f);
    f.setResonance(0.0f);
    f.setMode(OtaSKFilter::Mode::HighPass);

    double sumLow = 0.0, sumHigh = 0.0;
    const int N = 8000, skip = 2000;

    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 100.0 * i / kSR);
        float out = f.process(0, in);
        if (i >= skip) sumLow += out * out;
    }
    f.reset();
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 8000.0 * i / kSR);
        float out = f.process(0, in);
        if (i >= skip) sumHigh += out * out;
    }
    REQUIRE(sumHigh > sumLow * 5.0);
}

TEST_CASE("OtaSK: BandPass attenuates both extremes", "[OtaSK]") {
    OtaSKFilter f;
    f.prepare(1, kSR);
    f.setCutoffHz(2000.0f);
    f.setResonance(0.3f);
    f.setMode(OtaSKFilter::Mode::BandPass);

    double sumLow = 0.0, sumMid = 0.0, sumHigh = 0.0;
    const int N = 8000, skip = 2000;

    // 100 Hz
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 100.0 * i / kSR);
        float out = f.process(0, in);
        if (i >= skip) sumLow += out * out;
    }
    f.reset();
    // 2000 Hz
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 2000.0 * i / kSR);
        float out = f.process(0, in);
        if (i >= skip) sumMid += out * out;
    }
    f.reset();
    // 15000 Hz
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0 * M_PI * 15000.0 * i / kSR);
        float out = f.process(0, in);
        if (i >= skip) sumHigh += out * out;
    }
    REQUIRE(sumMid > sumLow * 0.5);
    REQUIRE(sumMid > sumHigh * 2.0);
}

TEST_CASE("OtaSK: resonance boosts near cutoff", "[OtaSK]") {
    const float fc = 2000.0f;
    const int N = 8000, skip = 2000;

    auto measureEnergy = [&](float reso) {
        OtaSKFilter f;
        f.prepare(1, kSR);
        f.setCutoffHz(fc);
        f.setResonance(reso);
        f.setMode(OtaSKFilter::Mode::LowPass);
        double sum = 0.0;
        for (int i = 0; i < N; ++i) {
            float in = 0.3f * std::sin(2.0 * M_PI * (double)fc * i / kSR);
            float out = f.process(0, in);
            if (i >= skip) sum += out * out;
        }
        return sum;
    };

    double enoReso = measureEnergy(0.0f);
    double eHiReso = measureEnergy(0.7f);
    REQUIRE(eHiReso > enoReso * 1.5);
}

TEST_CASE("OtaSK: OTA drive adds distortion", "[OtaSK]") {
    OtaSKFilter f;
    f.prepare(1, kSR);
    f.setMode(OtaSKFilter::Mode::LowPass);

    OtaSKFilter::Params p;
    p.cutoffHz = 5000.0f;
    p.resonance = 0.0f;
    p.mode = 0;

    // Clean
    p.drive = 0.0f;
    f.reset();
    double sumClean = 0.0;
    for (int i = 0; i < 4000; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = f.process(0, in, p);
        if (i >= 1000) sumClean += out * out;
    }

    // Driven
    p.drive = 1.0f;
    f.reset();
    double sumDriven = 0.0;
    for (int i = 0; i < 4000; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = f.process(0, in, p);
        if (i >= 1000) sumDriven += out * out;
    }

    // Drive should change the signal (different RMS)
    REQUIRE(std::fabs(sumClean - sumDriven) > 0.001);
}

TEST_CASE("OtaSK: temperature changes cutoff slightly", "[OtaSK]") {
    const int N = 8000, skip = 2000;
    const float fc = 2000.0f;
    const float testFreq = 2500.0f;

    auto measureEnergy = [&](float temp) {
        OtaSKFilter f;
        f.prepare(1, kSR);
        OtaSKFilter::Params p;
        p.cutoffHz = fc;
        p.resonance = 0.0f;
        p.temperature = temp;
        p.mode = 0;
        double sum = 0.0;
        for (int i = 0; i < N; ++i) {
            float in = std::sin(2.0 * M_PI * (double)testFreq * i / kSR);
            float out = f.process(0, in, p);
            if (i >= skip) sum += out * out;
        }
        return sum;
    };

    double e25 = measureEnergy(25.0f);
    double e55 = measureEnergy(55.0f);
    REQUIRE(std::fabs(e25 - e55) > 0.001);
}

TEST_CASE("OtaSK: processBlock matches per-sample", "[OtaSK]") {
    OtaSKFilter f1, f2;
    f1.prepare(1, kSR);
    f2.prepare(1, kSR);

    OtaSKFilter::Params p;
    p.cutoffHz = 1500.0f;
    p.resonance = 0.3f;
    p.mode = 0;

    const int N = 256;
    std::vector<float> buf(N);
    for (int i = 0; i < N; ++i)
        buf[i] = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);

    // Per-sample
    std::vector<float> ref(N);
    for (int i = 0; i < N; ++i)
        ref[i] = f1.process(0, buf[i], p);

    // Block
    float* ptr = buf.data();
    f2.processBlock(&ptr, 1, N, p);

    for (int i = 0; i < N; ++i)
        REQUIRE(buf[i] == Approx(ref[i]).margin(1e-6f));
}

TEST_CASE("OtaSK: multichannel independence", "[OtaSK]") {
    OtaSKFilter f;
    f.prepare(2, kSR);
    f.setCutoffHz(1000.0f);
    f.setResonance(0.0f);
    f.setMode(OtaSKFilter::Mode::LowPass);

    // Drive ch0 only
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
