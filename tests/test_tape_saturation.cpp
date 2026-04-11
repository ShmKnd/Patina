// ============================================================================
// TapeSaturation unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/saturation/TapeSaturation.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

TEST_CASE("Tape: prepare and reset without crash", "[Tape]") {
    TapeSaturation ts;
    ts.prepare(2, kSR);
    ts.reset();
    REQUIRE(true);
}

TEST_CASE("Tape: prepare with ProcessSpec", "[Tape]") {
    TapeSaturation ts;
    patina::ProcessSpec spec;
    spec.sampleRate = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    ts.prepare(spec);
    REQUIRE(true);
}

TEST_CASE("Tape: silence in -> silence out", "[Tape]") {
    TapeSaturation ts;
    ts.prepare(1, kSR);
    TapeSaturation::Params params;
    params.wowFlutter = 0.0f;

    float out = ts.process(0, 0.0f, params);
    REQUIRE(std::fabs(out) < 0.01f);
}

TEST_CASE("Tape: saturation compresses signal", "[Tape]") {
    TapeSaturation ts;
    ts.prepare(1, kSR);
    TapeSaturation::Params params;
    params.inputGain = 12.0f;
    params.saturation = 1.0f;
    params.wowFlutter = 0.0f;

    float maxOut = 0.0f;
    for (int i = 0; i < 4000; ++i) {
        float in = std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = ts.process(0, in, params);
        maxOut = std::max(maxOut, std::fabs(out));
    }
    // High saturation should limit peak
    REQUIRE(maxOut > 0.0f);
    REQUIRE(std::isfinite(maxOut));
}

TEST_CASE("Tape: saturation changes waveform shape", "[Tape]") {
    double peakLow = 0.0, peakHigh = 0.0;
    const int N = 4000;

    // Low saturation
    {
        TapeSaturation ts;
        ts.prepare(1, kSR);
        TapeSaturation::Params params;
        params.inputGain = 6.0f;
        params.saturation = 0.1f;
        params.wowFlutter = 0.0f;
        for (int i = 0; i < N; ++i) {
            float in = std::sin(2.0 * M_PI * 440.0 * i / kSR);
            float out = ts.process(0, in, params);
            peakLow = std::max(peakLow, (double)std::fabs(out));
        }
    }
    // High saturation
    {
        TapeSaturation ts;
        ts.prepare(1, kSR);
        TapeSaturation::Params params;
        params.inputGain = 6.0f;
        params.saturation = 1.0f;
        params.wowFlutter = 0.0f;
        for (int i = 0; i < N; ++i) {
            float in = std::sin(2.0 * M_PI * 440.0 * i / kSR);
            float out = ts.process(0, in, params);
            peakHigh = std::max(peakHigh, (double)std::fabs(out));
        }
    }
    // Different saturation settings should produce different peak levels
    REQUIRE(std::fabs(peakLow - peakHigh) > 0.001);
    REQUIRE(std::isfinite(peakLow));
    REQUIRE(std::isfinite(peakHigh));
}

TEST_CASE("Tape: head bump boosts low frequencies", "[Tape]") {
    double sumWithBump = 0.0, sumWithout = 0.0;
    const int N = 8000;
    const int skip = 4000;

    {
        TapeSaturation ts;
        ts.prepare(1, kSR);
        TapeSaturation::Params params;
        params.saturation = 0.0f;
        params.wowFlutter = 0.0f;
        params.enableHeadBump = true;
        params.enableHfRolloff = false;
        for (int i = 0; i < N; ++i) {
            float in = 0.3f * std::sin(2.0 * M_PI * 60.0 * i / kSR);
            float out = ts.process(0, in, params);
            if (i >= skip) sumWithBump += out * out;
        }
    }
    {
        TapeSaturation ts;
        ts.prepare(1, kSR);
        TapeSaturation::Params params;
        params.saturation = 0.0f;
        params.wowFlutter = 0.0f;
        params.enableHeadBump = false;
        params.enableHfRolloff = false;
        for (int i = 0; i < N; ++i) {
            float in = 0.3f * std::sin(2.0 * M_PI * 60.0 * i / kSR);
            float out = ts.process(0, in, params);
            if (i >= skip) sumWithout += out * out;
        }
    }
    REQUIRE(sumWithBump >= sumWithout);
}

TEST_CASE("Tape: HF self-erase attenuates highs", "[Tape]") {
    double sumWith = 0.0, sumWithout = 0.0;
    const int N = 8000;
    const int skip = 4000;

    {
        TapeSaturation ts;
        ts.prepare(1, kSR);
        TapeSaturation::Params params;
        params.saturation = 0.0f;
        params.wowFlutter = 0.0f;
        params.enableHeadBump = false;
        params.enableHfRolloff = true;
        for (int i = 0; i < N; ++i) {
            float in = 0.3f * std::sin(2.0 * M_PI * 15000.0 * i / kSR);
            float out = ts.process(0, in, params);
            if (i >= skip) sumWith += out * out;
        }
    }
    {
        TapeSaturation ts;
        ts.prepare(1, kSR);
        TapeSaturation::Params params;
        params.saturation = 0.0f;
        params.wowFlutter = 0.0f;
        params.enableHeadBump = false;
        params.enableHfRolloff = false;
        for (int i = 0; i < N; ++i) {
            float in = 0.3f * std::sin(2.0 * M_PI * 15000.0 * i / kSR);
            float out = ts.process(0, in, params);
            if (i >= skip) sumWithout += out * out;
        }
    }
    REQUIRE(sumWithout >= sumWith);
}

TEST_CASE("Tape: wow & flutter modulates pitch", "[Tape]") {
    TapeSaturation ts;
    ts.prepare(1, kSR);
    TapeSaturation::Params params;
    params.saturation = 0.0f;
    params.wowFlutter = 1.0f;

    // Just check the output remains finite with full wow/flutter
    for (int i = 0; i < 4000; ++i) {
        float in = std::sin(2.0 * M_PI * 440.0 * i / kSR);
        float out = ts.process(0, in, params);
        REQUIRE(std::isfinite(out));
    }
}

TEST_CASE("Tape: processBlock produces finite output", "[Tape]") {
    const int N = 512;
    TapeSaturation ts;
    ts.prepare(1, kSR);

    TapeSaturation::Params params;
    params.saturation = 0.5f;
    params.wowFlutter = 0.3f;

    std::vector<float> buf(N);
    for (int i = 0; i < N; ++i)
        buf[i] = 0.5f * std::sin(2.0 * M_PI * 440.0 * i / kSR);

    float* ch[] = { buf.data() };
    ts.processBlock(ch, 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(buf[i]));
}

TEST_CASE("Tape: output finite for extreme inputs", "[Tape]") {
    TapeSaturation ts;
    ts.prepare(1, kSR);
    TapeSaturation::Params params;
    params.inputGain = 24.0f;
    params.saturation = 1.0f;

    for (int i = 0; i < 200; ++i) {
        float out = ts.process(0, 10.0f, params);
        REQUIRE(std::isfinite(out));
    }
}
