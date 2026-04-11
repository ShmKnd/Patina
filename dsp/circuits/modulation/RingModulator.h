#pragma once
#include <cmath>
#include <algorithm>
#include <vector>
#include "../../constants/PartsConstants.h"
#include "../../core/ProcessSpec.h"
#include "../../parts/DiodePrimitive.h"

// ring modulator — diode bridge type ring modulator
// constructs a bridge circuit with 4 DiodePrimitives, operating as a 2-input multiplier
// reproduces real-hardware diode mismatch, temperature dependence, and nonlinear distortion
//
// circuit configuration (diode bridge):
//        D1 (+)      D2 (+)
//   In ──┤├──┬──┤├── Carrier
//              │
//            Output
//              │
//   In ──┤├──┴──┤├── Carrier
//        D3 (-)      D4 (-)
//
// ideal operation: Out = In × Carrier (multiplier)
// analog distortion: carrier leakage below diode Vf threshold, nonlinear crosstalk
//
// 4-layer architecture:
//   Parts: DiodePrimitive × 4 (Si/Schottky/Ge)
//   → Circuit: RingModulator (bridge multiplication + DC removal)
class RingModulator
{
public:
    struct Params
    {
        float mix          = 1.0f;    // Dry/Wet 0.0–1.0
        float temperature  = 25.0f;   // operating temperature (°C)
        int   diodeType    = 0;       // 0=Si, 1=Schottky, 2=Ge
        float mismatch     = 0.02f;   // diode matching variation 0.0–0.1
    };

    RingModulator() noexcept
    {
        // Initialize 4 diodes with manufacturing tolerance
        initDiodes(0, 0.02);
    }

    void prepare(int numChannels, double sampleRate) noexcept
    {
        sr = (sampleRate > 1.0 ? sampleRate : 44100.0);
        const size_t nCh = (size_t)std::max(1, numChannels);
        dcStateIn.assign(nCh, 0.0);
        dcStateOut.assign(nCh, 0.0);
        dcPrevIn.assign(nCh, 0.0);
        dcPrevOut.assign(nCh, 0.0);

        // HPF for DC removal (fc ≈ 20Hz)
        double fc = 20.0;
        double rc = 1.0 / (2.0 * M_PI * fc);
        double dt = 1.0 / sr;
        dcAlpha = rc / (rc + dt);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        std::fill(dcStateIn.begin(), dcStateIn.end(), 0.0);
        std::fill(dcStateOut.begin(), dcStateOut.end(), 0.0);
        std::fill(dcPrevIn.begin(), dcPrevIn.end(), 0.0);
        std::fill(dcPrevOut.begin(), dcPrevOut.end(), 0.0);
    }

    // input: signal to be modulated, carrier: carrier signal
    inline float process(int channel, float input, float carrier,
                         const Params& params) noexcept
    {
        const double temp = (double)params.temperature;
        const double mix  = std::clamp((double)params.mix, 0.0, 1.0);

        // Re-initialize when diode type or mismatch changes
        updateDiodesIfNeeded(params.diodeType, params.mismatch);

        double in  = (double)input;
        double car = (double)carrier;

        // diode bridge ring modulator
        // D1, D2: positive half-cycle path (In + Carrier → Output)
        // D3, D4: negative half-cycle path (In - Carrier → Output)
        double sumPos  = in + car;   // D1-D2 path input
        double sumNeg  = in - car;   // D3-D4 path input

        // each diode's nonlinear response
        double d1out = diodes[0].saturate(sumPos, temp);
        double d2out = diodes[1].saturate(sumPos, temp);
        double d3out = diodes[2].saturate(sumNeg, temp);
        double d4out = diodes[3].saturate(sumNeg, temp);

        // bridge output: difference between positive and negative paths
        double bridgeOut = (d1out + d2out) - (d3out + d4out);

        // normalization — level correction depending on diode Vf
        double vf = diodes[0].effectiveVf(temp);
        double normGain = 1.0 / std::max(0.1, vf * 4.0);
        bridgeOut *= normGain;

        // DC removal HPF
        if (channel >= 0 && (size_t)channel < dcStateIn.size())
        {
            const size_t ch = (size_t)channel;
            double filtered = dcAlpha * (dcPrevOut[ch] + bridgeOut - dcPrevIn[ch]);
            dcPrevIn[ch]  = bridgeOut;
            dcPrevOut[ch] = filtered;
            bridgeOut = filtered;
        }

        // Dry/Wet Mix
        double out = in * (1.0 - mix) + bridgeOut * mix;

        return (float)out;
    }

    void processBlock(float* const* io, const float* const* carrier,
                      int numChannels, int numSamples,
                      const Params& params) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], carrier[ch][i], params);
    }

    const DiodePrimitive& getDiode(int index) const noexcept
    {
        return diodes[std::clamp(index, 0, 3)];
    }

private:
    DiodePrimitive diodes[4];

    double sr = 44100.0;
    double dcAlpha = 0.999;
    std::vector<double> dcStateIn;
    std::vector<double> dcStateOut;
    std::vector<double> dcPrevIn;
    std::vector<double> dcPrevOut;

    int    currentDiodeType = 0;
    double currentMismatch  = 0.02;

    void initDiodes(int diodeType, double mismatch) noexcept
    {
        DiodePrimitive::Spec base;
        switch (diodeType) {
            case 1:  base = DiodePrimitive::Schottky1N5818(); break;
            case 2:  base = DiodePrimitive::GeOA91(); break;
            default: base = DiodePrimitive::Si1N4148(); break;
        }

        // apply manufacturing variation to 4 diodes
        // 1–5% mismatch is typical in real ring modulators
        double offsets[4] = { 1.0, 1.0, 1.0, 1.0 };
        if (mismatch > 0.0)
        {
            // deterministic but asymmetric offsets (no random numbers for reproducibility)
            offsets[0] = 1.0 + mismatch * 0.7;
            offsets[1] = 1.0 - mismatch * 0.3;
            offsets[2] = 1.0 + mismatch * 0.2;
            offsets[3] = 1.0 - mismatch * 0.6;
        }

        for (int i = 0; i < 4; ++i)
        {
            DiodePrimitive::Spec s = base;
            s.Vf_25C    *= offsets[i];
            s.asymmetry *= (2.0 - offsets[i]); // inverse correlation of mismatch
            diodes[i] = DiodePrimitive(s);
        }

        currentDiodeType = diodeType;
        currentMismatch  = mismatch;
    }

    void updateDiodesIfNeeded(int diodeType, double mismatch) noexcept
    {
        if (diodeType != currentDiodeType ||
            std::fabs(mismatch - currentMismatch) > 1e-6)
        {
            initDiodes(diodeType, mismatch);
        }
    }
};
