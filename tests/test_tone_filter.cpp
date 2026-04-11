// ============================================================================
// ToneFilter unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/circuits/filters/ToneFilter.h"
#include "dsp/circuits/filters/ToneShaper.h"

using Catch::Approx;

static constexpr double kSampleRate = 44100.0;

TEST_CASE("ToneFilter: prepare and reset without crash", "[ToneFilter]") {
    ToneFilter bank;
    bank.prepare(kSampleRate, 2, 512);
    bank.reset();
    REQUIRE(true);
}

TEST_CASE("ToneFilter: passes DC (low-pass behavior)", "[ToneFilter]") {
    ToneFilter bank;
    bank.prepare(kSampleRate, 1, 512);
    bank.setDefaultCutoff(5000.0f);

    float out = 0.0f;
    for (int i = 0; i < 2000; ++i)
        out = bank.processSample(0, 1.0f);
    REQUIRE(out == Approx(1.0f).margin(0.01f));
}

TEST_CASE("ToneFilter: attenuates high frequencies", "[ToneFilter]") {
    ToneFilter bank;
    bank.prepare(kSampleRate, 1, 512);
    bank.setDefaultCutoff(1000.0f);

    double sumLow = 0.0, sumHigh = 0.0;
    const int N = 4000;

    // 200 Hz
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0f * 3.14159f * 200.0f * i / (float)kSampleRate);
        float out = bank.processSample(0, in);
        if (i > 1000) sumLow += out * out;
    }
    bank.reset();

    // 10000 Hz
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0f * 3.14159f * 10000.0f * i / (float)kSampleRate);
        float out = bank.processSample(0, in);
        if (i > 1000) sumHigh += out * out;
    }

    REQUIRE(sumLow > sumHigh);
}

TEST_CASE("ToneFilter: tone param 0 = dark, 1 = bright", "[ToneFilter]") {
    ToneFilter bankDark, bankBright;
    bankDark.prepare(kSampleRate, 1, 512);
    bankBright.prepare(kSampleRate, 1, 512);

    bankDark.updateToneFilterIfNeeded(0.0f, 1.0, false);
    bankBright.updateToneFilterIfNeeded(1.0f, 1.0, false);

    double sumDark = 0.0, sumBright = 0.0;
    const int N = 4000;

    // 5000 Hz test tone
    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0f * 3.14159f * 5000.0f * i / (float)kSampleRate);
        float outDark = bankDark.processSample(0, in);
        float outBright = bankBright.processSample(0, in);
        if (i > 1000) {
            sumDark += outDark * outDark;
            sumBright += outBright * outBright;
        }
    }

    // Bright setting should pass more HF than dark
    REQUIRE(sumBright > sumDark);
}

TEST_CASE("ToneFilter: RC emulation produces valid output", "[ToneFilter]") {
    ToneFilter bank;
    bank.prepare(kSampleRate, 1, 512);
    bank.updateToneFilterIfNeeded(0.5f, 1.0, true);

    float out = bank.processSample(0, 0.5f);
    REQUIRE(std::isfinite(out));
}

TEST_CASE("ToneFilter: aging scale affects cutoff", "[ToneFilter]") {
    ToneFilter bankNew, bankOld;
    bankNew.prepare(kSampleRate, 1, 512);
    bankOld.prepare(kSampleRate, 1, 512);

    bankNew.updateToneFilterIfNeeded(0.5f, 1.0, false);
    bankOld.updateToneFilterIfNeeded(0.5f, 0.5, false); // Aged capacitors

    double sumNew = 0.0, sumOld = 0.0;
    const int N = 4000;

    for (int i = 0; i < N; ++i) {
        float in = std::sin(2.0f * 3.14159f * 4000.0f * i / (float)kSampleRate);
        float outNew = bankNew.processSample(0, in);
        float outOld = bankOld.processSample(0, in);
        if (i > 1000) {
            sumNew += outNew * outNew;
            sumOld += outOld * outOld;
        }
    }

    // Lower aging scale = lower cutoff = less HF
    REQUIRE(sumNew > sumOld);
}

TEST_CASE("ToneFilter: updateToneFilterWithCutoff", "[ToneFilter]") {
    ToneFilter bank;
    bank.prepare(kSampleRate, 1, 512);
    bank.updateToneFilterWithCutoff(3000.0f, 1.0);

    float out = bank.processSample(0, 0.5f);
    REQUIRE(std::isfinite(out));
}

TEST_CASE("ToneFilter: ensureChannels adjusts", "[ToneFilter]") {
    ToneFilter bank;
    bank.prepare(kSampleRate, 1, 512);
    bank.ensureChannels(4, 512);

    float out = bank.processSample(3, 0.5f);
    REQUIRE(std::isfinite(out));
}

TEST_CASE("ToneShaper: process applies tone filter", "[ToneFilter][ToneShaper]") {
    ToneFilter bank;
    bank.prepare(kSampleRate, 2, 512);
    bank.setDefaultCutoff(2000.0f);

    std::vector<float> delayedOut = {0.5f, -0.3f};
    ToneShaper::process(bank, 2, delayedOut);

    REQUIRE(std::isfinite(delayedOut[0]));
    REQUIRE(std::isfinite(delayedOut[1]));
}
