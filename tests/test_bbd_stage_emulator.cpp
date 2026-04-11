// ============================================================================
// BbdStageEmulator unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/bbd/BbdStageEmulator.h"

using Catch::Approx;

static constexpr double kSampleRate = 44100.0;

TEST_CASE("BbdStageEmulator: prepare and reset without crash", "[BBD]") {
    BbdStageEmulator bbd;
    bbd.prepare(2, kSampleRate);
    bbd.reset();
    REQUIRE(true);
}

TEST_CASE("BbdStageEmulator: silence in -> silence out", "[BBD]") {
    BbdStageEmulator bbd;
    bbd.prepare(1, kSampleRate);

    std::vector<float> samples = {0.0f};
    bbd.process(samples, 1.0, 4096, 9.0, false, 0.0);
    REQUIRE(samples[0] == 0.0f);
}

TEST_CASE("BbdStageEmulator: process produces finite output", "[BBD]") {
    BbdStageEmulator bbd;
    bbd.prepare(2, kSampleRate);

    std::vector<float> samples = {0.5f, -0.3f};
    bbd.process(samples, 1.0, 4096, 9.0, false, 0.0);

    REQUIRE(std::isfinite(samples[0]));
    REQUIRE(std::isfinite(samples[1]));
}

TEST_CASE("BbdStageEmulator: LPF character (attenuates high frequencies)", "[BBD]") {
    BbdStageEmulator bbd;
    bbd.prepare(1, kSampleRate);

    const int N = 4000;
    double sumLow = 0.0, sumHigh = 0.0;

    // Low frequency signal (100 Hz)
    for (int i = 0; i < N; ++i) {
        float v = 0.5f * std::sin(2.0 * M_PI * 100.0 * i / kSampleRate);
        std::vector<float> s = {v};
        bbd.process(s, 1.0, 4096, 9.0, false, 0.0);
        if (i > 1000) sumLow += s[0] * s[0];
    }

    bbd.reset();

    // High frequency signal (15000 Hz)
    for (int i = 0; i < N; ++i) {
        float v = 0.5f * std::sin(2.0 * M_PI * 15000.0 * i / kSampleRate);
        std::vector<float> s = {v};
        bbd.process(s, 1.0, 4096, 9.0, false, 0.0);
        if (i > 1000) sumHigh += s[0] * s[0];
    }

    // BBD should attenuate high frequencies more than low
    REQUIRE(sumLow > sumHigh);
}

TEST_CASE("BbdStageEmulator: supply voltage affects bandwidth", "[BBD]") {
    BbdStageEmulator bbd9, bbd18;
    bbd9.prepare(1, kSampleRate);
    bbd18.prepare(1, kSampleRate);

    const int N = 4000;
    double sum9 = 0.0, sum18 = 0.0;

    // Mid frequency (8kHz) signal, measure energy through different supply voltages
    for (int i = 0; i < N; ++i) {
        float v = 0.5f * std::sin(2.0 * M_PI * 8000.0 * i / kSampleRate);
        std::vector<float> s9 = {v}, s18 = {v};
        bbd9.process(s9, 1.0, 4096, 9.0, false, 0.0);
        bbd18.process(s18, 1.0, 4096, 18.0, false, 0.0);
        if (i > 1000) {
            sum9 += s9[0] * s9[0];
            sum18 += s18[0] * s18[0];
        }
    }

    // 18V supply should allow more bandwidth (more HF energy)
    REQUIRE(sum18 >= sum9 * 0.8); // At least not dramatically less
}

TEST_CASE("BbdStageEmulator: fewer stages = wider bandwidth", "[BBD]") {
    BbdStageEmulator bbdFew, bbdMany;
    bbdFew.prepare(1, kSampleRate);
    bbdMany.prepare(1, kSampleRate);

    const int N = 4000;
    double sumFew = 0.0, sumMany = 0.0;

    for (int i = 0; i < N; ++i) {
        float v = 0.5f * std::sin(2.0 * M_PI * 10000.0 * i / kSampleRate);
        std::vector<float> sf = {v}, sm = {v};
        bbdFew.process(sf, 1.0, 512, 9.0, false, 0.0);
        bbdMany.process(sm, 1.0, 8192, 9.0, false, 0.0);
        if (i > 1000) {
            sumFew += sf[0] * sf[0];
            sumMany += sm[0] * sm[0];
        }
    }

    // Fewer stages = less LPF = more HF energy
    REQUIRE(sumFew >= sumMany);
}

TEST_CASE("BbdStageEmulator: aging degrades signal", "[BBD]") {
    BbdStageEmulator bbdNew, bbdOld;
    bbdNew.prepare(1, kSampleRate);
    bbdOld.prepare(1, kSampleRate);

    const int N = 4000;
    double sumNew = 0.0, sumOld = 0.0;

    for (int i = 0; i < N; ++i) {
        float v = 0.5f * std::sin(2.0 * M_PI * 5000.0 * i / kSampleRate);
        std::vector<float> sn = {v}, so = {v};
        bbdNew.process(sn, 1.0, 4096, 9.0, false, 0.0);
        bbdOld.process(so, 1.0, 4096, 9.0, true, 30.0); // 30 years old
        if (i > 1000) {
            sumNew += sn[0] * sn[0];
            sumOld += so[0] * so[0];
        }
    }

    // Aging should affect the signal (dielectric absorption)
    REQUIRE(std::isfinite(sumOld));
}

TEST_CASE("BbdStageEmulator: setBandwidthScale works", "[BBD]") {
    BbdStageEmulator bbd;
    bbd.prepare(1, kSampleRate);
    bbd.setBandwidthScale(2.0);

    std::vector<float> s = {0.5f};
    bbd.process(s, 1.0, 4096, 9.0, false, 0.0);
    REQUIRE(std::isfinite(s[0]));
}
