#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"
#include "../../parts/RC_Element.h"
#include "../../parts/DiodePrimitive.h"
#include "../../parts/OTA_Primitive.h"
#include "../../parts/BJT_Primitive.h"

// analog ADSR envelope generator — full CEM 3310 / SSM 2056 emulation
//
// L2 component configuration:
//   DiodePrimitive (Si1N4148) — attack/release switching diode
//     real circuit: compares capacitor voltage with input → diode switch for charge or discharge path
//     Effect: voltage drop by diode Vf during attack→decay transition (unique to analog EG
//            curve rounding just before peak), temperature dependence
//   OTA_Primitive (CA3080) — charging current source
//     real circuit: comparator/current control cell inside CEM3310
//     Effect: Attack curve compressed at large signals due to gm saturation of charging current,
//            Temperature drift causes time-constant variation; manufacturing tolerances create unit-to-unit differences
//   BJT_Primitive (Matched) — current mirror
//     Actual circuit: Current mirror generating charging current from timing resistance
//     Effect: Minor attack/decay asymmetry from pair mismatch,
//            Temperature-dependent Vbe drift
//   RC_Element — timing RC circuit
//     Actual circuit: external timing capacitor (10uF tantalum) + resistance
//     effect: physical basis for exponential charge/discharge, capacitor leakage
//
// real circuit topology (per CEM3310 block diagram):
//
//   Gate ON → OTA(CA3080) current source ON
//             ↓ (gm-controlled charging current)
//   BJT current mirror → DiodeBridge(1N4148) → ┬─ R_att (charge path) ─┬─ C (10μF)
//                                                                                                    └─ R_rel (discharge path) ─┘ tantalum
//                                                                         ↓
//   comparator(OTA): C voltage ≥ Vref(≈1.3V) → transition to Decay
//   Gate OFF → Release: C → R_rel → GND (discharge)
//
// 4-layer architecture:
//   Parts: DiodePrimitive + OTA_Primitive + BJT_Primitive + RC_Element
//   → Circuit: EnvelopeGenerator (ADSR state machine)
class EnvelopeGenerator
{
public:
    enum class Stage { Idle = 0, Attack, Decay, Sustain, Release };
    enum class Mode  { ADSR = 0, AD = 1, AR = 2 };
    enum class Curve { RC = 0, Linear = 1 };

    struct Params
    {
        float attack   = 0.3f;    // 0.0–1.0 → 0.5ms ~ 5000ms
        float decay    = 0.3f;    // 0.0–1.0 → 5ms ~ 5000ms
        float sustain  = 0.7f;    // 0.0–1.0 sustain level
        float release  = 0.4f;    // 0.0–1.0 → 10ms ~ 10000ms
        int   mode     = 0;       // 0=ADSR, 1=AD, 2=AR
        int   curve    = 0;       // 0=RC (exponential), 1=Linear
        float temperature = 25.0f; // operating temperature (°C)
    };

