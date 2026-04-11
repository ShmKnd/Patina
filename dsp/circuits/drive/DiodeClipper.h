#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../../constants/PartsConstants.h"
#include "../../core/ProcessSpec.h"
#include "../../parts/DiodePrimitive.h"

// Diode clipper (extended with BBD-level analog behavior)
// - RCAnalog drive circuit emulation with high-frequency filter
// - Diode type selection (Si 1N4148 / Schottky 1N5818 / Ge OA91)
// - Temperature-dependent forward voltage (Vf ≈ -2mV/°C)
// - Diode series resistance model (dynamic impedance from bulk resistance)
// - Asymmetric clipping (different characteristics for positive/negative — manufacturing variation)
// - Frequency-dependent clipping due to junction capacitance
// - Reverse recovery transient response (high-frequency artifacts of Si diodes)
//
// 4-layer architecture:
//   Parts: DiodePrimitive (Si1N4148 / Schottky / Ge) — Vf/temperature/saturation/junction capacitance
//   → Circuit: DiodeClipper (RC HP + clip + reverse recovery)
class DiodeClipper
{
public:
    enum class Mode { Bypass = 0, Diode = 1, Tanh = 2 };

    // Diode type (based on actual component datasheet values)
    enum class DiodeType { Si1N4148 = 0, Schottky1N5818 = 1, GeOA91 = 2 };

    struct Params
    {
        float drive = 0.0f;
        int mode = 0;               // 0=Bypass, 1=Diode, 2=Tanh
        int diodeType = 0;          // 0=Si, 1=Schottky, 2=Ge
        float temperature = 25.0f;  // Junction temperature (°C)
    };

    DiodeClipper() noexcept
        : sampleRate(PartsConstants::defaultSampleRate), alphaHp(1.0),
          diodeSi(DiodePrimitive::Si1N4148()),
          diodeSchottky(DiodePrimitive::Schottky1N5818()),
          diodeGe(DiodePrimitive::GeOA91())
    {}

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = sr;
        const size_t nCh = (size_t)std::max(1, numChannels);
        prevIn.assign(nCh, 0.0f);
        hpPrevX.assign(nCh, 0.0);
        hpPrevY.assign(nCh, 0.0);
        junctionCapState.assign(nCh, 0.0);
        reverseRecoveryState.assign(nCh, 0.0);
        updateHpAlpha();
    }
    void prepare(const patina::ProcessSpec& spec) noexcept { prepare(spec.numChannels, spec.sampleRate); }
    void reset() noexcept
    {
        std::fill(prevIn.begin(), prevIn.end(), 0.0f);
        std::fill(hpPrevX.begin(), hpPrevX.end(), 0.0);
        std::fill(hpPrevY.begin(), hpPrevY.end(), 0.0);
        std::fill(junctionCapState.begin(), junctionCapState.end(), 0.0);
        std::fill(reverseRecoveryState.begin(), reverseRecoveryState.end(), 0.0);
    }

    void setEffectiveSampleRate(double sr) noexcept
    {
        if (sr > 200.0 && std::fabs(sr - sampleRate) > 1e-6)
        {
            sampleRate = sr;
            updateHpAlpha();
        }
    }

    inline float process(int channel, float x, float drive, int modeIndex = 0) noexcept
    {
        return processWithParams(channel, x, drive, modeIndex, DiodeType::Si1N4148, 25.0f);
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        return processWithParams(channel, x, params.drive, params.mode,
                                 static_cast<DiodeType>(params.diodeType), params.temperature);
    }

    void processBlock(float* const* io, int numChannels, int numSamples, const Params& params) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], params);
    }

