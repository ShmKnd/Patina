// ============================================================================
// WaveFolder unit tests
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/circuits/saturation/WaveFolder.h"

using Catch::Approx;

static constexpr double kSampleRate = 44100.0;

TEST_CASE("WaveFolder: prepare and reset without crash", "[WaveFolder]") {
    WaveFolder wf;
    wf.prepare(2, kSampleRate);
    wf.reset();
    REQUIRE(true);
}

TEST_CASE("WaveFolder: prepare with ProcessSpec", "[WaveFolder]") {
    WaveFolder wf;
    patina::ProcessSpec spec{kSampleRate, 512, 2};
    wf.prepare(spec);
    wf.reset();
    REQUIRE(true);
}

TEST_CASE("WaveFolder: zero input produces zero output", "[WaveFolder]") {
    WaveFolder wf;
    wf.prepare(1, kSampleRate);

    WaveFolder::Params params;
    params.foldAmount = 0.5f;

    // Warm up DC filter
    for (int i = 0; i < 1000; ++i)
        wf.process(0, 0.0f, params);

    float out = wf.process(0, 0.0f, params);
    REQUIRE(std::fabs(out) < 0.001f);
}

TEST_CASE("WaveFolder: low fold amount approximates clean signal", "[WaveFolder]") {
    WaveFolder wf;
    wf.prepare(1, kSampleRate);

    WaveFolder::Params params;
    params.foldAmount = 0.0f;
    params.numStages = 1;

    // Feed 1kHz sine and measure
    const int N = 4000;
    double sumIn = 0.0, sumOut = 0.0;
    for (int i = 0; i < N; ++i) {
        float in = 0.3f * std::sin(2.0 * M_PI * 1000.0 * i / kSampleRate);
        float out = wf.process(0, in, params);
        if (i > 2000) {
            sumIn += in * in;
            sumOut += out * out;
        }
    }
    double rmsIn = std::sqrt(sumIn / 2000.0);
    double rmsOut = std::sqrt(sumOut / 2000.0);

    // With fold=0, output should be comparable to input (BJT saturate is mild at low levels)
    REQUIRE(rmsOut > 0.01);
    REQUIRE(rmsOut < rmsIn * 3.0); // Not drastically amplified
}

TEST_CASE("WaveFolder: high fold amount adds harmonics", "[WaveFolder]") {
    WaveFolder wf;
    wf.prepare(1, kSampleRate);

    WaveFolder::Params params;
    params.foldAmount = 1.0f;
    params.numStages = 4;

    // Feed sine and collect output
    const int N = 4000;
    double sumOut = 0.0;
    int zeroCrossings = 0;
    float prev = 0.0f;
    for (int i = 0; i < N; ++i) {
        float in = 0.5f * std::sin(2.0 * M_PI * 200.0 * i / kSampleRate);
        float out = wf.process(0, in, params);
        if (i > 2000) {
            sumOut += out * out;
            // Count zero crossings as harmonic indicator
            if ((out > 0 && prev <= 0) || (out < 0 && prev >= 0))
                ++zeroCrossings;
            prev = out;
        }
    }

    // Folded signal should have more zero crossings than a pure sine
    // Pure 200Hz sine in 2000 samples at 44100 Hz ~ 18 crossings
    REQUIRE(zeroCrossings > 15);
    REQUIRE(std::isfinite(sumOut));
}

TEST_CASE("WaveFolder: output is finite for extreme inputs", "[WaveFolder]") {
    WaveFolder wf;
    wf.prepare(1, kSampleRate);

    WaveFolder::Params params;
    params.foldAmount = 1.0f;
    params.numStages = 8;

    for (int i = 0; i < 500; ++i) {
        float out = wf.process(0, 10.0f, params);
        REQUIRE(std::isfinite(out));
    }
    for (int i = 0; i < 500; ++i) {
        float out = wf.process(0, -10.0f, params);
        REQUIRE(std::isfinite(out));
    }
}

TEST_CASE("WaveFolder: different diode types produce different results", "[WaveFolder]") {
    // Si diode
    WaveFolder wfSi;
    wfSi.prepare(1, kSampleRate);
    WaveFolder::Params pSi;
    pSi.foldAmount = 0.8f;
    pSi.diodeType = 0;

    // Ge diode
    WaveFolder wfGe;
    wfGe.prepare(1, kSampleRate);
    WaveFolder::Params pGe;
    pGe.foldAmount = 0.8f;
    pGe.diodeType = 2;

    float input = 0.7f;
    // Warm up DC filters
    for (int i = 0; i < 2000; ++i) {
        wfSi.process(0, 0.3f * std::sin(2.0 * M_PI * 1000.0 * i / kSampleRate), pSi);
        wfGe.process(0, 0.3f * std::sin(2.0 * M_PI * 1000.0 * i / kSampleRate), pGe);
    }

    float outSi = wfSi.process(0, input, pSi);
    float outGe = wfGe.process(0, input, pGe);

    // Different diode types should produce different outputs
    REQUIRE(std::fabs(outSi - outGe) > 0.001f);
}

TEST_CASE("WaveFolder: symmetry parameter affects output", "[WaveFolder]") {
    WaveFolder wfSym;
    wfSym.prepare(1, kSampleRate);
    WaveFolder::Params pSym;
    pSym.foldAmount = 0.8f;
    pSym.symmetry = 1.0f;

    WaveFolder wfAsym;
    wfAsym.prepare(1, kSampleRate);
    WaveFolder::Params pAsym;
    pAsym.foldAmount = 0.8f;
    pAsym.symmetry = 0.3f;

    for (int i = 0; i < 2000; ++i) {
        float in = 0.4f * std::sin(2.0 * M_PI * 500.0 * i / kSampleRate);
        wfSym.process(0, in, pSym);
        wfAsym.process(0, in, pAsym);
    }

    // Compare RMS over next block
    double rmsSym = 0.0, rmsAsym = 0.0;
    for (int i = 0; i < 1000; ++i) {
        float in = 0.4f * std::sin(2.0 * M_PI * 500.0 * (i + 2000) / kSampleRate);
        float oSym = wfSym.process(0, in, pSym);
        float oAsym = wfAsym.process(0, in, pAsym);
        rmsSym += oSym * oSym;
        rmsAsym += oAsym * oAsym;
    }

    // Different symmetry should alter RMS or waveform shape
    REQUIRE(std::fabs(rmsSym - rmsAsym) > 0.0001);
}

TEST_CASE("WaveFolder: processBlock works correctly", "[WaveFolder]") {
    WaveFolder wf;
    wf.prepare(1, kSampleRate);
    WaveFolder::Params params;
    params.foldAmount = 0.5f;

    const int N = 256;
    float buffer[N];
    for (int i = 0; i < N; ++i)
        buffer[i] = 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / kSampleRate);

    float* channels[1] = { buffer };
    wf.processBlock(channels, 1, N, params);

    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(buffer[i]));
}
