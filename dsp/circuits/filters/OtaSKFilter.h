#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/OTA_Primitive.h"
#include "../../parts/DiodePrimitive.h"

// OTA Sallen-Key filter emulation
// - OTA (LM13700)-based Sallen-Key topology
// - 2-pole (-12dB/oct) HPF + LPF dual filter configuration
// - Aggressive resonance characteristic (capable of self-oscillation)
// - OTA input diode clipping (distortion on large signals)
// - Temperature-dependent gm drift
// - Component tolerance (left/right asymmetry from OTA mismatch)
//
// Circuit topology:
//   early type: OTA (LM13700) Sallen-Key
//   late type: OPA (LM358) Sallen-Key
//   this implementation models the OTA version (early type)
//
// 4-layer architecture:
//   Parts: OTA_Primitive (LM13700) × 2 + DiodePrimitive (input protection)
//   → Circuit: OtaSKFilter (Sallen-Key 2-pole)
//   → Effect: (used by upper-level modules)
//
// usage:
//   HPF → LPF cascade for classic OTA dual filter
//   can also be used as standalone HPF/LPF
class OtaSKFilter
{
public:
    struct Output
    {
        double lp = 0.0;
        double bp = 0.0;
        double hp = 0.0;
        double notch = 0.0;
    };

    enum class Mode : int
    {
        LowPass  = 0,
        HighPass = 1,
        BandPass = 2   // HPF → LPF internal cascade
    };

    struct Params
    {
        float cutoffHz    = 1000.0f;
        float resonance   = 0.0f;     // 0.0–1.0, self-oscillation at 1.0
        float drive       = 0.0f;     // OTA input overdrive 0.0–1.0
        float temperature = 25.0f;    // operating temperature (°C)
        int   mode        = 0;        // 0=LP, 1=HP, 2=BP (dual)
    };

    OtaSKFilter() noexcept
        : rng(41),
          ota1(OTA_Primitive(OTA_Primitive::LM13700(), 211)),
          ota2(OTA_Primitive(OTA_Primitive::LM13700(), 212)),
          inputDiode(DiodePrimitive(DiodePrimitive::OtaInputDiode()))
    {
    }

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        chState.resize(nCh);
        for (auto& st : chState) st = ChannelState{};
        updateCoefficients(1000.0f, 0.0f, 25.0f);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        for (auto& st : chState) st = ChannelState{};
    }

    void setCutoffHz(float hz) noexcept
    {
        cutoffHz = std::clamp((double)hz, 20.0, sampleRate * 0.49);
        updateCoefficients((float)cutoffHz, (float)reso, lastTemp);
    }

    void setResonance(float r) noexcept
    {
        reso = std::clamp((double)r, 0.0, 1.0);
        updateCoefficients((float)cutoffHz, (float)reso, lastTemp);
    }

    void setMode(Mode m) noexcept { filterMode = m; }

    inline Output processAll(int channel, float x) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)chState.size() - 1);

        // OTA input protection diode clipping on large signals
        // Uses DiodePrimitive (OtaInputDiode preset) matching physical circuit
        double v0 = (double)x;
        if (driveAmount > 0.001)
        {
            double scale = 1.0 + driveAmount * 4.0;
            v0 = inputDiode.clip(v0 * scale, (double)lastTemp) / scale
                 * (1.0 + driveAmount * 0.5);
        }

        // Thermal noise (using OTA primitive physical properties)
        double sigLevel = std::abs(v0);
        if (sigLevel > 1e-10)
            v0 += fastNoise(rng) * ota1.getSpec().thermalNoise * std::min(1.0, sigLevel);

        auto out = processSVF(chState[ch].svf, v0);
        return { out.lp, out.bp, out.hp, out.lp + out.hp };
    }

    // single sample processing
    inline float process(int channel, float x, int modeOverride = -1) noexcept
    {
        Mode m = (modeOverride >= 0) ? (Mode)modeOverride : filterMode;

        auto out = processAll(channel, x);
        switch (m)
        {
            case Mode::LowPass:  return (float)out.lp;
            case Mode::HighPass:  return (float)out.hp;
            case Mode::BandPass:  return (float)out.bp;
        }
        return (float)out.lp;
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        updateCoefficients(params.cutoffHz, params.resonance, params.temperature);
        driveAmount = std::clamp((double)params.drive, 0.0, 1.0);
        filterMode = (Mode)std::clamp(params.mode, 0, 2);
        return process(channel, x);
    }

    void processBlock(float* const* io, int numChannels, int numSamples, const Params& params) noexcept
    {
        updateCoefficients(params.cutoffHz, params.resonance, params.temperature);
        driveAmount = std::clamp((double)params.drive, 0.0, 1.0);
        filterMode = (Mode)std::clamp(params.mode, 0, 2);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i]);
    }

