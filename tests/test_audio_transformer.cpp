// ============================================================================
// TransformerPrimitive unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <vector>

#include "dsp/parts/TransformerPrimitive.h"

using Catch::Matchers::WithinAbs;

static constexpr double kSR = 48000.0;

// ============================================================================
// Construction and initialization
// ============================================================================

TEST_CASE("TransformerPrimitive: all presets construct with valid specs", "[Parts][TransformerPrimitive]") {
    auto check = [](const TransformerPrimitive::Spec& s, const char* name) {
        TransformerPrimitive t(s);
        INFO("Preset: " << name);
        REQUIRE(t.getSpec().turnsRatio > 0.0);
        REQUIRE(t.getSpec().primaryInductance > 0.0);
        REQUIRE(t.getSpec().couplingCoeff > 0.0);
        REQUIRE(t.getSpec().couplingCoeff <= 1.0);
        REQUIRE(t.getSpec().coreSatLevel > 0.0);
    };
    check(TransformerPrimitive::Neve1073Input(),    "Neve1073Input");
    check(TransformerPrimitive::API2520Output(),    "API2520Output");
    check(TransformerPrimitive::Lundahl1538(),      "Lundahl1538");
    check(TransformerPrimitive::JensenDIBox(),      "JensenDIBox");
    check(TransformerPrimitive::InterstageTriode(), "InterstageTriode");
    check(TransformerPrimitive::RibbonMicOutput(),  "RibbonMicOutput");
}

TEST_CASE("TransformerPrimitive: prepare and reset without crash", "[Parts][TransformerPrimitive]") {
    TransformerPrimitive t(TransformerPrimitive::Neve1073Input());
    t.prepare(kSR);
    t.reset();
    REQUIRE(true);
}

TEST_CASE("TransformerPrimitive: prepare at various sample rates", "[Parts][TransformerPrimitive]") {
    double rates[] = { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 };
    for (double sr : rates) {
        TransformerPrimitive t(TransformerPrimitive::API2520Output());
        t.prepare(sr);
        double out = t.process(0.5, 0.3);
        INFO("Sample rate: " << sr);
        REQUIRE(std::isfinite(out));
    }
}

// ============================================================================
// Basic processing
// ============================================================================

TEST_CASE("TransformerPrimitive: silence in -> near-silence out", "[Parts][TransformerPrimitive]") {
    TransformerPrimitive t(TransformerPrimitive::Neve1073Input());
    t.prepare(kSR);

    // Warm up filter states
    for (int i = 0; i < 100; ++i)
        t.process(0.0);

    double out = t.process(0.0);
    REQUIRE(std::fabs(out) < 0.01);
}

TEST_CASE("TransformerPrimitive: turns ratio scales output voltage", "[Parts][TransformerPrimitive]") {
    // 1:10 step-up should produce larger output than 1:1
    double peakStepUp = 0.0, peakUnity = 0.0;
    const int N = 4000;
    const int skip = 2000;

    {
        TransformerPrimitive t(TransformerPrimitive::Neve1073Input()); // 1:10
        t.prepare(kSR);
        for (int i = 0; i < N; ++i) {
            double in = 0.01 * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
            double out = t.process(in, 0.0);
            if (i >= skip) peakStepUp = std::max(peakStepUp, std::fabs(out));
        }
    }
    {
        TransformerPrimitive t(TransformerPrimitive::API2520Output()); // 1:1
        t.prepare(kSR);
        for (int i = 0; i < N; ++i) {
            double in = 0.01 * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
            double out = t.process(in, 0.0);
            if (i >= skip) peakUnity = std::max(peakUnity, std::fabs(out));
        }
    }

    REQUIRE(peakStepUp > peakUnity);
}

TEST_CASE("TransformerPrimitive: saturation limits signal at high drive", "[Parts][TransformerPrimitive]") {
    TransformerPrimitive t(TransformerPrimitive::Neve1073Input());
    t.prepare(kSR);

    double maxOut = 0.0;
    for (int i = 0; i < 4000; ++i) {
        double in = 1.0 * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        double out = t.process(in, 1.0);
        maxOut = std::max(maxOut, std::fabs(out));
    }
    REQUIRE(maxOut > 0.0);
    REQUIRE(std::isfinite(maxOut));
    // With saturation, output should not grow unbounded
    REQUIRE(maxOut < 100.0);
}

