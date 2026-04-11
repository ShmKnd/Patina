#pragma once
#include <cmath>
#include <algorithm>
#include <vector>
#include "../../constants/PartsConstants.h"
#include "../../core/ProcessSpec.h"
#include "../../parts/DiodePrimitive.h"
#include "../../parts/BJT_Primitive.h"

// Wave folder — Classic West Coast synthesizer-style wave-folding distortion circuit
// Composed of DiodePrimitive nonlinear clipping and BJT_Primitive gain stages
// Analog-style waveform synthesis where harmonic structure varies with fold stages
//
// 4-layer architecture:
//   Parts: DiodePrimitive (Si/Schottky/Ge) + BJT_Primitive (gain + saturation)
//   → Circuit: WaveFolder (Multi-stage folding + DC offset correction)
//
// Signal path:
//   Input -> BJT gain -> [DiodeFold x N stages] -> DC removal -> Output
class WaveFolder
{
public:
    struct Params
    {
        float foldAmount   = 0.5f;    // Fold amount 0.0–1.0
        float symmetry     = 1.0f;    // Symmetry 0.0–1.0 (1.0=fully symmetric)
        int   numStages    = 4;       // Number of fold stages 1–8
        float temperature  = 25.0f;   // operating temperature (°C)
        int   diodeType    = 0;       // 0=Si, 1=Schottky, 2=Ge
    };

    WaveFolder() noexcept
        : diodeSi(DiodePrimitive::Si1N4148()),
          diodeSchottky(DiodePrimitive::Schottky1N5818()),
          diodeGe(DiodePrimitive::GeOA91()),
          bjt(BJT_Primitive::Generic())
    {}

    void prepare(int numChannels, double sampleRate) noexcept
    {
        sr = (sampleRate > 1.0 ? sampleRate : 44100.0);
        const size_t nCh = (size_t)std::max(1, numChannels);
        dcState.assign(nCh, 0.0);
        prevOut.assign(nCh, 0.0);

        // DC removal HPF coefficient (fc ≈ 10Hz)
        double fc = 10.0;
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
        std::fill(dcState.begin(), dcState.end(), 0.0);
        std::fill(prevOut.begin(), prevOut.end(), 0.0);
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        const int stages = std::clamp(params.numStages, 1, 8);
        const double fold = std::clamp((double)params.foldAmount, 0.0, 1.0);
        const double sym  = std::clamp((double)params.symmetry, 0.0, 1.0);
        const double temp = (double)params.temperature;

        DiodePrimitive& diode = getActiveDiode(params.diodeType);

        // BJT gain stage — increases drive according to fold amount
        // foldAmount=0 gives 1x, foldAmount=1 gives approximately 10x max
        double gain = 1.0 + fold * 9.0;
        double sig = bjt.saturate((double)x * gain);

        // Multi-stage folding: diode clip → invert → re-gain
        for (int s = 0; s < stages; ++s)
        {
            double clipped = diode.clip(sig, temp);
            double excess = sig - clipped;

            // Asymmetry: adjusts positive-side fold strength
            double asymFactor = 1.0 + (1.0 - sym) * 0.3;
            if (sig > 0.0)
                excess *= asymFactor;

            // Folding: inverts excess and adds
            sig = clipped - excess * fold;

            // Light BJT saturation between stages (adds harmonics)
            if (s < stages - 1)
                sig = bjt.saturate(sig * (1.0 + fold * 0.5));
        }

        // DC removal (1-pole HPF)
        if (channel >= 0 && (size_t)channel < dcState.size())
        {
            double prev = dcState[(size_t)channel];
            double prevO = prevOut[(size_t)channel];
            double filtered = dcAlpha * (prevO + sig - prev);
            dcState[(size_t)channel] = sig;
            prevOut[(size_t)channel] = filtered;
            sig = filtered;
        }

        return (float)sig;
    }

    void processBlock(float* const* io, int numChannels, int numSamples,
                      const Params& params) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i], params);
    }

    const DiodePrimitive& getDiode(int type) const noexcept
    {
        switch (type) {
            case 1:  return diodeSchottky;
            case 2:  return diodeGe;
            default: return diodeSi;
        }
    }

    const BJT_Primitive& getBjt() const noexcept { return bjt; }

private:
    DiodePrimitive diodeSi;
    DiodePrimitive diodeSchottky;
    DiodePrimitive diodeGe;
    BJT_Primitive  bjt;

    double sr = 44100.0;
    double dcAlpha = 0.999;
    std::vector<double> dcState;
    std::vector<double> prevOut;

    DiodePrimitive& getActiveDiode(int type) noexcept
    {
        switch (type) {
            case 1:  return diodeSchottky;
            case 2:  return diodeGe;
            default: return diodeSi;
        }
    }
};
