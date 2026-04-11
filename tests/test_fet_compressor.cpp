// ============================================================================
// FetCompressor (FET compressor) unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/dynamics/FetCompressor.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("FET: prepare and reset without crash", "[FET]") {
    FetCompressor fc;
    fc.prepare(2, kSR);
    fc.reset();
    REQUIRE(true);
}

TEST_CASE("FET: prepare with ProcessSpec", "[FET]") {
    FetCompressor fc;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    fc.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("FET: silence in -> silence out", "[FET]") {
    FetCompressor fc;
    fc.prepare(1, kSR);
    FetCompressor::Params p;
    float out = 0.0f;
    for (int i = 0; i < 1000; ++i)
        out = fc.process(0, 0.0f, p);
    REQUIRE(std::fabs(out) < 1e-6f);
}

TEST_CASE("FET: 4:1 ratio compresses loud signal", "[FET]") {
    FetCompressor fc;
    fc.prepare(1, kSR);
    FetCompressor::Params p;
    p.inputGain = 0.7f;
    p.outputGain = 0.5f;
    p.attack = 0.3f;
    p.release = 0.3f;
    p.ratio = 0;  // 4:1
    p.mix = 1.0f;

    for (int i = 0; i < 8000; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        fc.process(0, in, p);
    }

    float gr = fc.getGainReductionDb(0);
    REQUIRE(gr < 0.0f);
}

TEST_CASE("FET: higher ratio compresses more", "[FET]") {
    const int N = 8000;

    auto runAndGetGR = [&](int ratio) {
        FetCompressor fc;
        fc.prepare(1, kSR);
        FetCompressor::Params p;
        p.inputGain = 0.7f;
        p.outputGain = 0.5f;
        p.attack = 0.3f;
        p.release = 0.3f;
        p.ratio = ratio;
        p.mix = 1.0f;
        for (int i = 0; i < N; ++i) {
            float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            fc.process(0, in, p);
        }
        return fc.getGainReductionDb(0);
    };

    float gr4  = runAndGetGR(0);   // 4:1
    float gr20 = runAndGetGR(3);   // 20:1
    REQUIRE(gr20 < gr4);
}

TEST_CASE("FET: All-Buttons mode applies extreme compression", "[FET]") {
    FetCompressor fc;
    fc.prepare(1, kSR);
    FetCompressor::Params p;
    p.inputGain = 0.8f;
    p.outputGain = 0.5f;
    p.attack = 0.1f;
    p.release = 0.3f;
    p.ratio = 4;  // All-Buttons
    p.mix = 1.0f;

    for (int i = 0; i < 8000; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        fc.process(0, in, p);
    }

    float gr = fc.getGainReductionDb(0);
    REQUIRE(gr < -3.0f);
}

TEST_CASE("FET: fast attack responds quickly", "[FET]") {
    FetCompressor fc;
    fc.prepare(1, kSR);
    FetCompressor::Params p;
    p.inputGain = 0.8f;
    p.outputGain = 0.5f;
    p.attack = 0.0f;  // fastest (20us)
    p.release = 0.5f;
    p.ratio = 2;  // 12:1
    p.mix = 1.0f;

    // Feed sudden transient
    for (int i = 0; i < 200; ++i) {
        float in = 0.9f;
        fc.process(0, in, p);
    }

    float gr = fc.getGainReductionDb(0);
    // Fast attack should already have significant GR in 200 samples (~4.5ms)
    REQUIRE(gr < -1.0f);
}

TEST_CASE("FET: input gain drives into compression", "[FET]") {
    const int N = 8000;

    auto runAndGetGR = [&](float inputG) {
        FetCompressor fc;
        fc.prepare(1, kSR);
        FetCompressor::Params p;
        p.inputGain = inputG;
        p.outputGain = 0.5f;
        p.attack = 0.3f;
        p.release = 0.3f;
        p.ratio = 0;
        p.mix = 1.0f;
        for (int i = 0; i < N; ++i) {
            float in = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            fc.process(0, in, p);
        }
        return fc.getGainReductionDb(0);
    };

    float grLow  = runAndGetGR(0.2f);
    float grHigh = runAndGetGR(0.9f);
    REQUIRE(grHigh < grLow);
}

TEST_CASE("FET: output stays bounded", "[FET]") {
    FetCompressor fc;
    fc.prepare(1, kSR);
    FetCompressor::Params p;
    p.inputGain = 1.0f;
    p.outputGain = 1.0f;
    p.attack = 0.0f;
    p.release = 0.0f;
    p.ratio = 4;
    p.mix = 1.0f;

    float maxAbs = 0.0f;
    for (int i = 0; i < 16000; ++i) {
        float in = 0.9f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = fc.process(0, in, p);
        maxAbs = std::max(maxAbs, std::fabs(out));
    }
    REQUIRE(maxAbs < 50.0f);  // bounded, not infinite
    REQUIRE(std::isfinite(maxAbs));
}

TEST_CASE("FET: processBlock matches per-sample", "[FET]") {
    FetCompressor f1, f2;
    f1.prepare(1, kSR);
    f2.prepare(1, kSR);

    FetCompressor::Params p;
    p.inputGain = 0.5f;
    p.outputGain = 0.5f;
    p.attack = 0.3f;
    p.release = 0.3f;
    p.ratio = 0;
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

TEST_CASE("FET: getGainReductionDb returns valid range", "[FET]") {
    FetCompressor fc;
    fc.prepare(1, kSR);
    FetCompressor::Params p;
    p.inputGain = 0.5f;
    p.outputGain = 0.5f;
    p.ratio = 0;
    p.mix = 1.0f;

    float gr0 = fc.getGainReductionDb(0);
    REQUIRE(gr0 <= 0.0f);

    for (int i = 0; i < 8000; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        fc.process(0, in, p);
    }
    float gr1 = fc.getGainReductionDb(0);
    REQUIRE(gr1 >= -60.0f);
    REQUIRE(gr1 <= 0.0f);
}

TEST_CASE("FET: multichannel independence", "[FET]") {
    FetCompressor fc;
    fc.prepare(2, kSR);
    FetCompressor::Params p;
    p.inputGain = 0.7f;
    p.outputGain = 0.5f;
    p.ratio = 0;
    p.mix = 1.0f;

    for (int i = 0; i < 8000; ++i) {
        float in = 0.8f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        fc.process(0, in, p);
        fc.process(1, 0.0f, p);
    }

    float gr0 = fc.getGainReductionDb(0);
    float gr1 = fc.getGainReductionDb(1);
    REQUIRE(gr0 < gr1);
}
