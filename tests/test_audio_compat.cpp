// ============================================================================
// AudioCompat.h unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "dsp/core/AudioCompat.h"

using namespace patina::compat;
using Catch::Approx;

// ============================================================================
// AudioBuffer
// ============================================================================
TEST_CASE("AudioBuffer: construction and sizing", "[AudioCompat][AudioBuffer]") {
    AudioBuffer<float> buf(2, 512);
    REQUIRE(buf.getNumChannels() == 2);
    REQUIRE(buf.getNumSamples() == 512);
}

TEST_CASE("AudioBuffer: default construction is empty", "[AudioCompat][AudioBuffer]") {
    AudioBuffer<float> buf;
    REQUIRE(buf.getNumChannels() == 0);
    REQUIRE(buf.getNumSamples() == 0);
}

TEST_CASE("AudioBuffer: zero/negative size handled gracefully", "[AudioCompat][AudioBuffer]") {
    AudioBuffer<float> buf(0, 100);
    REQUIRE(buf.getNumChannels() == 0);

    AudioBuffer<float> buf2(-1, 100);
    REQUIRE(buf2.getNumChannels() == 0);
}

TEST_CASE("AudioBuffer: clear zeros all samples", "[AudioCompat][AudioBuffer]") {
    AudioBuffer<float> buf(2, 64);
    buf.setSample(0, 0, 1.0f);
    buf.setSample(1, 32, -0.5f);
    buf.clear();

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i)
            REQUIRE(buf.getSample(ch, i) == 0.0f);
}

TEST_CASE("AudioBuffer: get/set sample roundtrip", "[AudioCompat][AudioBuffer]") {
    AudioBuffer<float> buf(1, 16);
    buf.setSample(0, 5, 0.42f);
    REQUIRE(buf.getSample(0, 5) == Approx(0.42f));
}

TEST_CASE("AudioBuffer: out-of-range access returns zero", "[AudioCompat][AudioBuffer]") {
    AudioBuffer<float> buf(2, 64);
    REQUIRE(buf.getSample(5, 0) == 0.0f);   // ch out of range
    REQUIRE(buf.getSample(0, 999) == 0.0f);  // idx out of range
    REQUIRE(buf.getSample(-1, 0) == 0.0f);   // negative ch
}

TEST_CASE("AudioBuffer: pointer access", "[AudioCompat][AudioBuffer]") {
    AudioBuffer<float> buf(2, 64);
    float* wp = buf.getWritePointer(0);
    REQUIRE(wp != nullptr);
    wp[10] = 0.77f;
    REQUIRE(buf.getSample(0, 10) == Approx(0.77f));

    const float* rp = buf.getReadPointer(0);
    REQUIRE(rp[10] == Approx(0.77f));
}

TEST_CASE("AudioBuffer: write pointer null for invalid channel", "[AudioCompat][AudioBuffer]") {
    AudioBuffer<float> buf(1, 16);
    REQUIRE(buf.getWritePointer(5) == nullptr);
    REQUIRE(buf.getWritePointer(-1) == nullptr);
}

TEST_CASE("AudioBuffer: setSize resizes correctly", "[AudioCompat][AudioBuffer]") {
    AudioBuffer<float> buf(1, 16);
    buf.setSample(0, 0, 1.0f);
    buf.setSize(4, 128);
    REQUIRE(buf.getNumChannels() == 4);
    REQUIRE(buf.getNumSamples() == 128);
    // After resize, all samples should be zeroed
    REQUIRE(buf.getSample(0, 0) == 0.0f);
}

TEST_CASE("AudioBuffer: double precision", "[AudioCompat][AudioBuffer]") {
    AudioBuffer<double> buf(2, 32);
    buf.setSample(0, 0, 3.141592653589793);
    REQUIRE(buf.getSample(0, 0) == Approx(3.141592653589793));
}

// ============================================================================
// SIMDRegister
// ============================================================================
TEST_CASE("SIMDRegister: default is zero", "[AudioCompat][SIMD]") {
    SIMDRegister<float> r;
    for (auto val : r.v) REQUIRE(val == 0.0f);
}

TEST_CASE("SIMDRegister: scalar broadcast", "[AudioCompat][SIMD]") {
    SIMDRegister<float> r(2.5f);
    for (auto val : r.v) REQUIRE(val == 2.5f);
}

