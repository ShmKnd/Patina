#pragma once
#include <cmath>
#include <algorithm>
#include <vector>
#include "../../constants/PartsConstants.h"
#include "../../core/ProcessSpec.h"

// DC adapter power supply sag model
// Emulates power characteristics of 9V / 18V pedal DC adapters
//
// Characteristics by adapter type:
//   Linear9V:       Linear regulator 9V — low ripple, gentle load regulation
//   Linear18V:      Linear regulator 18V — high headroom, low ripple
//   Switching9V:    Switching regulator 9V — tight, high-frequency ripple
//   Switching18V:   Switching regulator 18V — high headroom, switching noise
//   Unregulated9V:  Unregulated 9V (cheap adapter) — large voltage fluctuation under load
//   Unregulated18V: Unregulated 18V — large ripple + sag
//
// Physical model:
//   V_out = V_nominal - I_load × R_out - V_ripple(t)
//   Ripple: linear→100/120Hz (full-wave rectification), switching→50-200kHz
//   Load regulation: regulated→ ±2-5%, unregulated→ ±15-30%
//
// Differences from BatterySag:
//   - No battery life concept (wall power is constant)
//   - Instead, regulation quality and ripple characteristics are modeled in detail
//   - Supports both 9V and 18V
class AdapterSag
{
public:
    enum AdapterType : int
    {
        Linear9V       = 0,  // Regulated linear 9V (Boss PSA type)
        Linear18V      = 1,  // Regulated linear 18V (high headroom)
        Switching9V    = 2,  // Regulated switching 9V (modern compact)
        Switching18V   = 3,  // Regulated switching 18V (Strymon type)
        Unregulated9V  = 4,  // Unregulated 9V (cheap wall wart)
        Unregulated18V = 5,  // Unregulated 18V
        kAdapterTypeCount
    };

    struct AdapterSpec
    {
        const char* name;
        double nominalVoltage;    // Nominal voltage (V)
        double outputResistance;  // Output impedance (Ω) — load regulation
        double rippleFreqHz;      // Ripple fundamental frequency (Hz)
        double rippleAmplitude;   // Ripple amplitude (Vpp, no-load)
        double maxCurrentMa;      // Maximum supply current (mA)
        double currentLimitKnee;  // Current limit onset point (ratio to maxCurrent)
        bool   isRegulated;       // Whether regulated
    };

    static constexpr AdapterSpec adapterSpecs[kAdapterTypeCount] = {
        //                      name              Vnom   Rout  ripFreq  ripAmp  maxmA  knee   reg
        /* Linear9V       */ { "Linear 9V",       9.0,   0.5,  100.0,  0.010,  500,   0.85,  true  },
        /* Linear18V      */ { "Linear 18V",      18.0,  0.8,  100.0,  0.015,  500,   0.85,  true  },
        /* Switching9V    */ { "Switching 9V",     9.0,   0.2,  65000.0, 0.030, 1000,  0.90,  true  },
        /* Switching18V   */ { "Switching 18V",    18.0,  0.3,  65000.0, 0.040, 1000,  0.90,  true  },
        /* Unregulated9V  */ { "Unregulated 9V",   9.6,   5.0,  100.0,  0.200,  300,   0.70,  false },
        /* Unregulated18V */ { "Unregulated 18V",  19.2,  8.0,  100.0,  0.350,  300,   0.70,  false },
    };

    struct Params
    {
        int   adapterType    = Linear9V;  // Adapter type
        float loadCurrentMa  = 20.0f;     // Circuit current draw (mA)
        float sagAmount      = 1.0f;      // Dynamic sag amount (0.0=none, 1.0=full)
        float attackMs       = 3.0f;      // Dynamic sag attack (ms)
        float releaseMs      = 30.0f;     // Dynamic sag release (ms)
        float rippleMix      = 1.0f;      // Ripple injection amount (0.0=none, 1.0=full)
        float mainsHz        = 50.0f;     // Mains frequency (50Hz=Japan/Europe, 60Hz=North America)
    };

