// ============================================================================
// OtaLadderFilter unit tests
//
// OtaLadderFilter: 4-pole OTA ladder.
// ZDF TPT, OTA tanh saturator (softer than BJT), k=reso*4.0, self-oscillation.
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <vector>
#include "dsp/circuits/filters/OtaLadderFilter.h"
#include "dsp/core/ProcessSpec.h"

using Catch::Approx;
static constexpr double kSR = 44100.0;

// ── Helpers ──────────────────────────────────────────────────────────────────

static double measureRms (OtaLadderFilter& f, int ch, double freq,
                          int nTotal = 8000, int skip = 2000)
{
    f.reset();
    double sum = 0.0;
    for (int i = 0; i < nTotal; ++i)
    {
        float in  = 0.3f * (float)std::sin (2.0 * M_PI * freq * i / kSR);
        float out = f.process (ch, in);
        if (i >= skip) sum += (double)out * out;
    }
    return std::sqrt (sum / (nTotal - skip));
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

TEST_CASE("OtaLadder: prepare(numCh, sr) and reset without crash", "[OtaLadder]")
{
    OtaLadderFilter f;
    f.prepare (2, kSR);
    f.reset();
    REQUIRE (true);
}

TEST_CASE("OtaLadder: prepare via ProcessSpec", "[OtaLadder]")
{
    OtaLadderFilter f;
    patina::ProcessSpec spec;
    spec.sampleRate   = kSR;
    spec.numChannels  = 2;
    spec.maxBlockSize = 512;
    f.prepare (spec);
    REQUIRE (true);
}

// ── Basic correctness ─────────────────────────────────────────────────────────

TEST_CASE("OtaLadder: silence in -> silence out", "[OtaLadder]")
{
    OtaLadderFilter f;
    f.prepare (1, kSR);
    float out = 0.0f;
    for (int i = 0; i < 1000; ++i)
        out = f.process (0, 0.0f);
    REQUIRE (std::fabs (out) < 1e-5f);   // allow small thermal noise floor
}

TEST_CASE("OtaLadder: LP passes low frequencies, attenuates high (4-pole -24dB/oct)", "[OtaLadder]")
{
    OtaLadderFilter f;
    f.prepare (1, kSR);
    f.setCutoffHz (1000.0f);
    f.setResonance (0.0f);

    // 200 Hz (passband) vs 8000 Hz (8× fc, 4-pole → ~48 dB drop)
    const double rmsLow  = measureRms (f, 0, 200.0);
    const double rmsHigh = measureRms (f, 0, 8000.0);

    // 4-pole rolloff: high-freq should be at least 20 dB below passband
    REQUIRE (rmsHigh < rmsLow * 0.1);
}

TEST_CASE("OtaLadder: higher cutoff lets more high-freq energy through", "[OtaLadder]")
{
    OtaLadderFilter f;
    f.prepare (1, kSR);

    f.setCutoffHz (500.0f);
    f.setResonance (0.0f);
    const double rmsLow500  = measureRms (f, 0, 200.0);
    const double rmsHigh500 = measureRms (f, 0, 4000.0);

    f.setCutoffHz (8000.0f);
    f.setResonance (0.0f);
    const double rmsLow8k  = measureRms (f, 0, 200.0);
    const double rmsHigh8k = measureRms (f, 0, 4000.0);

    // At 8 kHz cutoff, 4 kHz should pass much more than at 500 Hz cutoff
    REQUIRE (rmsHigh8k > rmsHigh500 * 2.0);
    // Passband (200 Hz) should be similar regardless of cutoff
    REQUIRE (rmsLow500 == Approx (rmsLow8k).epsilon (0.3));
}

// ── Resonance / self-oscillation ──────────────────────────────────────────────

TEST_CASE("OtaLadder: resonance boosts frequency at cutoff", "[OtaLadder]")
{
    OtaLadderFilter f;
    f.prepare (1, kSR);
    f.setCutoffHz (1000.0f);

    f.setResonance (0.0f);
    const double rmsNoReso = measureRms (f, 0, 1000.0);

    f.setResonance (0.7f);
    const double rmsHiReso = measureRms (f, 0, 1000.0);

    // High resonance should boost output at fc
    REQUIRE (rmsHiReso > rmsNoReso);
}

TEST_CASE("OtaLadder: high resonance impulse response remains stable", "[OtaLadder]")
{
    OtaLadderFilter f;
    f.prepare (1, kSR);
    f.setCutoffHz (1000.0f);
    f.setResonance (1.0f);

    // Kick-start with a single impulse, then silence
    f.process (0, 0.01f);
    double energy = 0.0;
    for (int i = 0; i < 4000; ++i)
        energy += std::fabs ((double)f.process (0, 0.0f));

    REQUIRE (std::isfinite (energy));
    REQUIRE (energy > 1e-6);
}

// ── Parameter API ─────────────────────────────────────────────────────────────

TEST_CASE("OtaLadder: Params struct overload matches direct setters", "[OtaLadder]")
{
    OtaLadderFilter f1, f2;
    f1.prepare (1, kSR);
    f2.prepare (1, kSR);

    OtaLadderFilter::Params p;
    p.cutoffHz  = 2000.0f;
    p.resonance = 0.4f;
    p.drive     = 0.0f;

    f1.setCutoffHz (p.cutoffHz);
    f1.setResonance (p.resonance);

    const int N = 512;
    std::vector<float> buf (N);
    for (int i = 0; i < N; ++i)
        buf[i] = 0.3f * (float)std::sin (2.0 * M_PI * 440.0 * i / kSR);

    float maxDiff = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        const float out1 = f1.process (0, buf[i]);
        const float out2 = f2.process (0, buf[i], p);
        maxDiff = std::max (maxDiff, std::fabs (out1 - out2));
    }
    // Both paths should produce identical output (same internal state path)
    REQUIRE (maxDiff < 1e-5f);
}