TEST_CASE("SIMDRegister: arithmetic operations", "[AudioCompat][SIMD]") {
    SIMDRegister<float> a(2.0f);
    SIMDRegister<float> b(3.0f);

    auto sum = a + b;
    for (auto val : sum.v) REQUIRE(val == Approx(5.0f));

    auto diff = a - b;
    for (auto val : diff.v) REQUIRE(val == Approx(-1.0f));

    auto prod = a * b;
    for (auto val : prod.v) REQUIRE(val == Approx(6.0f));

    auto scaled = a * 0.5f;
    for (auto val : scaled.v) REQUIRE(val == Approx(1.0f));
}

TEST_CASE("SIMDRegister: fromRawArray / copyToRawArray roundtrip", "[AudioCompat][SIMD]") {
    float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    auto reg = SIMDRegister<float>::fromRawArray(data);

    float out[4];
    reg.copyToRawArray(out);
    for (int i = 0; i < 4; ++i) REQUIRE(out[i] == data[i]);
}

// ============================================================================
// IIRFilter / IIRCoefficients
// ============================================================================
TEST_CASE("IIRCoefficients: makeLowPass produces valid coefficients", "[AudioCompat][IIR]") {
    auto spec = dsp::IIRCoefficients<float>::makeLowPass(44100.0, 1000.0);
    // b0 + b1 + b2 should equal 1 + a1 + a2 at DC (gain = 1)
    float dcGain = (spec.b0 + spec.b1 + spec.b2) / (1.0f + spec.a1 + spec.a2);
    REQUIRE(dcGain == Approx(1.0f).margin(0.001f));
}

TEST_CASE("IIRCoefficients: makeHighPass produces valid coefficients", "[AudioCompat][IIR]") {
    auto spec = dsp::IIRCoefficients<float>::makeHighPass(44100.0, 1000.0);
    // DC gain of high pass should be near 0
    float dcGain = (spec.b0 + spec.b1 + spec.b2) / (1.0f + spec.a1 + spec.a2);
    REQUIRE(std::fabs(dcGain) < 0.01f);
}

TEST_CASE("IIRFilter: LPF passes DC", "[AudioCompat][IIR]") {
    dsp::IIRFilter<float> filt;
    auto spec = dsp::IIRCoefficients<float>::makeLowPass(44100.0, 5000.0);
    filt.setCoefficients(spec);

    // Feed 1.0 for many samples and check convergence to 1.0
    float out = 0.0f;
    for (int i = 0; i < 2000; ++i) out = filt.processSample(1.0f);
    REQUIRE(out == Approx(1.0f).margin(0.001f));
}

TEST_CASE("IIRFilter: HPF blocks DC", "[AudioCompat][IIR]") {
    dsp::IIRFilter<float> filt;
    auto spec = dsp::IIRCoefficients<float>::makeHighPass(44100.0, 200.0);
    filt.setCoefficients(spec);

    float out = 0.0f;
    for (int i = 0; i < 5000; ++i) out = filt.processSample(1.0f);
    REQUIRE(std::fabs(out) < 0.001f);
}

TEST_CASE("IIRFilter: reset clears state", "[AudioCompat][IIR]") {
    dsp::IIRFilter<float> filt;
    auto spec = dsp::IIRCoefficients<float>::makeLowPass(44100.0, 1000.0);
    filt.setCoefficients(spec);

    filt.processSample(1.0f);
    filt.processSample(0.5f);
    filt.reset();

    // After reset, processing 0.0 should yield 0.0
    REQUIRE(filt.processSample(0.0f) == 0.0f);
}

// ============================================================================
// MathConstants
// ============================================================================
TEST_CASE("MathConstants: pi values", "[AudioCompat][Math]") {
    REQUIRE(MathConstants<double>::pi == Approx(3.14159265358979));
    REQUIRE(MathConstants<double>::twoPi == Approx(6.28318530717959));
    REQUIRE(MathConstants<double>::halfPi == Approx(1.57079632679490));
}

// ============================================================================
// ignoreUnused
// ============================================================================
TEST_CASE("ignoreUnused compiles without warnings", "[AudioCompat]") {
    int a = 0;
    float b = 1.0f;
    ignoreUnused(a, b);
    REQUIRE(true); // If it compiles, we're good
}