    AdapterSag() noexcept = default;

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        envState.assign(nCh, 0.0);
        ripplePhase = 0.0;
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        std::fill(envState.begin(), envState.end(), 0.0);
        ripplePhase = 0.0;
    }

    // Get static output voltage (without dynamic sag)
    static inline double getStaticVoltage(int type, float loadCurrentMa) noexcept
    {
        const auto& spec = adapterSpecs[std::clamp(type, 0, (int)kAdapterTypeCount - 1)];
        const double iLoad = std::max(0.0, (double)loadCurrentMa) * 0.001;
        const double iMax = spec.maxCurrentMa * 0.001;

        // Load regulation: V_out = V_nom - I × R_out
        double vOut = spec.nominalVoltage - iLoad * spec.outputResistance;

        // Regulated adapter maintains regulation within rated range
        if (spec.isRegulated && iLoad < iMax * spec.currentLimitKnee)
        {
            // Within regulation range: voltage drop greatly suppressed
            vOut = spec.nominalVoltage - iLoad * spec.outputResistance * 0.1;
        }

        // Foldback when current limit exceeded
        if (iLoad > iMax * spec.currentLimitKnee)
        {
            const double excess = (iLoad - iMax * spec.currentLimitKnee)
                                / (iMax * (1.0 - spec.currentLimitKnee) + 1e-9);
            const double foldback = std::clamp(excess, 0.0, 1.0);
            vOut -= foldback * spec.nominalVoltage * 0.5;
        }

        return std::max(0.0, vOut);
    }

    // single sample processing: Track dynamic current demand from input signal and return effective voltage
    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)envState.size() - 1);
        const int type = std::clamp(params.adapterType, 0, (int)kAdapterTypeCount - 1);
        const auto& spec = adapterSpecs[type];

        // --- Static voltage (idle current portion)---
        const double Vstatic = getStaticVoltage(type, params.loadCurrentMa);

        // --- Dynamic sag ---
        const double sagAmt = std::clamp((double)params.sagAmount, 0.0, 1.0);
        double Vdynamic = Vstatic;

        if (sagAmt > 1e-6)
        {
            const double attAlpha = msToAlpha(std::max(0.1f, params.attackMs));
            const double relAlpha = msToAlpha(std::max(0.1f, params.releaseMs));

            const double absIn = std::abs((double)x);
            double& env = envState[ch];
            if (absIn > env)
                env += attAlpha * (absIn - env);
            else
                env += relAlpha * (absIn - env);

            // Estimate additional current from envelope
            const double iIdle = std::max(0.001, (double)params.loadCurrentMa * 0.001);
            const double iDynamic = env * iIdle * 2.0 * sagAmt;

            // Dynamic voltage drop
            Vdynamic = std::max(0.0, Vstatic - iDynamic * spec.outputResistance
                                * (spec.isRegulated ? 1.0 : 5.0));
        }

        // --- Ripple injection ---
        const double rippleMix = std::clamp((double)params.rippleMix, 0.0, 1.0);
        double ripple = 0.0;
        if (rippleMix > 1e-6)
        {
            // Linear/unregulated: mains frequency × 2 (full-wave rectification)
            // Switching: high-frequency ripple (within reproducible range at sample rate)
            double rippleFreq = spec.rippleFreqHz;
            if (rippleFreq <= 200.0)
            {
                // Mains-based ripple — matched to mainsHz
                rippleFreq = (double)params.mainsHz * 2.0;  // Full-wave rectification
            }
            else
            {
                // Switching ripple — limited below Nyquist
                rippleFreq = std::min(rippleFreq, sampleRate * 0.4);
            }

            ripple = std::sin(ripplePhase) * spec.rippleAmplitude * rippleMix;

            // In unregulated mode, heavier load increases ripple
            if (!spec.isRegulated)
            {
                const double iLoad = std::max(0.0, (double)params.loadCurrentMa) * 0.001;
                const double iMax = spec.maxCurrentMa * 0.001;
                ripple *= (0.5 + 0.5 * std::clamp(iLoad / (iMax + 1e-9), 0.0, 1.0));
            }
        }

        return (float)std::clamp(Vdynamic + ripple, 0.0, spec.nominalVoltage * 1.1);
    }

    // Block processing: input passthrough + output effective voltage per sample to voltageOut
    void processBlock(const float* const* input, float* const* output,
                      float* voltageOut,
                      int numChannels, int numSamples, const Params& params) noexcept
    {
        const int type = std::clamp(params.adapterType, 0, (int)kAdapterTypeCount - 1);
        const auto& spec = adapterSpecs[type];

        // Calculate ripple phase increment
        double rippleFreq = spec.rippleFreqHz;
        if (rippleFreq <= 200.0)
            rippleFreq = (double)params.mainsHz * 2.0;
        else
            rippleFreq = std::min(rippleFreq, sampleRate * 0.4);
        const double phaseInc = 2.0 * 3.14159265358979323846 * rippleFreq / sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            float minV = 100.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                output[ch][i] = input[ch][i];
                float v = process(ch, input[ch][i], params);
                if (v < minV) minV = v;
            }
            if (voltageOut) voltageOut[i] = minV;

            ripplePhase += phaseInc;
            if (ripplePhase > 2.0 * 3.14159265358979323846)
                ripplePhase -= 2.0 * 3.14159265358979323846;
        }
    }

    // Get nominal voltage
    static inline double getNominalVoltage(int type) noexcept
    {
        return adapterSpecs[std::clamp(type, 0, (int)kAdapterTypeCount - 1)].nominalVoltage;
    }

    // Helper: convert voltage to 0–1 normalized sag level
    static inline float voltageToSagLevel(float voltage, int type) noexcept
    {
        const double vNom = getNominalVoltage(type);
        return (float)std::clamp((double)voltage / vNom, 0.0, 1.0);
    }

    // spec accessors
    static const AdapterSpec& getAdapterSpec(int type) noexcept
    {
        return adapterSpecs[std::clamp(type, 0, (int)kAdapterTypeCount - 1)];
    }

private:
    inline double msToAlpha(float ms) const noexcept
    {
        if (ms <= 0.0f) return 1.0;
        return 1.0 - std::exp(-1.0 / (sampleRate * ms * 0.001));
    }

    double sampleRate = PartsConstants::defaultSampleRate;
    std::vector<double> envState;
    double ripplePhase = 0.0;
};
