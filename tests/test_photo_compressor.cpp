// ============================================================================
// PhotoCompressor (photo-opto) unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/dynamics/PhotoCompressor.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("Photo: prepare and reset without crash", "[Photo]") {
    PhotoCompressor pc;
    pc.prepare(2, kSR);
    pc.reset();
    REQUIRE(true);
}

TEST_CASE("Photo: prepare with ProcessSpec", "[Photo]") {
    PhotoCompressor pc;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    pc.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("Photo: silence in -> silence out", "[Photo]") {
    PhotoCompressor pc;
    pc.prepare(1, kSR);
    PhotoCompressor::Params p;
    float out = 0.0f;
    for (int i = 0; i < 1000; ++i)
        out = pc.process(0, 0.0f, p);
    REQUIRE(std::fabs(out) < 1e-6f);
}

TEST_CASE("Photo: Compress mode reduces loud signal", "[Photo]") {
    PhotoCompressor pc;
    pc.prepare(1, kSR);
    PhotoCompressor::Params p;
    p.peakReduction = 0.7f;
    p.outputGain = 0.5f;
    p.mode = 0; // Compress
    p.mix = 1.0f;

    // Feed loud signal
    float lastOut = 0.0f;
    for (int i = 0; i < 8000; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        lastOut = pc.process(0, in, p);
    }

    // GR should be negative (compression happening)
    float gr = pc.getGainReductionDb(0);
    REQUIRE(gr < 0.0f);
}

TEST_CASE("Photo: Limit mode compresses harder", "[Photo]") {
    const int N = 8000;

    auto runAndGetGR = [&](int mode) {
        PhotoCompressor pc;
        pc.prepare(1, kSR);
        PhotoCompressor::Params p;
        p.peakReduction = 0.7f;
        p.outputGain = 0.5f;
        p.mode = mode;
        p.mix = 1.0f;
        for (int i = 0; i < N; ++i) {
            float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            pc.process(0, in, p);
        }
        return pc.getGainReductionDb(0);
    };

    float grCompress = runAndGetGR(0);
    float grLimit    = runAndGetGR(1);
    // Limit mode should compress more (more negative GR)
    REQUIRE(grLimit < grCompress);
}

TEST_CASE("Photo: no compression with zero peakReduction", "[Photo]") {
    PhotoCompressor pc;
    pc.prepare(1, kSR);
    PhotoCompressor::Params p;
    p.peakReduction = 0.0f;
    p.outputGain = 0.5f;  // makeup = 0.25x4 = 1.0
    p.mode = 0;
    p.mix = 1.0f;

    for (int i = 0; i < 4000; ++i) {
        float in = 0.3f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        pc.process(0, in, p);
    }

    float gr = pc.getGainReductionDb(0);
    // With peakReduction=0, minimal compression (sensitivity still > 0)
    REQUIRE(gr > -6.0f);
}

TEST_CASE("Photo: memory effect slows release over time", "[Photo]") {
    // T4B CdS memory effect: longer compression -> slower release
    auto measureReleaseGR = [](int compressionSamples) {
        PhotoCompressor pc;
        pc.prepare(1, kSR);
        PhotoCompressor::Params p;
        p.peakReduction = 0.8f;
        p.outputGain = 0.5f;
        p.mode = 0;
        p.mix = 1.0f;

        // Compression burst
        for (int i = 0; i < compressionSamples; ++i) {
            float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            pc.process(0, in, p);
        }

        // Release (silence) - 1 second to allow decay below clamp region
        for (int i = 0; i < 44100; ++i)
            pc.process(0, 0.0f, p);

        return pc.getGainReductionDb(0);
    };

    float grShortBurst = measureReleaseGR(4000);   // ~90ms
    float grLongBurst  = measureReleaseGR(88200);  // 2 seconds

    // Long burst charges CdS memory more -> slower release -> more GR remaining
    REQUIRE(grLongBurst < grShortBurst);
}

TEST_CASE("Photo: dry/wet mix works", "[Photo]") {
    PhotoCompressor pc;
    pc.prepare(1, kSR);

    PhotoCompressor::Params pWet, pDry;
    pWet.peakReduction = 0.8f;
    pWet.outputGain = 0.5f;
    pWet.mode = 0;
    pWet.mix = 1.0f;

    pDry = pWet;
    pDry.mix = 0.0f;

    const float input = 0.8f;

    // Warm up
    for (int i = 0; i < 4000; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        pc.process(0, in, pWet);
    }

    // Wet vs Dry
    float outWet = pc.process(0, input, pWet);
    pc.reset();
    for (int i = 0; i < 100; ++i)
        pc.process(0, input, pDry);
    float outDry = pc.process(0, input, pDry);

    // They should differ (dry bypasses compression)
    REQUIRE(std::fabs(outWet - outDry) > 0.001f);
}

TEST_CASE("Photo: processBlock matches per-sample", "[Photo]") {
    PhotoCompressor f1, f2;
    f1.prepare(1, kSR);
    f2.prepare(1, kSR);

    PhotoCompressor::Params p;
    p.peakReduction = 0.5f;
    p.outputGain = 0.5f;
    p.mode = 0;
    p.mix = 1.0f;

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

TEST_CASE("Photo: getGainReductionDb returns valid range", "[Photo]") {
    PhotoCompressor pc;
    pc.prepare(1, kSR);
    PhotoCompressor::Params p;
    p.peakReduction = 0.5f;
    p.outputGain = 0.5f;
    p.mode = 0;
    p.mix = 1.0f;

    // Initial state: no compression
    float gr0 = pc.getGainReductionDb(0);
    REQUIRE(gr0 <= 0.0f);

    // After signal
    for (int i = 0; i < 8000; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        pc.process(0, in, p);
    }
    float gr1 = pc.getGainReductionDb(0);
    REQUIRE(gr1 >= -60.0f);
    REQUIRE(gr1 <= 0.0f);
}

TEST_CASE("Photo: multichannel independence", "[Photo]") {
    PhotoCompressor pc;
    pc.prepare(2, kSR);
    PhotoCompressor::Params p;
    p.peakReduction = 0.7f;
    p.outputGain = 0.5f;
    p.mode = 0;
    p.mix = 1.0f;

    for (int i = 0; i < 8000; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        pc.process(0, in, p);
        pc.process(1, 0.0f, p);
    }

    float gr0 = pc.getGainReductionDb(0);
    float gr1 = pc.getGainReductionDb(1);
    REQUIRE(gr0 < gr1);  // ch0 should have more GR
}