    EnvelopeGenerator() noexcept
        : switchDiode(DiodePrimitive::Si1N4148())   // stage-switching diode
        , currentSource(OTA_Primitive::CA3080(), 313) // charging current source OTA
        , currentMirror(BJT_Primitive::Matched(), 417) // current mirror BJT pair
    {}

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        chState.resize(nCh);
        for (auto& st : chState)
        {
            st = ChannelState{};
            st.timingRC = RC_Element(kDefaultR, kEnvCap);
            st.timingRC.prepare(sampleRate);
        }
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        for (auto& st : chState)
        {
            st = ChannelState{};
            st.timingRC = RC_Element(kDefaultR, kEnvCap);
            st.timingRC.prepare(sampleRate);
        }
    }

    // gate ON — start attack stage
    void gateOn(int channel) noexcept
    {
        if (channel < 0 || (size_t)channel >= chState.size()) return;
        auto& st = chState[(size_t)channel];
        st.stage = Stage::Attack;
        st.gateHeld = true;
    }

    // gate OFF — start release stage
    void gateOff(int channel) noexcept
    {
        if (channel < 0 || (size_t)channel >= chState.size()) return;
        auto& st = chState[(size_t)channel];
        st.gateHeld = false;
        if (st.stage != Stage::Idle)
            st.stage = Stage::Release;
    }

    // all-channel gate control
    void gateOnAll() noexcept  { for (int i = 0; i < (int)chState.size(); ++i) gateOn(i); }
    void gateOffAll() noexcept { for (int i = 0; i < (int)chState.size(); ++i) gateOff(i); }

    // advance 1 sample — returns envelope control voltage (0.0–1.0)
    inline float process(int channel, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)chState.size() - 1);
        auto& st = chState[ch];

        const auto mode = static_cast<Mode>(std::clamp(params.mode, 0, 2));
        const auto curveType = static_cast<Curve>(std::clamp(params.curve, 0, 1));
        const double sustainLevel = std::clamp((double)params.sustain, 0.0, 1.0);
        const double temp = (double)params.temperature;

        // === temperature/characteristic calculation by L2 components ===

        // OTA gm temperature scale: CA3080's gm decreases at high temperature → charging slows
        const double otaGmScale = currentSource.gmScale(temp);

        // BJT Vbe temperature drift: reduced current mirror accuracy → minor change in charging current
        const double bjtTempScale = currentMirror.tempScale(temp);

        // diode Vf temperature dependence: Vf decreases at high temperature → stage transition threshold shifts slightly
        const double diodeVf = switchDiode.effectiveVf(temp);

        // combined temperature scale = OTA gm × BJT Vbe drift
        const double tempScale = otaGmScale * bjtTempScale;

        // peak transition threshold variation due to diode Vf
        // CEM3310: comparator triggers Decay when (Vcap - Vf_diode) ≥ Vref
        // → transition occurs slightly earlier when Vf drops (at high temperature)
        const double peakThreshold = 1.0 - (diodeVf - kNominalDiodeVf) * kDiodeVfInfluence;

        // BJT current mirror mismatch: attack/decay asymmetry
        const double mirrorMismatch = currentMirror.getMismatch();

        switch (st.stage)
        {
            case Stage::Idle:
                // idle — capacitor leakage
                st.envValue *= kLeakFactor;
                if (st.envValue < 1e-6) st.envValue = 0.0;
                break;

            case Stage::Attack:
            {
                const double attackMs = paramToAttackMs(params.attack) * tempScale;

                if (curveType == Curve::RC)
                {
                    // === OTA current source + RC charging ===
                    // OTA gm saturation: charging current saturates on large signals → curve compressed
                    // capacitor charges toward target kAttackOvershoot (≈1.3)
                    const double rawDelta = kAttackOvershoot - st.envValue;

                    // apply OTA gm saturation: the larger the difference from target, the more current is compressed
                    const double otaSaturated = currentSource.saturate(rawDelta);

                    // IIR coefficient from RC time constant
                    const double alpha = msToAlpha(attackMs);

                    // current mirror mismatch: minor deviation in charging speed
                    st.envValue += alpha * otaSaturated * mirrorMismatch;
                }
                else
                {
                    // linear curve (no OTA saturation, BJT mirror mismatch only)
                    const double inc = mirrorMismatch / std::max(1.0, attackMs * sampleRate * 0.001);
                    st.envValue += inc;
                }

                // peak transition accounting for diode Vf
                if (st.envValue >= peakThreshold)
                {
                    st.envValue = std::min(st.envValue, 1.0);
                    if (mode == Mode::AD || mode == Mode::ADSR)
                        st.stage = Stage::Decay;
                    else // AR
                        st.stage = st.gateHeld ? Stage::Sustain : Stage::Release;
                }
                break;
            }

            case Stage::Decay:
            {
                const double decayMs = paramToDecayMs(params.decay) * tempScale;
                const double target = (mode == Mode::AD) ? 0.0 : sustainLevel;

                if (curveType == Curve::RC)
                {
                    // === RC discharge + OTA gm saturation ===
                    const double rawDelta = target - st.envValue;
                    const double otaSaturated = currentSource.saturate(rawDelta);
                    const double alpha = msToAlpha(decayMs);

                    // decay direction: mirror mismatch is reversed (2.0 - mismatch)
                    const double decayMismatch = 2.0 - mirrorMismatch;
                    st.envValue += alpha * otaSaturated * decayMismatch;
                }
                else
                {
                    const double dec = mirrorMismatch / std::max(1.0, decayMs * sampleRate * 0.001);
                    double diff = target - st.envValue;
                    if (std::abs(diff) > dec)
                        st.envValue += (diff > 0 ? dec : -dec);
                    else
                        st.envValue = target;
                }

                if (mode == Mode::AD)
                {
                    if (st.envValue < 0.001)
                    {
                        st.envValue = 0.0;
                        st.stage = Stage::Idle;
                    }
                }
                else
                {
                    // include diode Vf offset in sustain level judgment
                    const double sustainTolerance = 0.001 + (diodeVf - kNominalDiodeVf) * 0.01;
                    if (std::abs(st.envValue - sustainLevel) < std::max(0.001, sustainTolerance))
                    {
                        st.envValue = sustainLevel;
                        st.stage = st.gateHeld ? Stage::Sustain : Stage::Release;
                    }
                }

                if (!st.gateHeld && mode != Mode::AD)
                    st.stage = Stage::Release;
                break;
            }

            case Stage::Sustain:
            {
                st.envValue = sustainLevel;
                // capacitor leakage (minute)
                st.envValue *= (1.0 - kLeakRate);
                if (!st.gateHeld)
                    st.stage = Stage::Release;
                break;
            }

            case Stage::Release:
            {
                const double releaseMs = paramToReleaseMs(params.release) * tempScale;

                if (curveType == Curve::RC)
                {
                    // === diode switching → discharge path ===
                    // release diode: "dead zone" in discharge current equivalent to diode Vf
                    // → release curve becomes gentle near zero (characteristic of analog EGs)
                    const double rawDelta = 0.0 - st.envValue;

                    // diode switch: current drops sharply when voltage falls below Vf
                    // this produces the "gradual taper" at the tail of analog EG release
                    double diodeFactor = 1.0;
                    if (st.envValue < diodeVf * kDiodeReleaseScale)
                    {
                        // diode I-V characteristic: current decreases exponentially below Vf
                        diodeFactor = switchDiode.saturate(st.envValue / kDiodeReleaseScale, temp)
                                      / std::max(1e-10, st.envValue / kDiodeReleaseScale);
                        diodeFactor = std::clamp(std::abs(diodeFactor), 0.05, 1.0);
                    }

                    const double alpha = msToAlpha(releaseMs);
                    st.envValue += alpha * rawDelta * diodeFactor;
                }
                else
                {
                    const double dec = st.envValue / std::max(1.0, releaseMs * sampleRate * 0.001);
                    st.envValue -= dec;
                }

                if (st.envValue < 0.001)
                {
                    st.envValue = 0.0;
                    st.stage = Stage::Idle;
                }

                // retrigger: gate ON during release returns to attack
                if (st.gateHeld)
                    st.stage = Stage::Attack;
                break;
            }
        }

        st.envValue = std::clamp(st.envValue, 0.0, 1.0);
        return (float)st.envValue;
    }

    // get current envelope value (for UI display)
    float getEnvelope(int channel) const noexcept
    {
        if (channel < 0 || (size_t)channel >= chState.size()) return 0.0f;
        return (float)chState[(size_t)channel].envValue;
    }

    // get current stage
    Stage getStage(int channel) const noexcept
    {
        if (channel < 0 || (size_t)channel >= chState.size()) return Stage::Idle;
        return chState[(size_t)channel].stage;
    }

    // gate state
    bool isGateHeld(int channel) const noexcept
    {
        if (channel < 0 || (size_t)channel >= chState.size()) return false;
        return chState[(size_t)channel].gateHeld;
    }

    // L2 component access (for diagnostics)
    const DiodePrimitive& getSwitchDiode()    const noexcept { return switchDiode; }
    const OTA_Primitive&  getCurrentSource()  const noexcept { return currentSource; }
    const BJT_Primitive&  getCurrentMirror()  const noexcept { return currentMirror; }