TEST_CASE("TransformerPrimitive: core saturation compresses large signals", "[Parts][TransformerPrimitive]") {
    TransformerPrimitive t(TransformerPrimitive::Neve1073Input());
    t.prepare(kSR);

    double out_small = t.processCoreSaturation(0.1, 0.5);
    t.reset();
    double out_big = t.processCoreSaturation(5.0, 0.5);

    // Ratio of outputs should be less than ratio of inputs (compression)
    REQUIRE(std::fabs(out_big / out_small) < (5.0 / 0.1));
}

// ============================================================================
// Frequency response
// ============================================================================

TEST_CASE("TransformerPrimitive: HF rolloff attenuates high frequencies", "[Parts][TransformerPrimitive]") {
    // Test processHfRolloff in isolation - pure LP filter
    TransformerPrimitive t(TransformerPrimitive::InterstageTriode());
    t.prepare(kSR);

    double energy1k = 0.0, energy15k = 0.0;
    const int N = 8000;
    const int skip = 4000;

    for (int i = 0; i < N; ++i) {
        double in = 0.1 * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
        double out = t.processHfRolloff(in);
        if (i >= skip) energy1k += out * out;
    }

    t.reset();
    for (int i = 0; i < N; ++i) {
        double in = 0.1 * std::sin(2.0 * M_PI * 15000.0 * i / kSR);
        double out = t.processHfRolloff(in);
        if (i >= skip) energy15k += out * out;
    }

    // 1kHz should have more energy than 15kHz after LP filter
    REQUIRE(energy1k > energy15k);
}

TEST_CASE("TransformerPrimitive: LF coupling creates high-pass behaviour", "[Parts][TransformerPrimitive]") {
    // DI box with high source Z has noticeable LF rolloff
    TransformerPrimitive t(TransformerPrimitive::JensenDIBox());
    t.prepare(kSR);

    double energy20Hz = 0.0, energy1kHz = 0.0;
    const int N = 16000;
    const int skip = 8000;

    for (int i = 0; i < N; ++i) {
        double in = 0.1 * std::sin(2.0 * M_PI * 20.0 * i / kSR);
        double out = t.process(in, 0.0);
        if (i >= skip) energy20Hz += out * out;
    }

    t.reset();
    for (int i = 0; i < N; ++i) {
        double in = 0.1 * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
        double out = t.process(in, 0.0);
        if (i >= skip) energy1kHz += out * out;
    }

    // Normalized per-sample energy at 20Hz should be less (HP behaviour)
    REQUIRE(energy1kHz > energy20Hz);
}

// ============================================================================
// Balanced input / CMRR
// ============================================================================

TEST_CASE("TransformerPrimitive: balanced rejects common-mode signal", "[Parts][TransformerPrimitive]") {
    TransformerPrimitive t(TransformerPrimitive::Neve1073Input());
    t.prepare(kSR);

    // Pure common-mode: hot = cold = signal
    double cmEnergy = 0.0;
    const int N = 4000;
    const int skip = 2000;
    for (int i = 0; i < N; ++i) {
        double sig = 0.5 * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
        double out = t.processBalanced(sig, sig, 0.0);
        if (i >= skip) cmEnergy += out * out;
    }

    t.reset();

    // Pure differential: hot = +signal, cold = -signal
    double diffEnergy = 0.0;
    for (int i = 0; i < N; ++i) {
        double sig = 0.5 * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
        double out = t.processBalanced(sig, -sig, 0.0);
        if (i >= skip) diffEnergy += out * out;
    }

    // Differential should be much larger than common-mode leakage
    REQUIRE(diffEnergy > cmEnergy * 100.0);
}

TEST_CASE("TransformerPrimitive: balanced differential equals 2x single-ended", "[Parts][TransformerPrimitive]") {
    // With hot=+A, cold=-A, differential = 2A
    // process(2A) should approximately equal processBalanced(A, -A)
    const int N = 4000;
    const int skip = 2000;
    double energySE = 0.0, energyBal = 0.0;

    {
        TransformerPrimitive t(TransformerPrimitive::API2520Output());
        t.prepare(kSR);
        for (int i = 0; i < N; ++i) {
            double sig = 0.1 * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
            double out = t.process(sig * 2.0, 0.0);
            if (i >= skip) energySE += out * out;
        }
    }
    {
        TransformerPrimitive t(TransformerPrimitive::API2520Output());
        t.prepare(kSR);
        for (int i = 0; i < N; ++i) {
            double sig = 0.1 * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
            double out = t.processBalanced(sig, -sig, 0.0);
            if (i >= skip) energyBal += out * out;
        }
    }

    // Should be approximately equal (CMRR leakage negligible for differential)
    double ratio = energyBal / std::max(energySE, 1e-30);
    REQUIRE(ratio > 0.8);
    REQUIRE(ratio < 1.2);
}

