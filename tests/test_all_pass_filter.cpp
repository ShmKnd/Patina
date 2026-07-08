// ============================================================================
// AllPassFilter unit tests
//
// AllPassFilter: 1st-order ZDF TPT all-pass section, cascadable up to 4 stages.
// LP = (s + g*x) / (1+g),  s' = 2*LP - s,  AP = x - 2*LP
// Unity magnitude at all frequencies; phase monotonically decreases with freq.
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "dsp/circuits/filters/AllPassFilter.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

// ── Helpers ──────────────────────────────────────────────────────────────────

// Measure steady-state RMS for a single-frequency sinusoid.
static double measureRms (AllPassFilter& f, int ch, double freq, int stages,
                          int nTotal = 8000, int skip = 2000)
{
    f.reset();
    double sum = 0.0;
    for (int i = 0; i < nTotal; ++i)
    {
        float in  = 0.5f * (float)std::sin (2.0 * M_PI * freq * i / kSR);
        float out = f.process (ch, in, 0.0f, stages);
        if (i >= skip) sum += (double)out * out;
    }
    return std::sqrt (sum / (nTotal - skip));
}

// Estimate phase shift at fc by comparing to a reference cosine (90° delayed).
// A 1-stage AP has -90° at fc, so the output should correlate with sin (in phase
// with a reference 90° later).  Returns phase in radians (expected ≈ -π/2 per stage).
static double estimatePhase (AllPassFilter& f, int ch, double fc, int stages,
                             int nTotal = 16000, int skip = 4000)
{
    f.reset();
    // Run until steady state
    double corrSin = 0.0, corrCos = 0.0;
    for (int i = 0; i < nTotal; ++i)
    {
        const double t   = 2.0 * M_PI * fc * i / kSR;
        float        in  = 0.5f * (float)std::sin (t);
        float        out = f.process (ch, in, 0.0f, stages);
        if (i >= skip)
        {
            corrSin += (double)out * std::sin (t);
            corrCos += (double)out * std::cos (t);
        }
    }
    return std::atan2 (corrCos, corrSin);   // atan2(im, re) — quadrant-correct
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

TEST_CASE("AllPassFilter: prepare via ProcessSpec, then reset", "[AllPassFilter]")
{
    AllPassFilter f;
    patina::ProcessSpec spec;
    spec.sampleRate  = kSR;
    spec.numChannels = 2;
    spec.maxBlockSize = 512;
    f.prepare (spec);
    f.reset();
    REQUIRE (true);
}

TEST_CASE("AllPassFilter: prepareSampleRate overload", "[AllPassFilter]")
{
    AllPassFilter f;
    f.prepareSampleRate (kSR);
    f.reset();
    REQUIRE (true);
}

TEST_CASE("AllPassFilter: silence in -> silence out (all stage counts)", "[AllPassFilter]")
{
    for (int stages = 1; stages <= 4; ++stages)
    {
        AllPassFilter f;
        f.prepareSampleRate (kSR);
        float out = 0.0f;
        for (int i = 0; i < 1000; ++i)
            out = f.process (0, 0.0f, 0.0f, stages);
        INFO ("stages = " << stages);
        REQUIRE (std::fabs (out) < 1e-6f);
    }
}

// ── Magnitude response ────────────────────────────────────────────────────────

TEST_CASE("AllPassFilter 1-stage: flat magnitude (low/mid/high)", "[AllPassFilter]")
{
    AllPassFilter f;
    f.prepareSampleRate (kSR);
    f.setCutoffHz (2000.0f);

    const double rmsLow  = measureRms (f, 0, 200.0,  1);
    const double rmsMid  = measureRms (f, 0, 2000.0, 1);
    const double rmsHigh = measureRms (f, 0, 8000.0, 1);

    // All-pass: magnitude flat to within 5 % (ε = 0.05)
    REQUIRE (rmsLow  == Approx (rmsMid ).epsilon (0.05));
    REQUIRE (rmsLow  == Approx (rmsHigh).epsilon (0.05));
}

TEST_CASE("AllPassFilter 2-stage: flat magnitude", "[AllPassFilter]")
{
    AllPassFilter f;
    f.prepareSampleRate (kSR);
    f.setCutoffHz (2000.0f);

    const double rmsLow  = measureRms (f, 0, 200.0,  2);
    const double rmsHigh = measureRms (f, 0, 8000.0, 2);
    REQUIRE (rmsLow == Approx (rmsHigh).epsilon (0.05));
}

TEST_CASE("AllPassFilter 4-stage: flat magnitude", "[AllPassFilter]")
{
    AllPassFilter f;
    f.prepareSampleRate (kSR);
    f.setCutoffHz (1000.0f);

    const double rmsLow  = measureRms (f, 0, 100.0,  4);
    const double rmsHigh = measureRms (f, 0, 6000.0, 4);
    REQUIRE (rmsLow == Approx (rmsHigh).epsilon (0.05));
}

// ── Phase response ────────────────────────────────────────────────────────────

TEST_CASE("AllPassFilter 1-stage: phase at fc is approximately -90 deg", "[AllPassFilter]")
{
    AllPassFilter f;
    f.prepareSampleRate (kSR);
    f.setCutoffHz (1000.0f);

    // estimatePhase returns atan2 of the correlation → expected ≈ -π/2 for 1-stage AP at fc
    const double phase = estimatePhase (f, 0, 1000.0, 1);
    // -π/2 ≈ -1.5708; allow ±0.3 rad tolerance (sinusoid measurement noise)
    REQUIRE (phase == Approx (-M_PI / 2.0).margin (0.3));
}

TEST_CASE("AllPassFilter 2-stage: phase at fc is approximately -180 deg", "[AllPassFilter]")
{
    AllPassFilter f;
    f.prepareSampleRate (kSR);
    f.setCutoffHz (1000.0f);

    const double phase = estimatePhase (f, 0, 1000.0, 2);
    // 2 stages → phase at fc ≈ -π; atan2 range is (-π, π] so expect close to ±π
    REQUIRE (std::fabs (std::fabs (phase) - M_PI) < 0.3);
}

TEST_CASE("AllPassFilter: phase decreases monotonically with frequency (1-stage)", "[AllPassFilter]")
{
    AllPassFilter f;
    f.prepareSampleRate (kSR);
    f.setCutoffHz (2000.0f);

    // Phase is monotonically decreasing (0° at DC → -180° at Nyquist)
    // Measure at three points by cross-correlation with the input
    auto phase = [&] (double freq) { return estimatePhase (f, 0, freq, 1); };
    const double pLow  = phase (200.0);
    const double pMid  = phase (2000.0);
    const double pHigh = phase (10000.0);

    REQUIRE (pLow  > pMid);
    REQUIRE (pMid  > pHigh);
}

// ── Cutoff parameter ──────────────────────────────────────────────────────────

TEST_CASE("AllPassFilter: setCutoffHz changes phase at reference freq", "[AllPassFilter]")
{
    AllPassFilter f;
    f.prepareSampleRate (kSR);

    f.setCutoffHz (500.0f);
    const double phase500 = estimatePhase (f, 0, 2000.0, 1);

    f.setCutoffHz (4000.0f);
    const double phase4k  = estimatePhase (f, 0, 2000.0, 1);

    // Different fc → different phase shift at 2 kHz; at least 0.1 rad difference
    REQUIRE (std::fabs (phase500 - phase4k) > 0.1);
}

// ── Multichannel independence ─────────────────────────────────────────────────

TEST_CASE("AllPassFilter: channels are independent", "[AllPassFilter]")
{
    AllPassFilter f;
    f.prepareSampleRate (kSR);
    f.setCutoffHz (1000.0f);

    // Feed ch0 with a tone, ch1 with silence
    for (int i = 0; i < 4000; ++i)
    {
        float in = 0.5f * (float)std::sin (2.0 * M_PI * 440.0 * i / kSR);
        f.process (0, in,   0.0f, 1);
        f.process (1, 0.0f, 0.0f, 1);
    }
    // ch1 should still output silence, ch0 should have a signal
    const float out0 = f.process (0, 0.5f, 0.0f, 1);
    const float out1 = f.process (1, 0.0f, 0.0f, 1);

    REQUIRE (std::fabs (out0) > 0.01f);
    REQUIRE (std::fabs (out1) < 1e-5f);
}

// ── Stability / edge cases ────────────────────────────────────────────────────

TEST_CASE("AllPassFilter: no NaN/Inf at extreme cutoff values", "[AllPassFilter]")
{
    AllPassFilter f;
    f.prepareSampleRate (kSR);

    for (float fc : { 20.0f, 100.0f, 10000.0f, 20000.0f, 21000.0f })
    {
        f.setCutoffHz (fc);
        f.reset();
        float out = 0.0f;
        for (int i = 0; i < 100; ++i)
            out = f.process (0, 0.5f * (float)std::sin (i * 0.1f), 0.0f, 2);
        INFO ("fc = " << fc);
        REQUIRE (std::isfinite (out));
    }
}

TEST_CASE("AllPassFilter: stages clamped to 1-4, no crash", "[AllPassFilter]")
{
    AllPassFilter f;
    f.prepareSampleRate (kSR);

    // stages outside [1,4] should be clamped internally
    const float out0 = f.process (0, 0.5f, 0.0f, 0);   // clamp to 1 (or handle as 0)
    const float out5 = f.process (0, 0.5f, 0.0f, 5);   // clamp to 4
    REQUIRE (std::isfinite (out0));
    REQUIRE (std::isfinite (out5));
}