private:
    // === L2 component layer ===
    DiodePrimitive switchDiode;    // stage-switching diode (Si 1N4148)
    OTA_Primitive  currentSource;  // charging current source OTA (CA3080)
    BJT_Primitive  currentMirror;  // current mirror BJT (matched pair)

    // === envelope circuit constants ===
    // CEM3310 style: C = 10μF tantalum capacitor
    static constexpr double kEnvCap       = 10.0e-6;
    static constexpr double kDefaultR     = 1000.0;

    // RC charge overshoot (CEM3310 design: ~1.3x target)
    static constexpr double kAttackOvershoot = 1.3;

    // capacitor leak coefficient (per sample)
    static constexpr double kLeakRate     = 1e-7;
    static constexpr double kLeakFactor   = 1.0 - kLeakRate;

    // diode-related constants
    static constexpr double kNominalDiodeVf    = 0.65;  // 1N4148 reference Vf @ 25°C
    static constexpr double kDiodeVfInfluence  = 0.15;  // Vf influence ratio of variation on peak threshold
    static constexpr double kDiodeReleaseScale = 0.3;   // diode dead zone at release tail

    // === timing mapping ===
    // Attack: 0.0–1.0 → 0.5ms – 5000ms (quadratic curve)
    static constexpr double kAttackMsMin  = 0.5;
    static constexpr double kAttackMsMax  = 5000.0;
    // Decay:  0.0–1.0 → 5ms – 5000ms
    static constexpr double kDecayMsMin   = 5.0;
    static constexpr double kDecayMsMax   = 5000.0;
    // Release: 0.0–1.0 → 10ms – 10000ms
    static constexpr double kReleaseMsMin = 10.0;
    static constexpr double kReleaseMsMax = 10000.0;

    static inline double paramToAttackMs(float p) noexcept
    {
        double t = (double)p * (double)p;
        return kAttackMsMin + t * (kAttackMsMax - kAttackMsMin);
    }

    static inline double paramToDecayMs(float p) noexcept
    {
        double t = (double)p * (double)p;
        return kDecayMsMin + t * (kDecayMsMax - kDecayMsMin);
    }

    static inline double paramToReleaseMs(float p) noexcept
    {
        double t = (double)p * (double)p;
        return kReleaseMsMin + t * (kReleaseMsMax - kReleaseMsMin);
    }

    inline double msToAlpha(double ms) const noexcept
    {
        if (ms <= 0.0) return 1.0;
        return 1.0 - std::exp(-1.0 / (sampleRate * ms * 0.001));
    }

    struct ChannelState
    {
        RC_Element timingRC;
        Stage  stage    = Stage::Idle;
        double envValue = 0.0;
        bool   gateHeld = false;
    };

    double sampleRate = PartsConstants::defaultSampleRate;
    std::vector<ChannelState> chState;
};
