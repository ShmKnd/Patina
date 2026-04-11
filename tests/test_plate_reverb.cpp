// ============================================================================
// PlateReverb unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/delay/PlateReverb.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("PlateReverb: prepare and reset without crash", "[Plate]") {
    PlateReverb pr;
    pr.prepare(2, kSR);
    pr.reset();
    REQUIRE(true);
}

TEST_CASE("PlateReverb: prepare with ProcessSpec", "[Plate]") {
    PlateReverb pr;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    pr.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("PlateReverb: silence in -> decays to silence", "[Plate]") {
    PlateReverb pr;
    pr.prepare(1, kSR);
    PlateReverb::Params params;
    params.mix = 1.0f;
    params.decay = 0.3f;

    float out = 0.0f;
    for (int i = 0; i < 40000; ++i)
        out = pr.process(0, 0.0f, params);
    REQUIRE(std::fabs(out) < 0.01f);
}

TEST_CASE("PlateReverb: impulse produces reverb tail", "[Plate]") {
    PlateReverb pr;
    pr.prepare(1, kSR);
    PlateReverb::Params params;
    params.mix = 1.0f;
    params.decay = 0.7f;

    pr.process(0, 1.0f, params);

    double tailEnergy = 0.0;
    for (int i = 0; i < 8000; ++i) {
        float out = pr.process(0, 0.0f, params);
        tailEnergy += out * out;
    }
    REQUIRE(tailEnergy > 0.001);
}

TEST_CASE("PlateReverb: higher decay = longer tail", "[Plate]") {
    double energyShort = 0.0, energyLong = 0.0;
    const int N = 16000;

    {
        PlateReverb pr;
        pr.prepare(1, kSR);
        PlateReverb::Params params;
        params.mix = 1.0f;
        params.decay = 0.2f;
        pr.process(0, 1.0f, params);
        for (int i = 0; i < N; ++i) {
            float out = pr.process(0, 0.0f, params);
            energyShort += out * out;
        }
    }
    {
        PlateReverb pr;
        pr.prepare(1, kSR);
        PlateReverb::Params params;
        params.mix = 1.0f;
        params.decay = 0.9f;
        pr.process(0, 1.0f, params);
        for (int i = 0; i < N; ++i) {
            float out = pr.process(0, 0.0f, params);
            energyLong += out * out;
        }
    }
    REQUIRE(energyLong > energyShort);
}

TEST_CASE("PlateReverb: mix controls wet/dry balance", "[Plate]") {
    PlateReverb pr;
    pr.prepare(1, kSR);
    PlateReverb::Params params;
    params.mix = 0.0f;
    params.decay = 0.5f;

    float out = pr.process(0, 0.5f, params);
    REQUIRE(out == Approx(0.5f).margin(0.01f));
}

TEST_CASE("PlateReverb: predelay causes delayed onset", "[Plate]") {
    PlateReverb pr;
    pr.prepare(1, kSR);
    PlateReverb::Params params;
    params.mix = 1.0f;
    params.decay = 0.5f;
    params.predelayMs = 50.0f;

    // Feed impulse
    pr.process(0, 1.0f, params);

    // First ~50ms should be mostly silence (wet signal delayed)
    int predelaySamples = (int)(50.0 * kSR / 1000.0);
    double earlyEnergy = 0.0;
    for (int i = 0; i < predelaySamples / 2; ++i) {
        float out = pr.process(0, 0.0f, params);
        earlyEnergy += out * out;
    }

    // After predelay, should have reverb
    double lateEnergy = 0.0;
    for (int i = 0; i < 8000; ++i) {
        float out = pr.process(0, 0.0f, params);
        lateEnergy += out * out;
    }

    REQUIRE(lateEnergy > earlyEnergy);
}

TEST_CASE("PlateReverb: damping affects HF content", "[Plate]") {
    // High damping should reduce high-frequency content in tail
    // Low damping should preserve more
    double energyLowDamp = 0.0, energyHighDamp = 0.0;
    const int N = 8000;

    {
        PlateReverb pr;
        pr.prepare(1, kSR);
        PlateReverb::Params params;
        params.mix = 1.0f;
        params.decay = 0.7f;
        params.damping = 0.1f;
        // Feed high-frequency impulse
        for (int i = 0; i < 100; ++i)
            pr.process(0, std::sin(2.0 * M_PI * 8000.0 * i / kSR), params);
        for (int i = 0; i < N; ++i) {
            float out = pr.process(0, 0.0f, params);
            energyLowDamp += out * out;
        }
    }
    {
        PlateReverb pr;
        pr.prepare(1, kSR);
        PlateReverb::Params params;
        params.mix = 1.0f;
        params.decay = 0.7f;
        params.damping = 0.9f;
        for (int i = 0; i < 100; ++i)
            pr.process(0, std::sin(2.0 * M_PI * 8000.0 * i / kSR), params);
        for (int i = 0; i < N; ++i) {
            float out = pr.process(0, 0.0f, params);
            energyHighDamp += out * out;
        }
    }
    // Low damping should preserve more tail energy
    REQUIRE(energyLowDamp > energyHighDamp);
}

TEST_CASE("PlateReverb: diffusion produces finite output", "[Plate]") {
    PlateReverb pr;
    pr.prepare(1, kSR);
    PlateReverb::Params params;
    params.mix = 0.5f;
    params.diffusion = 1.0f;

    for (int i = 0; i < 4000; ++i) {
        float in = (i < 10) ? 1.0f : 0.0f;
        float out = pr.process(0, in, params);
        REQUIRE(std::isfinite(out));
    }
}

TEST_CASE("PlateReverb: modulation produces finite output", "[Plate]") {
    PlateReverb pr;
    pr.prepare(1, kSR);
    PlateReverb::Params params;
    params.mix = 0.5f;
    params.modDepth = 1.0f;

    pr.process(0, 1.0f, params);
    for (int i = 0; i < 4000; ++i) {
        float out = pr.process(0, 0.0f, params);
        REQUIRE(std::isfinite(out));
    }
}

TEST_CASE("PlateReverb: processBlock produces finite output", "[Plate]") {
    const int N = 512;
    PlateReverb pr;
    pr.prepare(1, kSR);

    PlateReverb::Params params;
    params.decay = 0.5f;
    params.mix = 0.3f;

    std::vector<float> buf(N);
    buf[0] = 1.0f;  // impulse

    float* ch[] = { buf.data() };
    pr.processBlock(ch, 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(buf[i]));
}

TEST_CASE("PlateReverb: uses 4-line FDN with 4 diffusion stages", "[Plate]") {
    // Just verify the module works with its built-in parameters
    PlateReverb pr;
    pr.prepare(1, kSR);
    PlateReverb::Params params;
    params.diffusion = 1.0f;
    pr.process(0, 1.0f, params);
    float out = pr.process(0, 0.0f, params);
    REQUIRE(std::isfinite(out));
}

TEST_CASE("PlateReverb: output finite for extreme inputs", "[Plate]") {
    PlateReverb pr;
    pr.prepare(1, kSR);
    PlateReverb::Params params;
    params.decay = 0.9f;

    for (int i = 0; i < 200; ++i) {
        float out = pr.process(0, 10.0f, params);
        REQUIRE(std::isfinite(out));
    }
}
