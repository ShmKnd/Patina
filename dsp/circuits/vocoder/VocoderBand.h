#pragma once
#include <cmath>
#include <algorithm>
#include <vector>

#include "../../parts/OTA_Primitive.h"
#include "../../parts/RC_Element.h"
#include "../../parts/DiodePrimitive.h"
#include "../../parts/VcaPrimitive.h"
#include "../../core/ProcessSpec.h"

// ============================================================
// VocoderBand — L3 vocoder band circuit
//
// 1 band of a classic analog vocoder channel strip.
// Fully constructed from L2 primitives only.
//
// Signal flow (per band):
//
//   [Modulator In] ──► OTA SVF (BPF) ──► DiodePrimitive (half-wave rect)
//                                              │
//                              attack RC ◄─────┤
//                              release RC ◄────┘
//                                    │ (envelope CV 0.0–1.0)
//   [Carrier In]   ──► OTA SVF (BPF) ──► VcaPrimitive ──► [Out]
//
// BPF core:
//   OTA SVF (Andy Simper trapezoidal topology) driven by OTA_Primitive::integrate().
//   gCoeff = 2 * sin(π * fc / fs) * gm — temperature-dependent via OTA_Primitive::gmScale().
//   Q is set independently as a damping coefficient.
//
// Envelope detector:
//   DiodePrimitive::clip() (Si1N4148 half-wave rectifier)
//   → RC_Element attack  (R_att, C_env: fast charge)
//   → RC_Element release (R_rel, C_env: slow discharge toward GND)
//   Peak hold via asymmetric RC — standard analog vocoder topology.
//
// VCA:
//   VcaPrimitive::applyGain() — Blackmer THAT2180 log/antilog BJT cell.
//   CV input is the normalised envelope (0.0–1.0).
// ============================================================

class VocoderBand
{
public:
    // --------------------------------------------------------
    // Params — real-time controllable parameters
    // --------------------------------------------------------
    struct Params
    {
        float centerHz    = 1000.0f;  // BPF center frequency (Hz)
        float q           = 4.0f;     // BPF Q (resonance)
        float envAmount   = 1.0f;     // Envelope depth applied to VCA (0.0–1.0)
        float temperature = 25.0f;    // Operating temperature (°C) — affects OTA gm + VCA gain
    };
    // Spec — component-level constants (change for different band characters)
    // --------------------------------------------------------
    struct Spec
    {
        // BPF — OTA type for both mod and carrier SVF
        OTA_Primitive::Spec otaSpec      = OTA_Primitive::LM13700();

        // Envelope detector — rectifier diode type
        DiodePrimitive::Spec diodeSpec   = DiodePrimitive::Si1N4148();

        // VCA chip type
        VcaPrimitive::Spec   vcaSpec     = VcaPrimitive::THAT2180();

        // Envelope RC time constants
        // Attack:  R_att * C_env  → fast charge (default ≈ 5 ms at 1µF)
        double R_att = 5e3;     // 5 kΩ
        double C_env = 1e-6;    // 1 µF

        // Release: R_rel * C_env → slow discharge (default ≈ 50 ms at 1µF)
        double R_rel = 50e3;    // 50 kΩ
        // C_env shared with attack path (physically the same cap)
    };



    // --------------------------------------------------------
    // prepare / reset
    // --------------------------------------------------------
    void prepare(const patina::ProcessSpec& pspec) noexcept
    {
        sr    = std::max(1.0, pspec.sampleRate);
        numCh = std::max(1, pspec.numChannels);

        modState.assign((size_t)numCh, SVFState{});
        carState.assign((size_t)numCh, SVFState{});
        envState.assign((size_t)numCh, 0.0);

        attackRC.prepare(sr);
        releaseRC.prepare(sr);

        // [FIX 2] キャッシュ初期化
        cachedGCoeff = 0.0;
        cachedCarGCoeff = 0.0;
        cachedDamp = 0.0;
        lastCenterHz = -1.0f;
        lastQ = -1.0f;
        lastTemperature = -999.0f;
    }

    void prepare(int channels, double sampleRate) noexcept
    {
        patina::ProcessSpec ps;
        ps.sampleRate   = sampleRate;
        ps.numChannels  = channels;
        ps.maxBlockSize = 512;
        prepare(ps);
    }

    void reset() noexcept
    {
        for (auto& s : modState) s = SVFState{};
        for (auto& s : carState) s = SVFState{};
        std::fill(envState.begin(), envState.end(), 0.0);
        attackRC.reset();
        releaseRC.reset();
    }

