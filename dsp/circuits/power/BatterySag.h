#pragma once
#include <cmath>
#include <algorithm>
#include <vector>
#include "../../constants/PartsConstants.h"
#include "../../core/ProcessSpec.h"

// 9V battery sag (dying battery effect)
// Fuzz Face / Rangemaster / Tube Screamer commonly seen in pedals such as
// Models supply voltage drop + internal resistance increase from battery depletion
//
// Physical model:
//   V_bat(life) = V_fresh × (life^curve)           — Discharge curve
//   R_int(life) = R_fresh / max(life, 0.01)         — Internal resistance increase
//   V_out = V_bat - I_load × R_int                  — Voltage drop under load
//
// Characteristics by battery type:
//   Alkaline:    V_fresh=9.6V, R_fresh=3Ω,  Gradual decline then sharp drop at end
//   CarbonZinc:  V_fresh=9.0V, R_fresh=10Ω, Gradual decline from start (vintage)
//   Rechargeable: V_fresh=8.4V, R_fresh=1Ω,  Holds flat then sharp drop
//
// Output: effective supply voltage (V) — directly connectable to InputBuffer.setSupplyVoltage() etc.
//
// Differences from PowerSupplySag (for tube amplifiers):
//   - This is for pedals in the 5V–10V range
//   - Real-time control via battery life parameter
//   - Dynamic voltage drop via envelope tracking (sags the moment you play)
class BatterySag
{
public:
    enum BatteryType : int
    {
        Alkaline     = 0,   // Duracell type — 9.6V, low internal resistance, sharp drop at end
        CarbonZinc   = 1,   // Vintage — 9.0V, high internal resistance, gradual discharge
        Rechargeable = 2,   // NiMH type — 8.4V, very low internal resistance, flat→sharp drop
        kBatteryTypeCount
    };

    struct BatterySpec
    {
        const char* name;
        double freshVoltage;    // Fresh voltage (V)
        double deadVoltage;     // End-of-life voltage (V)
        double freshResistance; // Fresh internal resistance (Ω)
        double dischargeCurve;  // Discharge curve exponent (larger=flat→sharp drop)
    };

    static constexpr BatterySpec batterySpecs[kBatteryTypeCount] = {
        { "Alkaline",     9.6, 5.5, 3.0,  0.3 },
        { "Carbon-Zinc",  9.0, 5.0, 10.0, 0.6 },
        { "Rechargeable", 8.4, 6.0, 1.0,  0.1 },
    };

    struct Params
    {
        int   batteryType   = Alkaline;  // Battery type
        float batteryLife   = 1.0f;      // Battery life (0.0=empty, 1.0=full charge)
        float loadCurrentMa = 10.0f;     // Circuit current draw (mA) — typical pedal: 5–30mA
        float sagAmount     = 1.0f;      // Dynamic sag amount (0.0=none, 1.0=full)
        float attackMs      = 5.0f;      // Dynamic sag attack (ms)
        float releaseMs     = 50.0f;     // Dynamic sag release (ms)
    };

    BatterySag() noexcept = default;

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        envState.assign(nCh, 0.0);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        std::fill(envState.begin(), envState.end(), 0.0);
    }

    // Get static battery voltage (no load)
    static inline double getOpenCircuitVoltage(int type, float life) noexcept
    {
        const auto& spec = batterySpecs[std::clamp(type, 0, (int)kBatteryTypeCount - 1)];
        const double l = std::clamp((double)life, 0.0, 1.0);
        // Discharge curve: V = V_dead + (V_fresh - V_dead) × life^curve
        const double range = spec.freshVoltage - spec.deadVoltage;
        return spec.deadVoltage + range * std::pow(l, spec.dischargeCurve);
    }

    // Get static internal resistance
    static inline double getInternalResistance(int type, float life) noexcept
    {
        const auto& spec = batterySpecs[std::clamp(type, 0, (int)kBatteryTypeCount - 1)];
        const double l = std::clamp((double)life, 0.01, 1.0);
        // Internal resistance: R = R_fresh / life (resistance increases with depletion)
        return spec.freshResistance / l;
    }

    // single sample processing: Track dynamic current demand from input signal and return effective voltage
    // x: Audio input
    // Return value: effective supply voltage (V)
    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)envState.size() - 1);
        const int type = std::clamp(params.batteryType, 0, (int)kBatteryTypeCount - 1);
        const auto& spec = batterySpecs[type];

        // --- Open-circuit voltage / internal resistance ---
        const double Voc = getOpenCircuitVoltage(type, params.batteryLife);
        const double Rint = getInternalResistance(type, params.batteryLife);

        // --- Static voltage drop (circuit idle current portion)---
        const double iIdle = std::max(0.0, (double)params.loadCurrentMa) * 0.001;
        const double Vstatic = std::max(0.0, Voc - iIdle * Rint);

        // --- Dynamic sag (input signal envelope → additional current demand)---
        const double sagAmt = std::clamp((double)params.sagAmount, 0.0, 1.0);
        if (sagAmt < 1e-6)
            return (float)Vstatic;

        const double attAlpha = msToAlpha(std::max(0.1f, params.attackMs));
        const double relAlpha = msToAlpha(std::max(0.1f, params.releaseMs));

        const double absIn = std::abs((double)x);
        double& env = envState[ch];
        if (absIn > env)
            env += attAlpha * (absIn - env);
        else
            env += relAlpha * (absIn - env);

        // Estimate dynamic current from envelope
        // Assume 3x idle current at peak signal
        const double iDynamic = env * iIdle * 3.0 * sagAmt;
        const double Vout = std::max(0.0, Voc - (iIdle + iDynamic) * Rint);

        return (float)Vout;
    }

    // Block processing: input passthrough + output effective voltage per sample to voltageOut
    void processBlock(const float* const* input, float* const* output,
                      float* voltageOut,   // [numSamples] effective voltage output (V)
                      int numChannels, int numSamples, const Params& params) noexcept
    {
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
        }
    }

    // Current effective voltage (for UI display) — approximate value from last process() call
    float getVoltage(int type, float life, float loadCurrentMa) const noexcept
    {
        const double Voc = getOpenCircuitVoltage(type, life);
        const double Rint = getInternalResistance(type, life);
        const double iLoad = std::max(0.0, (double)loadCurrentMa) * 0.001;
        return (float)std::max(0.0, Voc - iLoad * Rint);
    }

    // Helper: convert battery life to normalized sag coefficient (0–1)
    // 1.0=rated, 0.0=minimum voltage
    static inline float voltageToSagLevel(float voltage) noexcept
    {
        // Normalized in 5V–10V range
        return (float)std::clamp(((double)voltage - 5.0) / 5.0, 0.0, 1.0);
    }

    // spec accessors
    static const BatterySpec& getBatterySpec(int type) noexcept
    {
        return batterySpecs[std::clamp(type, 0, (int)kBatteryTypeCount - 1)];
    }

private:
    inline double msToAlpha(float ms) const noexcept
    {
        if (ms <= 0.0f) return 1.0;
        return 1.0 - std::exp(-1.0 / (sampleRate * ms * 0.001));
    }

    double sampleRate = PartsConstants::defaultSampleRate;
    std::vector<double> envState;   // Envelope detector state
};
