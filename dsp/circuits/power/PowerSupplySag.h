#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../constants/PartsConstants.h"
#include "../../core/ProcessSpec.h"

// Classic tube rectifier power sag model (GZ34)
// - B+ voltage droop (load-current dependent)
// - Track output current demand with envelope follower
// - Filter capacitor discharge/charge time constant model
// - Ripple (50/60Hz hum) injection
// - Voltage drop due to internal rectifier tube resistance
//
// Output: normalized supply voltage coefficient (0.0–1.0)
//   1.0 = Rated B+ voltage (at idle)
//   0.0 = Full sag (theoretical maximum load)
//   Typical operating range: 0.7–1.0
//
// usage: processBlock() Reflect output to other modules' supplyVoltage parameter
//   Actual voltage = V_nominal * sagOutput  (e.g.: 450V × 0.85 = 382.5V)
//   For pedals: supplyV = lerp(V_supplyMin, V_supplyMax, sagOutput)
class PowerSupplySag
{
public:
    struct Params
    {
        // Rectifier tube model parameters
        float rectifierResistance = 25.0f;   // Rectifier tube internal resistance (Ω)  GZ34≈25Ω, 5Y3≈60Ω, 5U4≈50Ω
        float filterCapUf         = 50.0f;   // Filter capacitor value (µF)  classic tube amp≈50µF
        float idleCurrentMa       = 80.0f;   // Idle current (mA)
        float maxCurrentMa        = 250.0f;  // Maximum load current (mA)

        // Envelope detection (current demand tracking)
        float attackMs            = 2.0f;    // Attack time (ms) — current rise speed
        float releaseMs           = 80.0f;   // Release time (ms) — filter charge recovery

        // Sag depth control
        float sagDepth            = 1.0f;    // Sag amount (0.0=none, 1.0=full)

        // Ripple
        float rippleHz            = 100.0f;  // Ripple frequency (Hz)  50Hz full-wave rectification=100Hz
        float rippleDepth         = 0.002f;  // Ripple depth (0.0–0.05)

        // Temperature
        float temperature         = 25.0f;   // Ambient temperature (°C) — affects rectifier tube resistance
    };