    // --------------------------------------------------------
    // process — per-sample
    //
    //   modIn : modulator signal  (e.g. voice)
    //   carIn : carrier signal    (e.g. synth / noise)
    //   returns band-limited, envelope-shaped carrier output
    // --------------------------------------------------------
    inline float process(int channel, float modIn, float carIn,
                         const Params& params) noexcept
    {
        if (channel < 0 || channel >= numCh) return 0.0f;
        const int ch = channel;

        // [FIX 2] params変化時のみ係数を再計算
        updateCoeffsIfNeeded(params);

        // --- 2. Modulator BPF (OTA SVF) ---
        const double modBp = processSVF(modState[ch], modOta,
                                        (double)modIn, cachedGCoeff, cachedDamp);

        // --- 3. Envelope detector ---
        //   a) half-wave rectification via Si diode clip
        double rectified = rectDiode.clip(modBp);
        rectified = std::max(0.0, rectified);   // half-wave: keep positive only

        //   b) asymmetric RC: charge fast (attack), discharge slow (release)
        //   [FIX 3] release時は 0.0 を入力とする（GNDへの放電を正しくモデル化）
        double& env = envState[ch];
        if (rectified > env)
            env = attackRC.processLPF(rectified);   // fast charge toward rectified peak
        else
            env = releaseRC.processLPF(0.0);        // slow discharge toward GND

        // --- 4. Carrier BPF (OTA SVF, same gCoeff, independent OTA instance) ---
        const double carBp = processSVF(carState[ch], carOta,
                                        (double)carIn, cachedCarGCoeff, cachedDamp);

        // --- 5. VCA: carrier × normalised envelope CV ---
        const double cv  = std::clamp(env * (double)params.envAmount, 0.0, 1.0);
        const double out = vca.applyGain(carBp, cv);

        return (float)std::clamp(out, -1.0, 1.0);
    }

    // --------------------------------------------------------
    // processBlock — block processing
    // [FIX 1] ループ順を外チャンネル・内サンプルに修正（キャッシュ局所性）
    // --------------------------------------------------------
    void processBlock(const float* const* modIn,
                      const float* const* carIn,
                      float* const*       output,
                      int numChannels, int numSamples,
                      const Params& params) noexcept
    {
        const int ch = std::min(numChannels, numCh);

        // [FIX 2] ブロック先頭で係数を一回だけ更新
        updateCoeffsIfNeeded(params);

        // [FIX 1] 外ループ = チャンネル、内ループ = サンプル
        for (int c = 0; c < ch; ++c)
            for (int n = 0; n < numSamples; ++n)
                output[c][n] = process(c, modIn[c][n], carIn[c][n], params);
    }

    // --------------------------------------------------------
    // Accessors
    // --------------------------------------------------------
    double getEnvelope(int channel) const noexcept
    {
        if (channel < 0 || channel >= numCh) return 0.0;
        return envState[(size_t)channel];
    }

    double attackMs()  const noexcept { return spec.R_att * spec.C_env * 1000.0; }
    double releaseMs() const noexcept { return spec.R_rel * spec.C_env * 1000.0; }

private:
    // --------------------------------------------------------
    // OTA SVF state
    // --------------------------------------------------------
    struct SVFState
    {
        double lp = 0.0;
        double bp = 0.0;
        double hp = 0.0;
    };

    // --------------------------------------------------------
    // [FIX 2] 係数キャッシュ — params変化時のみ再計算
    // --------------------------------------------------------
    double cachedGCoeff    = 0.0;
    double cachedCarGCoeff = 0.0;
    double cachedDamp      = 0.0;
    float  lastCenterHz    = -1.0f;
    float  lastQ           = -1.0f;
    float  lastTemperature = -999.0f;

    inline void updateCoeffsIfNeeded(const Params& params) noexcept
    {
        if (params.centerHz   == lastCenterHz   &&
            params.q          == lastQ          &&
            params.temperature == lastTemperature)
            return;

        const double modGm = modOta.gmScale(params.temperature);
        const double carGm = carOta.gmScale(params.temperature);
        cachedGCoeff    = computeGCoeff(params.centerHz, modGm);
        cachedCarGCoeff = computeGCoeff(params.centerHz, carGm);
        cachedDamp      = computeDamp(params.q);

        lastCenterHz    = params.centerHz;
        lastQ           = params.q;
        lastTemperature = params.temperature;
    }

    // --------------------------------------------------------
    // computeGCoeff
    // --------------------------------------------------------
    inline double computeGCoeff(float centerHz, double gmScale) const noexcept
    {
        double fc = std::clamp((double)centerHz, 20.0, sr * 0.49);
        double g  = 2.0 * std::sin(M_PI * fc / sr);
        return std::clamp(g * gmScale, 1e-6, 0.99);
    }

    inline double computeDamp(float q) const noexcept
    {
        double qClamped = std::clamp((double)q, 0.5, 30.0);
        return 1.0 / qClamped;
    }

    // --------------------------------------------------------
    // processSVF — Andy Simper trapezoidal SVF
    // --------------------------------------------------------
    inline double processSVF(SVFState& st,
                              const OTA_Primitive& ota,
                              double in,
                              double g,
                              double damp) const noexcept
    {
        double denom = 1.0 + g * (damp + g);
        if (denom < 1e-10) denom = 1e-10;

        double hp  = (in - damp * st.bp - st.lp) / denom;
        double v1  = g * hp;

        double newBp = ota.integrate(st.bp, st.bp + v1, 1.0);
        double v2    = g * newBp;
        double newLp = ota.integrate(st.lp, st.lp + v2, 1.0);

        st.hp = hp;
        st.bp = newBp;
        st.lp = newLp;

        return st.bp;
    }

    // --------------------------------------------------------
    // Members
    // --------------------------------------------------------
    OTA_Primitive   modOta;
    OTA_Primitive   carOta;
    DiodePrimitive  rectDiode;
    RC_Element      attackRC;
    RC_Element      releaseRC;
    VcaPrimitive    vca;

    Spec   spec;
    double sr    = 44100.0;
    int    numCh = 2;

    std::vector<SVFState> modState;
    std::vector<SVFState> carState;
    std::vector<double>   envState;
};