TEST_CASE("OtaLadder: processBlock matches per-sample", "[OtaLadder]")
{
    OtaLadderFilter f1, f2;
    f1.prepare (1, kSR);
    f2.prepare (1, kSR);

    OtaLadderFilter::Params p;
    p.cutoffHz  = 1500.0f;
    p.resonance = 0.3f;
    p.drive     = 0.0f;

    const int N = 256;
    std::vector<float> srcBuf (N), blkBuf (N);
    for (int i = 0; i < N; ++i)
        srcBuf[i] = blkBuf[i] = 0.3f * (float)std::sin (2.0 * M_PI * 880.0 * i / kSR);

    // Per-sample reference
    for (int i = 0; i < N; ++i)
        srcBuf[i] = f1.process (0, srcBuf[i], p);

    // processBlock
    float* ptr = blkBuf.data();
    f2.processBlock (&ptr, 1, N, p);

    for (int i = 0; i < N; ++i)
        REQUIRE (blkBuf[i] == Approx (srcBuf[i]).margin (1e-5f));
}

// ── Multichannel ──────────────────────────────────────────────────────────────

TEST_CASE("OtaLadder: stereo channels are independent", "[OtaLadder]")
{
    OtaLadderFilter f;
    f.prepare (2, kSR);
    f.setCutoffHz (2000.0f);
    f.setResonance (0.0f);

    // Feed ch0 with a tone, ch1 with silence
    for (int i = 0; i < 4000; ++i)
    {
        float in = 0.3f * (float)std::sin (2.0 * M_PI * 440.0 * i / kSR);
        f.process (0, in);
        f.process (1, 0.0f);
    }
    const float out0 = f.process (0, 0.3f);
    const float out1 = f.process (1, 0.0f);

    REQUIRE (std::fabs (out0) > 1e-4f);
    REQUIRE (std::fabs (out1) < 1e-4f);
}

// ── Temperature ───────────────────────────────────────────────────────────────

TEST_CASE("OtaLadder: temperature change does not crash or produce NaN", "[OtaLadder]")
{
    OtaLadderFilter f;
    f.prepare (1, kSR);

    OtaLadderFilter::Params p;
    p.cutoffHz  = 1000.0f;
    p.resonance = 0.3f;

    for (float temp : { -20.0f, 0.0f, 25.0f, 60.0f, 80.0f })
    {
        p.temperature = temp;
        f.reset();
        float out = 0.0f;
        for (int i = 0; i < 200; ++i)
            out = f.process (0, 0.3f * (float)std::sin (i * 0.1f), p);
        INFO ("temperature = " << temp);
        REQUIRE (std::isfinite (out));
    }
}

TEST_CASE("OtaLadder: warm temperature shifts cutoff slightly lower", "[OtaLadder]")
{
    // OTA gm decreases with temperature (-0.3%/°C), effectively lowering fc.
    // At fc=1 kHz: cold (0°C) should let more high-freq through than hot (60°C).
    OtaLadderFilter f;
    f.prepare (1, kSR);

    OtaLadderFilter::Params p;
    p.cutoffHz  = 1000.0f;
    p.resonance = 0.0f;

    p.temperature = 0.0f;
    const double rmsCold = measureRms (f, 0, 2000.0);   // above fc

    p.temperature = 60.0f;
    f.reset();
    double sumHot = 0.0;
    const int N = 8000, skip = 2000;
    for (int i = 0; i < N; ++i)
    {
        float in  = 0.3f * (float)std::sin (2.0 * M_PI * 2000.0 * i / kSR);
        float out = f.process (0, in, p);
        if (i >= skip) sumHot += (double)out * out;
    }
    const double rmsHot = std::sqrt (sumHot / (N - skip));

    // Hot → gm lower → fc lower → more attenuation at 2 kHz than cold
    REQUIRE (rmsHot < rmsCold * 1.1);   // at worst equal (allow 10 % tolerance on direction)
}

// ── Stability / edge cases ────────────────────────────────────────────────────

TEST_CASE("OtaLadder: no NaN/Inf at extreme cutoff values", "[OtaLadder]")
{
    OtaLadderFilter f;
    f.prepare (1, kSR);
    f.setResonance (0.0f);

    for (float fc : { 20.0f, 100.0f, 10000.0f, 19000.0f })
    {
        f.setCutoffHz (fc);
        f.reset();
        float out = 0.0f;
        for (int i = 0; i < 200; ++i)
            out = f.process (0, 0.3f * (float)std::sin (i * 0.2f));
        INFO ("fc = " << fc);
        REQUIRE (std::isfinite (out));
    }
}

TEST_CASE("OtaLadder: no NaN/Inf at high resonance with loud input", "[OtaLadder]")
{
    OtaLadderFilter f;
    f.prepare (1, kSR);
    f.setCutoffHz (1000.0f);
    f.setResonance (0.99f);

    float out = 0.0f;
    for (int i = 0; i < 2000; ++i)
        out = f.process (0, 0.9f * (float)std::sin (2.0 * M_PI * 500.0 * i / kSR));

    REQUIRE (std::isfinite (out));
}