// ============================================================================
// Phase inversion
// ============================================================================

TEST_CASE("TransformerPrimitive: phase inversion flips polarity", "[Parts][TransformerPrimitive]") {
    TransformerPrimitive t(TransformerPrimitive::InterstageTriode()); // invertPhase = true
    t.prepare(kSR);

    // Warm up
    for (int i = 0; i < 2000; ++i)
        t.process(0.0);

    // Positive input should produce negative output (phase inverted)
    double out = t.process(0.5, 0.0);
    // Due to HP filter, first sample is tricky - run a few cycles
    t.reset();
    double sumProduct = 0.0;
    for (int i = 0; i < 4000; ++i) {
        double in = 0.1 * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
        double out = t.process(in, 0.0);
        if (i >= 2000) sumProduct += in * out;
    }
    // Inverted: input x output correlation should be negative
    REQUIRE(sumProduct < 0.0);
}

TEST_CASE("TransformerPrimitive: non-inverting preset has positive correlation", "[Parts][TransformerPrimitive]") {
    TransformerPrimitive t(TransformerPrimitive::API2520Output()); // invertPhase = false
    t.prepare(kSR);

    double sumProduct = 0.0;
    for (int i = 0; i < 4000; ++i) {
        double in = 0.1 * std::sin(2.0 * M_PI * 1000.0 * i / kSR);
        double out = t.process(in, 0.0);
        if (i >= 2000) sumProduct += in * out;
    }
    REQUIRE(sumProduct > 0.0);
}

// ============================================================================
// Temperature dependence / impedance
// ============================================================================

TEST_CASE("TransformerPrimitive: muScale varies with temperature", "[Parts][TransformerPrimitive]") {
    TransformerPrimitive t(TransformerPrimitive::Neve1073Input());
    double mu25 = t.muScale(25.0);
    double mu60 = t.muScale(60.0);
    REQUIRE(mu25 != mu60);
    REQUIRE_THAT(mu25, WithinAbs(1.0, 0.01));
}

TEST_CASE("TransformerPrimitive: impedanceRatio equals N squared", "[Parts][TransformerPrimitive]") {
    TransformerPrimitive t(TransformerPrimitive::Neve1073Input());
    REQUIRE_THAT(t.impedanceRatio(), WithinAbs(100.0, 1e-10)); // 10^2
}

TEST_CASE("TransformerPrimitive: DI box step-down impedance ratio", "[Parts][TransformerPrimitive]") {
    TransformerPrimitive t(TransformerPrimitive::JensenDIBox());
    REQUIRE_THAT(t.impedanceRatio(), WithinAbs(0.01, 1e-10)); // 0.1^2
}

// ============================================================================
// Stability
// ============================================================================

TEST_CASE("TransformerPrimitive: output finite for extreme inputs", "[Parts][TransformerPrimitive]") {
    TransformerPrimitive t(TransformerPrimitive::Neve1073Input());
    t.prepare(kSR);

    for (int i = 0; i < 500; ++i) {
        double out = t.process(10.0, 1.0);
        REQUIRE(std::isfinite(out));
    }
}

TEST_CASE("TransformerPrimitive: output finite for all presets under stress", "[Parts][TransformerPrimitive]") {
    auto stress = [](const TransformerPrimitive::Spec& s, const char* name) {
        TransformerPrimitive t(s);
        t.prepare(kSR);
        INFO("Preset: " << name);
        for (int i = 0; i < 2000; ++i) {
            double in = 5.0 * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            double out = t.process(in, 1.0, 60.0);
            REQUIRE(std::isfinite(out));
        }
    };
    stress(TransformerPrimitive::Neve1073Input(),    "Neve1073Input");
    stress(TransformerPrimitive::API2520Output(),    "API2520Output");
    stress(TransformerPrimitive::Lundahl1538(),      "Lundahl1538");
    stress(TransformerPrimitive::JensenDIBox(),      "JensenDIBox");
    stress(TransformerPrimitive::InterstageTriode(), "InterstageTriode");
    stress(TransformerPrimitive::RibbonMicOutput(),  "RibbonMicOutput");
}

TEST_CASE("TransformerPrimitive: resonance frequency is positive", "[Parts][TransformerPrimitive]") {
    TransformerPrimitive t(TransformerPrimitive::Neve1073Input());
    t.prepare(kSR);
    REQUIRE(t.getResonanceFreq() > 0.0);
    REQUIRE(t.getResonanceFreq() < kSR * 0.5);
}
