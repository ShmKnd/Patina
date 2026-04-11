#pragma once
#include <cmath>
#include <algorithm>
#include "../constants/CircuitConstants.h"

// RC Circuit primitive — basic filter element using resistance + capacitor
// Models 1st-order LPF / HPF / AllPass as analog RC circuits
// Discretized via bilinear transform or 1st-order exponential filter
//
// Usage example:
//   RC_Element lpf(10000.0, 3.3e-9);  // R=10kΩ, C=3.3nF
//   lpf.prepare(44100.0);
//   float out = lpf.processLPF(input);
class RC_Element
{
public:
    RC_Element() noexcept = default;
    RC_Element(double resistance, double capacitance) noexcept
        : R(resistance), C(capacitance) {}

    void prepare(double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        recalc();
    }

    void reset() noexcept { state = 0.0; prevX = 0.0; prevY = 0.0; }

    // change RC values
    void setRC(double resistance, double capacitance) noexcept
    {
        R = resistance;
        C = capacitance;
        recalc();
    }

    // Cutoff frequency fc = 1 / (2π RC)
    double cutoffHz() const noexcept
    {
        double rc = R * C;
        if (rc <= 0.0) return 20000.0;
        return 1.0 / (2.0 * 3.14159265358979323846 * rc);
    }

    // 1st-order LPF: y[n] = y[n-1] + α (x[n] - y[n-1])
    inline double processLPF(double x) noexcept
    {
        state += alpha * (x - state);
        return state;
    }

    // 1st-order HPF: y[n] = α_hp (y[n-1] + x[n] - x[n-1])
    inline double processHPF(double x) noexcept
    {
        double y = alphaHp * (prevY + x - prevX);
        prevX = x;
        prevY = y;
        return y;
    }

    // 1st-order AllPass: y[n] = a (x[n] - y[n-1]) + x[n-1]
    inline double processAP(double x) noexcept
    {
        double a = apCoeff;
        double y = a * (x - prevY) + prevX;
        prevX = x;
        prevY = y;
        return y;
    }

    double getAlpha() const noexcept { return alpha; }
    double getR() const noexcept { return R; }
    double getC() const noexcept { return C; }

private:
    void recalc() noexcept
    {
        double rc = R * C;
        if (rc <= 0.0 || sampleRate <= 0.0)
        {
            alpha = 1.0;
            alphaHp = 0.0;
            apCoeff = 0.0;
            return;
        }
        double dt = 1.0 / sampleRate;
        // LPF: exponential filter
        alpha = 1.0 - std::exp(-dt / rc);
        alpha = std::clamp(alpha, 0.0, 1.0);
        // HPF: RC / (RC + dt)
        alphaHp = rc / (rc + dt);
        if (!std::isfinite(alphaHp)) alphaHp = 1.0;
        alphaHp = std::clamp(alphaHp, 0.0, 0.999999999);
        // AllPass: (tan(π fc/fs) - 1) / (tan(π fc/fs) + 1)
        double fc = 1.0 / (2.0 * 3.14159265358979323846 * rc);
        fc = std::clamp(fc, 1.0, sampleRate * 0.49);
        double t = std::tan(3.14159265358979323846 * fc / sampleRate);
        apCoeff = (t - 1.0) / (t + 1.0);
    }

    double R = 10000.0;
    double C = 3.3e-9;
    double sampleRate = PartsConstants::defaultSampleRate;
    double alpha = 0.0;
    double alphaHp = 0.0;
    double apCoeff = 0.0;
    double state = 0.0;
    double prevX = 0.0;
    double prevY = 0.0;
};
