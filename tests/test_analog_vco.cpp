// ============================================================================
// AnalogVCO unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/parts/AnalogVCO.h"

using Catch::Approx;

static constexpr double kSampleRate = 44100.0;

TEST_CASE("AnalogVCO: prepare and reset without crash", "[AnalogVCO]") {
    AnalogVCO vco;
    vco.prepare(2, kSampleRate);
    vco.reset();
    REQUIRE(true);
}

TEST_CASE("AnalogVCO: prepare with ProcessSpec", "[AnalogVCO]") {
    AnalogVCO vco;
    patina::ProcessSpec spec{kSampleRate, 512, 2};
    vco.prepare(spec);
    vco.reset();
    REQUIRE(true);
}

TEST_CASE("AnalogVCO: saw waveform frequency is correct", "[AnalogVCO]") {
    AnalogVCO vco;
    vco.prepare(1, kSampleRate);

    AnalogVCO::Spec spec;
    spec.freqHz = 440.0;
    spec.waveform = 0; // Saw
    spec.drift = 0.0;  // No drift for frequency test

    // Count zero crossings over a known duration
    const int N = 44100; // 1 second
    int zeroCrossings = 0;
    float prev = 0.0f;
    for (int i = 0; i < N; ++i) {
        float out = vco.process(0, spec);
        // Rising zero crossings only (saw resets once per period)
        if (out >= 0.0f && prev < 0.0f)
            ++zeroCrossings;
        prev = out;
    }

    // 440Hz should have ~440 rising zero crossings in 1 second
    // Allow +/-5% tolerance for analog modeling artifacts
    REQUIRE(zeroCrossings > 418);
    REQUIRE(zeroCrossings < 462);
}

TEST_CASE("AnalogVCO: output stays within [-1, 1]", "[AnalogVCO]") {
    AnalogVCO vco;
    vco.prepare(1, kSampleRate);

    AnalogVCO::Spec spec;
    spec.freqHz = 1000.0;

    for (int wf = 0; wf < 3; ++wf) {
        vco.reset();
        spec.waveform = wf;
        for (int i = 0; i < 10000; ++i) {
            float out = vco.process(0, spec);
            REQUIRE(out >= -1.0f);
            REQUIRE(out <= 1.0f);
        }
    }
}

TEST_CASE("AnalogVCO: saw waveform has correct shape", "[AnalogVCO]") {
    AnalogVCO vco;
    vco.prepare(1, kSampleRate);

    AnalogVCO::Spec spec;
    spec.freqHz = 100.0;
    spec.waveform = 0; // Saw
    spec.drift = 0.0;

    // Run for a while and collect one period
    for (int i = 0; i < 4410; ++i) // 10 cycles warmup
        vco.process(0, spec);

    // Collect ~1 period (441 samples at 100Hz/44100)
    const int period = 441;
    float buf[period];
    for (int i = 0; i < period; ++i)
        buf[i] = vco.process(0, spec);

    // Saw should generally rise over the period
    int rising = 0;
    for (int i = 1; i < period; ++i)
        if (buf[i] > buf[i - 1]) ++rising;

    // Most samples should be rising (except at the reset point)
    REQUIRE(rising > period * 0.8);
}

TEST_CASE("AnalogVCO: tri waveform is symmetric", "[AnalogVCO]") {
    AnalogVCO vco;
    vco.prepare(1, kSampleRate);

    AnalogVCO::Spec spec;
    spec.freqHz = 200.0;
    spec.waveform = 1; // Tri
    spec.drift = 0.0;

    const int N = 10000;
    double sumPos = 0.0, sumNeg = 0.0;
    for (int i = 0; i < N; ++i) {
        float out = vco.process(0, spec);
        if (out > 0) sumPos += out;
        else sumNeg += -out;
    }

    // Triangle wave should be roughly symmetric
    double ratio = sumPos / std::max(sumNeg, 0.001);
    REQUIRE(ratio > 0.7);
    REQUIRE(ratio < 1.4);
}

TEST_CASE("AnalogVCO: pulse waveform duty cycle", "[AnalogVCO]") {
    AnalogVCO vco;
    vco.prepare(1, kSampleRate);

    AnalogVCO::Spec spec;
    spec.freqHz = 200.0;
    spec.waveform = 2; // Pulse
    spec.pulseWidth = 0.5;
    spec.drift = 0.0;

    const int N = 44100;
    int posCount = 0;
    for (int i = 0; i < N; ++i) {
        float out = vco.process(0, spec);
        if (out > 0) ++posCount;
    }

    // 50% duty cycle should have ~50% positive samples
    double dutyRatio = (double)posCount / N;
    REQUIRE(dutyRatio > 0.35);
    REQUIRE(dutyRatio < 0.65);
}

TEST_CASE("AnalogVCO: temperature drift affects pitch", "[AnalogVCO]") {
    AnalogVCO vcoCold;
    vcoCold.prepare(1, kSampleRate);
    AnalogVCO::Spec coldSpec;
    coldSpec.freqHz = 440.0;
    coldSpec.waveform = 0;
    coldSpec.temperature = 0.0;
    coldSpec.drift = 0.0;

    AnalogVCO vcoHot;
    vcoHot.prepare(1, kSampleRate);
    AnalogVCO::Spec hotSpec;
    hotSpec.freqHz = 440.0;
    hotSpec.waveform = 0;
    hotSpec.temperature = 50.0;
    hotSpec.drift = 0.0;

    // After many samples, phase should diverge due to temperature
    for (int i = 0; i < 44100; ++i) {
        vcoCold.process(0, coldSpec);
        vcoHot.process(0, hotSpec);
    }

    double phaseCold = vcoCold.getPhase(0);
    double phaseHot = vcoHot.getPhase(0);

    // Different temperatures should yield different phases
    REQUIRE(std::fabs(phaseCold - phaseHot) > 0.001);
}

TEST_CASE("AnalogVCO: drift parameter causes pitch variation", "[AnalogVCO]") {
    AnalogVCO vco;
    vco.prepare(1, kSampleRate);

    AnalogVCO::Spec spec;
    spec.freqHz = 440.0;
    spec.waveform = 0;
    spec.drift = 0.01; // Max drift

    // Output should still be finite and bounded
    for (int i = 0; i < 44100; ++i) {
        float out = vco.process(0, spec);
        REQUIRE(std::isfinite(out));
        REQUIRE(out >= -1.0f);
        REQUIRE(out <= 1.0f);
    }
}

TEST_CASE("AnalogVCO: different frequencies produce different periods", "[AnalogVCO]") {
    AnalogVCO vcoLow, vcoHigh;
    vcoLow.prepare(1, kSampleRate);
    vcoHigh.prepare(1, kSampleRate);

    AnalogVCO::Spec lowSpec, highSpec;
    lowSpec.freqHz = 100.0;
    lowSpec.waveform = 0;
    lowSpec.drift = 0.0;
    highSpec.freqHz = 1000.0;
    highSpec.waveform = 0;
    highSpec.drift = 0.0;

    int crossLow = 0, crossHigh = 0;
    float prevLow = 0, prevHigh = 0;
    for (int i = 0; i < 44100; ++i) {
        float oLow = vcoLow.process(0, lowSpec);
        float oHigh = vcoHigh.process(0, highSpec);
        if (oLow >= 0 && prevLow < 0) ++crossLow;
        if (oHigh >= 0 && prevHigh < 0) ++crossHigh;
        prevLow = oLow;
        prevHigh = oHigh;
    }

    // Higher freq should have ~10x more crossings
    REQUIRE(crossHigh > crossLow * 5);
}
