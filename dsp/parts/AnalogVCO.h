#pragma once
#include <cmath>
#include <algorithm>
#include <vector>
#include "../constants/PartsConstants.h"
#include "../core/ProcessSpec.h"
#include "BJT_Primitive.h"
#include "RC_Element.h"

// Analog VCO — audio-rate VCO via BJT differential pair current source + RC integrator
// Designed for UGen construction in SuperCollider/Max/MSP, combining with filters
// to form a complete synthesizer analog oscillator model
//
// Waveforms: Saw, Tri (triangle), Pulse (rectangular + PWM)
// Analog characteristics:
//   - BJT temperature drift (Vbe -2mV/°C → pitch oscillation)
//   - RC integrator leaky cap (waveform distortion)
//   - Nonlinearity of capacitor charge/discharge
//
// 4-layer architecture:
//   Parts: BJT_Primitive (current source, saturation) + RC_Element (integrator)
//   → Parts: AnalogVCO (waveform generation)
class AnalogVCO
{
public:
    enum class Waveform { Saw = 0, Tri = 1, Pulse = 2 };

    struct Spec
    {
        double freqHz     = 440.0;   // base frequency (Hz) 20–20000
        int    waveform   = 0;       // 0=Saw, 1=Tri, 2=Pulse
        double pulseWidth = 0.5;     // PWM duty ratio 0.05–0.95
        double temperature = 25.0;   // operating temperature (°C)
        double drift      = 0.002;   // pitch drift amount 0.0–0.01
    };

    AnalogVCO() noexcept
        : bjt(BJT_Primitive::Generic()),
          integrator(10000.0, 100e-9)  // 10kΩ + 100nF — reference RC
    {}

    void prepare(int numChannels, double sampleRate) noexcept
    {
        sr = (sampleRate > 1.0 ? sampleRate : 44100.0);
        const size_t nCh = (size_t)std::max(1, numChannels);
        phase.assign(nCh, 0.0);
        sawState.assign(nCh, 0.0);
        driftState.assign(nCh, 0.0);

        // for PolyBLEP anti-aliasing
        prevPhase.assign(nCh, 0.0);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        std::fill(phase.begin(), phase.end(), 0.0);
        std::fill(sawState.begin(), sawState.end(), 0.0);
        std::fill(driftState.begin(), driftState.end(), 0.0);
        std::fill(prevPhase.begin(), prevPhase.end(), 0.0);
    }

    inline float process(int channel, const Spec& spec) noexcept
    {
        if (channel < 0 || (size_t)channel >= phase.size()) return 0.0f;
        const size_t ch = (size_t)channel;

        double freq = std::clamp(spec.freqHz, 20.0, 20000.0);
        double pw   = std::clamp(spec.pulseWidth, 0.05, 0.95);
        double temp = spec.temperature;

        // BJT temperature drift — Vbe temperature dependence affects exponential converter
        double tempScale = bjt.tempScale(temp);
        double driftAmt  = std::clamp(spec.drift, 0.0, 0.01);

        // slow drift (1st-order LPF random walk)
        double noise = ((double)((int)(phase[ch] * 1e6) % 997) / 997.0 - 0.5) * 2.0;
        driftState[ch] += 0.0001 * (noise * driftAmt - driftState[ch]);

        double effectiveFreq = freq * tempScale * (1.0 + driftState[ch]);
        double phaseInc = effectiveFreq / sr;

        prevPhase[ch] = phase[ch];
        phase[ch] += phaseInc;
        if (phase[ch] >= 1.0) phase[ch] -= 1.0;

        double out = 0.0;
        switch (static_cast<Waveform>(std::clamp(spec.waveform, 0, 2)))
        {
            case Waveform::Saw:
                out = generateSaw(ch, phaseInc);
                break;
            case Waveform::Tri:
                out = generateTri(ch, phaseInc);
                break;
            case Waveform::Pulse:
                out = generatePulse(ch, phaseInc, pw);
                break;
        }

        // analog-like bandwidth limiting via RC integrator
        // nonlinear capacitor leakage (slight distortion at large amplitudes)
        double rcFiltered = integrator.processLPF(out);
        double leakage = bjt.saturate(rcFiltered);

        // mix 95% original signal + 5% leak distortion
        out = out * 0.95 + leakage * 0.05;

        return (float)std::clamp(out, -1.0, 1.0);
    }

    double getPhase(int channel) const noexcept
    {
        if (channel < 0 || (size_t)channel >= phase.size()) return 0.0;
        return phase[(size_t)channel];
    }

    const BJT_Primitive& getBjt() const noexcept { return bjt; }

private:
    BJT_Primitive bjt;
    RC_Element    integrator;

    double sr = 44100.0;
    std::vector<double> phase;
    std::vector<double> prevPhase;
    std::vector<double> sawState;
    std::vector<double> driftState;

    // PolyBLEP — bandwidth-limited correction of discontinuities
    inline double polyBlep(double t, double dt) const noexcept
    {
        if (t < dt)
        {
            double n = t / dt;
            return n + n - n * n - 1.0;
        }
        else if (t > 1.0 - dt)
        {
            double n = (t - 1.0) / dt;
            return n * n + n + n + 1.0;
        }
        return 0.0;
    }

    inline double generateSaw(size_t ch, double phaseInc) const noexcept
    {
        double p = phase[ch];
        // naive sawtooth: -1 → +1
        double saw = 2.0 * p - 1.0;
        // PolyBLEP correction (mitigates reset discontinuity)
        saw -= polyBlep(p, phaseInc);
        return saw;
    }

    inline double generateTri(size_t ch, double phaseInc) const noexcept
    {
        double p = phase[ch];
        // triangle wave: generated via sawtooth integration (leaky integrator)
        double saw = 2.0 * p - 1.0;
        saw -= polyBlep(p, phaseInc);

        // sawtooth → triangle conversion: |2x| - 1
        double tri = 2.0 * std::fabs(saw) - 1.0;
        return tri;
    }

    inline double generatePulse(size_t ch, double phaseInc, double pw) const noexcept
    {
        double p = phase[ch];
        // naive rectangular
        double pulse = (p < pw) ? 1.0 : -1.0;
        // PolyBLEP on both rising and falling edges
        pulse += polyBlep(p, phaseInc);
        double shifted = p + (1.0 - pw);
        if (shifted >= 1.0) shifted -= 1.0;
        pulse -= polyBlep(shifted, phaseInc);
        return pulse;
    }
};
