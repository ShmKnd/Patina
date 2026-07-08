#pragma once
#include <cmath>
#include <algorithm>
#include "../../core/ProcessSpec.h"

// ─────────────────────────────────────────────────────────────────────
// AllPassFilter  —  1st-order TPT All-Pass, cascadable up to 4 stages
//
// A 1st-order all-pass section has unity amplitude at all frequencies
// and shifts phase from 0° (f << fc) to -180° (f >> fc), with -90° at fc.
//
// Cascading N identical sections multiplies the phase shift:
//   N = 1 stage  →  0° … -180°   (-90° @ fc)
//   N = 2 stages →  0° … -360°   (-180° @ fc)   ← classic 2-stage phaser
//   N = 3 stages →  0° … -540°   (-270° @ fc)
//   N = 4 stages →  0° … -720°   (-360° @ fc)
//
// Hardware context:
//   Phaser and modulation circuits often use chains of 1st-order RC all-pass
//   networks (op-amp based in modern designs). Each RC section is exactly this
//   1st-order AP stage.
//   There is no "resonance" in a single AP section; resonance in phasers
//   comes from a feedback path around the entire chain.
//
// ZDF / TPT formulation (Zavalishin §3.1 extended to AP):
//   LP = (s_old + g * x) / (1 + g)    where g = tan(π * fc / fs)
//   s_new = 2 * LP - s_old             (TPT state update)
//   AP = 2 * LP - x                    (LP - HP = 2*LP - x)
//
// This avoids any delay-free-loop and is inherently stable.
//
// 4-layer architecture:
//   Parts:   (none — pure topology filter, no BJT/OTA nonlinearity)
//   Circuit: AllPassFilter (N-stage 1st-order AP ladder)
// ─────────────────────────────────────────────────────────────────────

class AllPassFilter
{
public:
    // ── Per-channel state ─────────────────────────────────────────
    struct ChannelState
    {
        float s[4] = {};   // integrator state for up to 4 AP stages
    };

    // ── Lifecycle ─────────────────────────────────────────────────
    void prepare (const patina::ProcessSpec& spec) noexcept
    {
        sampleRate = spec.sampleRate;
        g = std::tan (3.14159265358979323846 * cutoffHz / sampleRate);
        reset();
    }

    // Convenience overload for DigitalFilterEngine (no ProcessSpec available)
    void prepareSampleRate (double sr) noexcept
    {
        sampleRate = sr;
        g = std::tan (3.14159265358979323846 * cutoffHz / sampleRate);
        reset();
    }

    void reset() noexcept
    {
        for (auto& c : state) c = {};
    }

    // ── Parameter setters ─────────────────────────────────────────
    void setCutoffHz (float hz) noexcept
    {
        cutoffHz = std::max (20.0f, std::min ((float)(sampleRate * 0.48), hz));
        g = (float)std::tan (3.14159265358979323846 * cutoffHz / sampleRate);
    }

    // ── DSP ───────────────────────────────────────────────────────
    // Process one sample through 'stages' 1st-order AP sections.
    // stages = 1..4 (clamped internally).
    // The 'drive' argument is accepted for interface uniformity but unused
    // (all-pass sections have no nonlinearity or gain).
    float process (int ch, float x, float /*drive*/ = 0.0f, int stages = 1) noexcept
    {
        auto& st = state[ch & 1];
        float y  = x;
        const int n = std::min (stages, 4);
        for (int i = 0; i < n; ++i)
        {
            const float lp = (st.s[i] + g * y) / (1.0f + g);
            st.s[i]        = 2.0f * lp - st.s[i];   // TPT state update
            y              = 2.0f * lp - y;            // AP = 2*LP - x
        }
        return y;
    }

private:
    ChannelState state[2];          // [ch]
    float        cutoffHz  = 1000.0f;
    float        g         = 0.0f;   // tan(π*fc/fs), cached on setCutoffHz
    double       sampleRate = 44100.0;
};
