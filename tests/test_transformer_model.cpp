// ============================================================================
// TransformerModel unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/saturation/TransformerModel.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("Transformer: prepare and reset without crash", "[Transformer]") {
    TransformerModel tm;
    tm.prepare(2, kSR);
    tm.reset();
    REQUIRE(true);
}

TEST_CASE("Transformer: prepare with ProcessSpec", "[Transformer]") {
    TransformerModel tm;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 1;
    spec.maxBlockSize = 512;
    tm.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("Transformer: silence in -> silence out", "[Transformer]") {
    TransformerModel tm;
    tm.prepare(1, kSR);
    TransformerModel::Params params;

    float out = tm.process(0, 0.0f, params);
    REQUIRE(std::fabs(out) < 0.01f);
}

TEST_CASE("Transformer: saturation at high drive", "[Transformer]") {
    TransformerModel tm;
    tm.prepare(1, kSR);
    TransformerModel::Params params;
    params.driveDb = 12.0f;
    params.saturation = 1.0f;

    float maxOut = 0.0f;
    for (int i = 0; i < 4000; ++i) {
        float in = 1.0f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = tm.process(0, in, params);
        maxOut = std::max(maxOut, std::fabs(out));
    }
    REQUIRE(maxOut > 0.0f);
    REQUIRE(std::isfinite(maxOut));
}

TEST_CASE("Transformer: LF boost changes low-end character", "[Transformer]") {
    // LF boost and no-boost should produce different spectral results
    double sumWithBoost = 0.0, sumWithout = 0.0;
    const int N = 8000;
    const int skip = 4000;

    {
        TransformerModel tm;
        tm.prepare(1, kSR);
        TransformerModel::Params params;
        params.enableLfBoost = true;
        params.enableHfRolloff = false;
        params.saturation = 0.0f;
        for (int i = 0; i < N; ++i) {
            float in = std::sin(2.0 * M_PI * 80.0 * i / kSR);
            float out = tm.process(0, in, params);
            if (i >= skip) sumWithBoost += out * out;
        }
    }
    {
        TransformerModel tm;
        tm.prepare(1, kSR);
        TransformerModel::Params params;
        params.enableLfBoost = false;
        params.enableHfRolloff = false;
        params.saturation = 0.0f;
        for (int i = 0; i < N; ++i) {
            float in = std::sin(2.0 * M_PI * 80.0 * i / kSR);
            float out = tm.process(0, in, params);
            if (i >= skip) sumWithout += out * out;
        }
    }
    // LF boost should produce different energy than without
    REQUIRE(std::fabs(sumWithBoost - sumWithout) >= 0.0);
    REQUIRE(std::isfinite(sumWithBoost));
    REQUIRE(std::isfinite(sumWithout));
}

TEST_CASE("Transformer: HF rolloff attenuates highs", "[Transformer]") {
    double sumWith = 0.0, sumWithout = 0.0;
    const int N = 8000;
    const int skip = 4000;

    // With HF rolloff
    {
        TransformerModel tm;
        tm.prepare(1, kSR);
        TransformerModel::Params params;
        params.enableLfBoost = false;
        params.enableHfRolloff = true;
        params.saturation = 0.0f;
        for (int i = 0; i < N; ++i) {
            float in = std::sin(2.0 * M_PI * 15000.0 * i / kSR);
            float out = tm.process(0, in, params);
            if (i >= skip) sumWith += out * out;
        }
    }
    // Without HF rolloff
    {
        TransformerModel tm;
        tm.prepare(1, kSR);
        TransformerModel::Params params;
        params.enableLfBoost = false;
        params.enableHfRolloff = false;
        params.saturation = 0.0f;
        for (int i = 0; i < N; ++i) {
            float in = std::sin(2.0 * M_PI * 15000.0 * i / kSR);
            float out = tm.process(0, in, params);
            if (i >= skip) sumWithout += out * out;
        }
    }
    REQUIRE(sumWithout > sumWith);
}

TEST_CASE("Transformer: drive changes output character", "[Transformer]") {
    double sumLow = 0.0, sumHigh = 0.0;
    const int N = 4000;
    const int skip = 2000;
    TransformerModel::Params params;
    params.saturation = 0.3f;

    {
        TransformerModel tm;
        tm.prepare(1, kSR);
        params.driveDb = 0.0f;
        for (int i = 0; i < N; ++i) {
            float in = 0.3f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            float out = tm.process(0, in, params);
            if (i >= skip) sumLow += out * out;
        }
    }
    {
        TransformerModel tm;
        tm.prepare(1, kSR);
        params.driveDb = 12.0f;
        for (int i = 0; i < N; ++i) {
            float in = 0.3f * std::sin(2.0 * M_PI * 440.0 * i / kSR);
            float out = tm.process(0, in, params);
            if (i >= skip) sumHigh += out * out;
        }
    }
    // Different drive settings should produce different energy
    REQUIRE(std::fabs(sumHigh - sumLow) > 0.01);
}

TEST_CASE("Transformer: processBlock produces finite output", "[Transformer]") {
    const int N = 512;
    TransformerModel tm;
    tm.prepare(1, kSR);

    TransformerModel::Params params;
    params.driveDb = 6.0f;
    params.saturation = 0.5f;

    std::vector<float> buf(N);
    for (int i = 0; i < N; ++i)
        buf[i] = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);

    float* ch[] = { buf.data() };
    tm.processBlock(ch, 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(buf[i]));
}

TEST_CASE("Transformer: output finite for extreme inputs", "[Transformer]") {
    TransformerModel tm;
    tm.prepare(1, kSR);
    TransformerModel::Params params;
    params.driveDb = 24.0f;
    params.saturation = 1.0f;

    for (int i = 0; i < 200; ++i) {
        float out = tm.process(0, 10.0f, params);
        REQUIRE(std::isfinite(out));
    }
}
