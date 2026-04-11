// ============================================================================
// InputFilter unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/circuits/drive/InputFilter.h"

using Catch::Approx;

static constexpr double kSampleRate = 44100.0;

TEST_CASE("InputFilter: prepare and reset without crash", "[InputFilter]") {
    InputFilter lpf;
    lpf.prepare(2, kSampleRate);
    lpf.reset();
    REQUIRE(true);
}

TEST_CASE("InputFilter: silence in -> silence out", "[InputFilter]") {
    InputFilter lpf;
    lpf.prepare(1, kSampleRate);
    lpf.setDefaultCutoff();

    float out = lpf.process(0, 0.0f);
    REQUIRE(std::fabs(out) < 0.001f);
}

TEST_CASE("InputFilter: passes DC (converges to 1.0)", "[InputFilter]") {
    InputFilter lpf;
    lpf.prepare(1, kSampleRate);
    lpf.setDefaultCutoff();

    float out = 0.0f;
    for (int i = 0; i < 5000; ++i) out = lpf.process(0, 1.0f);
    REQUIRE(out == Approx(1.0f).margin(0.01f));
}

TEST_CASE("InputFilter: attenuates high frequencies more than low", "[InputFilter]") {
    InputFilter lpf;
    lpf.prepare(1, kSampleRate);
    lpf.setCutoffHz(2000.0);

    double sumLow = 0.0, sumHigh = 0.0;
    const int N = 4000;

    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0f * 3.14159f * 500.0f * i / (float)kSampleRate);
        float out = lpf.process(0, in);
        if (i > 1000) sumLow += out * out;
    }
    lpf.reset();
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0f * 3.14159f * 15000.0f * i / (float)kSampleRate);
        float out = lpf.process(0, in);
        if (i > 1000) sumHigh += out * out;
    }

    REQUIRE(sumLow > sumHigh);
}

TEST_CASE("InputFilter: setCutoffHz changes filter behavior", "[InputFilter]") {
    InputFilter lpfNarrow, lpfWide;
    lpfNarrow.prepare(1, kSampleRate);
    lpfWide.prepare(1, kSampleRate);
    lpfNarrow.setCutoffHz(500.0);
    lpfWide.setCutoffHz(10000.0);

    double sumNarrow = 0.0, sumWide = 0.0;
    const int N = 4000;

    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0f * 3.14159f * 3000.0f * i / (float)kSampleRate);
        float outN = lpfNarrow.process(0, in);
        float outW = lpfWide.process(0, in);
        if (i > 1000) {
            sumNarrow += outN * outN;
            sumWide += outW * outW;
        }
    }

    REQUIRE(sumWide > sumNarrow);
}

TEST_CASE("InputFilter: processStereo produces same result as individual process", "[InputFilter]") {
    InputFilter lpfStereo, lpfMono;
    lpfStereo.prepare(2, kSampleRate);
    lpfMono.prepare(2, kSampleRate);
    lpfStereo.setDefaultCutoff();
    lpfMono.setDefaultCutoff();

    float ch0_s = 0.5f, ch1_s = -0.3f;
    float ch0_m = 0.5f, ch1_m = -0.3f;

    // Process a number of samples to let any initialization differences settle
    for (int i = 0; i < 100; ++i) {
        ch0_s = 0.5f; ch1_s = -0.3f;
        lpfStereo.processStereo(ch0_s, ch1_s);
        ch0_m = lpfMono.process(0, 0.5f);
        ch1_m = lpfMono.process(1, -0.3f);
    }

    // Both paths should produce similar results
    REQUIRE(ch0_s == Approx(ch0_m).margin(0.001f));
    REQUIRE(ch1_s == Approx(ch1_m).margin(0.001f));
}

TEST_CASE("InputFilter: getCurrentCutoffHz reports set value", "[InputFilter]") {
    InputFilter lpf;
    lpf.prepare(1, kSampleRate);
    lpf.setCutoffHz(3000.0);
    REQUIRE(lpf.getCurrentCutoffHz() == Approx(3000.0));
}

TEST_CASE("InputFilter: invalid channel returns input", "[InputFilter]") {
    InputFilter lpf;
    lpf.prepare(1, kSampleRate);

    float out = lpf.process(-1, 0.5f);
    REQUIRE(out == 0.5f);

    out = lpf.process(5, 0.5f);
    REQUIRE(out == 0.5f);
}