private:
    // === Component layer (Parts) ===
    OTA_Primitive ota1;           // LM13700 OTA #1
    OTA_Primitive ota2;           // LM13700 OTA #2
    DiodePrimitive inputDiode;    // OTA-SK input protection diode

    // TPT SVF 2-pole filter state
    struct FilterState
    {
        double s1 = 0.0;  // integrator 1 state (≈ band-pass)
        double s2 = 0.0;  // integrator 2 state (≈ low-pass)
    };

    struct ChannelState
    {
        FilterState svf;  // single unified state (LP/BP/HP from one pass)
    };

    struct SVFOut { double lp, bp, hp; };

    // --- TPT SVF core with OTA nonlinear feedback ---
    // Reference: Zavalishin "Art of VA Filter Design" §3.10 (ZDF base)
    //
    // Correct ZDF SVF derivation (substituting bp and lp into hp = x - k*bp - lp):
    //   bp = s1 + g1*hp
    //   lp = s2 + g2*bp = s2 + g2*s1 + g1*g2*hp
    //   hp*(1 + k*g1 + g1*g2) = x - (k+g2)*s1 - s2
    //
    // The hp numerator must contain -(k+g2)*s1, NOT just -k*s1.
    // Omitting the g2*s1 term causes instability at high fc:
    //   eigenvalues |λ| > 1 for g > ~1 (cutoff above fs/4).
    //
    // OTA-SK character: the damping feedback (k*s1) passes through the OTA input
    // protection diode (clipped). The integrator feed-forward path (g2*s1) is
    // unclipped — it represents the linear integrator chain, not the feedback loop.
    inline SVFOut processSVF(FilterState& st, double x) noexcept
    {
        const double g1 = gCoeff * ota1.getMismatch();
        const double g2 = gCoeff * ota2.getMismatch();

        // Nonlinear feedback: diode clips only the damping-path portion of s1.
        // The g2*s1 integrator-path term stays linear (see derivation above).
        const double s1_fb = inputDiode.feedbackClip(st.s1, (double)lastTemp);

        const double denom = 1.0 + g1 * damping + g1 * g2;
        const double hp  = (x - damping * s1_fb - g2 * st.s1 - st.s2) / denom;
        const double bp  = g1 * hp + st.s1;
        const double lp  = g2 * bp + st.s2;

        constexpr double kLim = 10.0;
        const double ns1 = 2.0 * bp - st.s1;
        const double ns2 = 2.0 * lp - st.s2;
        st.s1 = std::isfinite(ns1) ? std::clamp(ns1, -kLim, kLim) : 0.0;
        st.s2 = std::isfinite(ns2) ? std::clamp(ns2, -kLim, kLim) : 0.0;
        return { lp, bp, hp };
    }

    void updateCoefficients(float fc, float r, float temperature = 25.0f) noexcept
    {
        cutoffHz = std::clamp((double)fc, 20.0, sampleRate * 0.49);
        reso = std::clamp((double)r, 0.0, 1.0);
        lastTemp = temperature;

        // temperature-dependent gm scaling
        const double gmScale = ota1.gmScale(temperature);
        const double effectiveFc = std::clamp(cutoffHz * gmScale, 20.0, sampleRate * 0.45);

        // TPT SVF g coefficient: raw tan (not divided by (1+g))
        gCoeff = std::tan(3.14159265358979323846 * effectiveFc / sampleRate);

        // Damping ↔ resonance mapping:
        // Keep a small minimum damping so OTA notch (LP+HP) does not collapse
        // to near all-pass at max resonance.
        //   reso=0.0 → damping=2.0
        //   reso=1.0 → damping=0.18
        constexpr double dMin = 0.18;
        constexpr double gamma = 1.2;
        damping = dMin + (2.0 - dMin) * std::pow(1.0 - reso, gamma);
    }

    double sampleRate  = PartsConstants::defaultSampleRate;
    double cutoffHz    = 1000.0;
    double reso        = 0.0;
    float  lastTemp    = 25.0f;
    double gCoeff      = 0.0;
    double damping     = 2.0;   // 0=self-oscillation, 2=overdamped
    double driveAmount = 0.0;
    Mode   filterMode  = Mode::LowPass;

    std::vector<ChannelState> chState;
    std::minstd_rand rng;
    std::normal_distribution<double> normalDist;  // unused placeholder
    static double fastNoise (std::minstd_rand& r) noexcept
    {
        constexpr double kInv = 1.0 / 2147483648.0;
        const double s = ((double)(int)r() + (double)(int)r()
                        + (double)(int)r() + (double)(int)r()) * kInv;
        return (s - 2.0) * 1.7320508;
    }
};