private:
    // === Component layer (Parts) ===
    DiodePrimitive diodeSi;       // Si 1N4148
    DiodePrimitive diodeSchottky; // Schottky 1N5818
    DiodePrimitive diodeGe;       // Ge OA91

    DiodePrimitive& getActiveDiode(DiodeType t) noexcept
    {
        switch (t)
        {
            case DiodeType::Schottky1N5818: return diodeSchottky;
            case DiodeType::GeOA91:         return diodeGe;
            default:                        return diodeSi;
        }
    }

    inline float processWithParams(int channel, float x, float drive, int modeIndex,
                                   DiodeType diodeType, float temperature) noexcept
    {
        // --- RC high-pass filter ---
        double xin = static_cast<double>(x);
        if (channel >= 0 && (size_t)channel < hpPrevX.size())
        {
            double a = alphaHp;
            double yhp = a * (hpPrevY[(size_t)channel] + xin - hpPrevX[(size_t)channel]);
            hpPrevX[(size_t)channel] = xin;
            hpPrevY[(size_t)channel] = yhp;
            xin = yhp;
        }

        double gain = 1.0 + static_cast<double>(drive) * 3.0;
        double y = xin * gain;

        Mode mode = Mode::Bypass;
        if (modeIndex == 1) mode = Mode::Diode;
        else if (modeIndex == 2) mode = Mode::Tanh;

        double v = y;
        const double headroom = 1.3 + 0.7 * static_cast<double>(drive);
        const double postTrim = 1.0 / (1.0 + 0.8 * static_cast<double>(drive));

        if (mode == Mode::Diode)
        {
            auto& diode = getActiveDiode(diodeType);
            const auto& spec = diode.getSpec();

            // Temperature-dependent forward voltage: delegated to DiodePrimitive
            const double Vf = diode.effectiveVf(temperature);
            const double vthPos = Vf * headroom;
            const double vthNeg = Vf * headroom * spec.asymmetry;
            const double slope = spec.slopeScale;
            const double Rs = spec.seriesR;

            // Junction capacitance filter (using DiodePrimitive physical properties)
            const size_t ch = (size_t)std::clamp(channel, 0, (int)junctionCapState.size() - 1);
            {
                double Cj = diode.junctionCapacitance(std::abs(v));
                // RC LPF: junction capacitance x series resistance
                double RCj = Rs * Cj;
                double dt = 1.0 / sampleRate;
                double jcAlpha = dt / (RCj + dt);
                jcAlpha = std::clamp(jcAlpha, 0.001, 1.0);
                junctionCapState[ch] += jcAlpha * (v - junctionCapState[ch]);
                v = junctionCapState[ch];
            }

            // Asymmetric diode clipping
            if (v > 0.0)
            {
                if (v > vthPos)
                {
                    double excess = v - vthPos;
                    // Series resistance: voltage drop from current × Rs even after clipping
                    double clipV = vthPos + (1.0 - std::exp(-slope * excess)) / slope;
                    v = clipV + excess * Rs * 0.001; // Rs effect (normalized)
                }
            }
            else
            {
                double ax = -v;
                if (ax > vthNeg)
                {
                    double excess = ax - vthNeg;
                    double clipV = vthNeg + (1.0 - std::exp(-slope * excess)) / slope;
                    v = -(clipV + excess * Rs * 0.001);
                }
            }

            // Reverse recovery transient response (significant only in Si diodes)
            {
                double& rrState = reverseRecoveryState[ch];
                double prevVal = prevIn[(size_t)channel];
                // Reverse current flows momentarily when signal transitions from positive to negative
                if (prevVal > 0.0 && v < 0.0)
                {
                    rrState = spec.recoveryTime * sampleRate * 0.5; // Recovery sample count
                }
                if (rrState > 0.1)
                {
                    // During recovery: slight reverse conduction (glitch)
                    v += prevVal * 0.015 * std::min(1.0, rrState);
                    rrState *= 0.7; // Exponential decay
                }
            }
        }
        else if (mode == Mode::Tanh)
        {
            const double knee = 1.0 * headroom;
            v = std::tanh(v / knee) * knee;
        }

        float fout = static_cast<float>(v * postTrim);
        if (channel >= 0 && (size_t)channel < prevIn.size()) prevIn[(size_t)channel] = fout;
        return fout;
    }

    double sampleRate;
    double alphaHp;
    std::vector<float> prevIn;
    std::vector<double> hpPrevX;
    std::vector<double> hpPrevY;
    std::vector<double> junctionCapState;       // Junction capacitance LPF state
    std::vector<double> reverseRecoveryState;   // Reverse recovery transient state

    inline void updateHpAlpha() noexcept
    {
        const double R = PartsConstants::R_driveHp;
        const double C = PartsConstants::C_driveHp;
        const double RC = R * C;
        const double dt = 1.0 / sampleRate;
        double a = RC / (RC + dt);
        if (!std::isfinite(a)) a = 1.0;
        if (a < 1e-9) a = 1e-9; if (a > 0.999999999) a = 0.999999999;
        alphaHp = a;
    }
};