    PowerSupplySag() noexcept = default;

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        envState.assign(nCh, 0.0);
        capVoltage.assign(nCh, 1.0);  // Initial state = rated voltage
        ripplePhase = 0.0;
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        std::fill(envState.begin(), envState.end(), 0.0);
        std::fill(capVoltage.begin(), capVoltage.end(), 1.0);
        ripplePhase = 0.0;
    }

    // single sample processing: Estimate current demand from input signal level and return sag coefficient
    // x: Input audio sample (assuming amplitude proportional to load current)
    // Return value: supply voltage coefficient (0.0–1.0)  1.0=rated, lower=sag
    inline float process(int channel, float x, const Params& params) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)envState.size() - 1);

        // --- Envelope detection (tracking output current demand)---
        const double attAlpha = msToAlpha(std::max(0.1f, params.attackMs));
        const double relAlpha = msToAlpha(std::max(0.1f, params.releaseMs));

        const double input = std::abs((double)x);
        double& env = envState[ch];
        if (input > env)
            env += attAlpha * (input - env);
        else
            env += relAlpha * (input - env);

        // --- Calculate current demand (envelope → normalized current 0–1)---
        const double currentDemand = std::clamp(env, 0.0, 1.0);

        // --- Rectifier tube voltage drop model ---
        // Rectifier tube resistance variation with temperature (temp coeff +0.2%/°C)
        const double tempFactor = 1.0 + 0.002 * ((double)params.temperature - 25.0);
        const double Rrect = (double)params.rectifierResistance * std::max(0.5, tempFactor);

        // Load current (A)
        const double iLoad = ((double)params.idleCurrentMa
                              + currentDemand * ((double)params.maxCurrentMa - (double)params.idleCurrentMa))
                             * 0.001;

        // Voltage drop from rectifier tube (normalized) — proportional as I×R / V_nominal
        // Normalized with V_nominal as 450V (classic tube amp B+)
        constexpr double V_nominal = 450.0;
        const double vDrop = (iLoad * Rrect) / V_nominal;

        // --- Filter capacitor charge/discharge ---
        // Time constant: τ = R_rect × C_filter
        const double C_filter = (double)params.filterCapUf * 1e-6;
        const double tau = Rrect * C_filter;
        const double capAlpha = (tau > 1e-9)
                                    ? (1.0 - std::exp(-1.0 / (sampleRate * tau)))
                                    : 1.0;

        // Capacitor voltage drops under load and recovers via charging through rectifier tube
        const double targetVoltage = std::max(0.0, 1.0 - vDrop);
        double& vCap = capVoltage[ch];
        vCap += capAlpha * (targetVoltage - vCap);

        // --- Ripple injection ---
        double ripple = 0.0;
        if (params.rippleDepth > 0.0f)
        {
            const double rippleFreq = std::clamp((double)params.rippleHz, 10.0, 500.0);
            ripple = std::sin(ripplePhase) * (double)params.rippleDepth;
            // Heavier load produces larger ripple
            ripple *= (0.3 + 0.7 * currentDemand);
        }

        // --- Sag depth control ---
        const double sagAmount = std::clamp((double)params.sagDepth, 0.0, 1.0);
        const double saggedVoltage = 1.0 - sagAmount * (1.0 - vCap) + ripple;

        return (float)std::clamp(saggedVoltage, 0.0, 1.0);
    }

    // Block processing: pass through input and output sag coefficient per sample to sagOut
    void processBlock(const float* const* input, float* const* output,
                      float* sagOut,   // [numSamples] sag coefficient output
                      int numChannels, int numSamples, const Params& params) noexcept
    {
        const double rippleFreq = std::clamp((double)params.rippleHz, 10.0, 500.0);
        const double phaseInc = 2.0 * 3.14159265358979323846 * rippleFreq / sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            float maxEnv = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                output[ch][i] = input[ch][i];
                float sag = process(ch, input[ch][i], params);
                if (ch == 0 || sag < maxEnv) maxEnv = sag;  // Use the channel with deepest sag
            }
            if (sagOut) sagOut[i] = maxEnv;

            // Advance ripple phase (shared across all channels)
            ripplePhase += phaseInc;
            if (ripplePhase > 2.0 * 3.14159265358979323846)
                ripplePhase -= 2.0 * 3.14159265358979323846;
        }
    }

    // Get current sag coefficient (for UI display)
    float getSagLevel(int channel) const noexcept
    {
        if (channel < 0 || (size_t)channel >= capVoltage.size()) return 1.0f;
        return (float)capVoltage[(size_t)channel];
    }

    // Helper: map sag coefficient to pedal supply voltage range
    static inline double sagToSupplyVoltage(float sagLevel) noexcept
    {
        const double t = std::clamp((double)sagLevel, 0.0, 1.0);
        return PartsConstants::V_supplyMin + t * (PartsConstants::V_supplyMax - PartsConstants::V_supplyMin);
    }

    // Helper: map sag coefficient to tube amplifier B+ voltage
    static inline double sagToBPlusVoltage(float sagLevel, double nominalBPlus = 450.0) noexcept
    {
        return nominalBPlus * std::clamp((double)sagLevel, 0.0, 1.0);
    }

private:
    inline double msToAlpha(float ms) const noexcept
    {
        if (ms <= 0.0f) return 1.0;
        return 1.0 - std::exp(-1.0 / (sampleRate * ms * 0.001));
    }

    double sampleRate = PartsConstants::defaultSampleRate;
    std::vector<double> envState;      // Envelope detector state
    std::vector<double> capVoltage;    // Normalized filter capacitor voltage
    double ripplePhase = 0.0;          // Ripple oscillator phase
};
