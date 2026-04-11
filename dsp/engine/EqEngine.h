#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include "../core/ProcessSpec.h"
#include "../core/DenormalGuard.h"
#include "../core/FastMath.h"
#include "../circuits/filters/StateVariableFilter.h"
#include "../circuits/drive/InputBuffer.h"
#include "../circuits/drive/OutputStage.h"
#include "../constants/PartsConstants.h"

namespace patina {

// 3-band parametric EQ engine — analog console-style equalizer
// Signal path:
//   Outboard: Input → [LowShelf + MidBell + HighShelf (parallel summing)] → OutputGain
//   Pedal:    Input → InputBuffer → [LowShelf + MidBell + HighShelf (parallel summing)] → OutputStage
//   pedalMode=false (default) for outboard quality, true for pedal quality
//
// Each band is based on StateVariableFilter (OTA-SVF)、
// Reproduces gm nonlinearity, temperature dependence, and component tolerance characteristic of analog circuits.
//
// Shelf/bell implementation (parallel summing method):
//   Feed all SVFs with the same input and sum only the gain delta.
//   Prevent crossover interference so each band operates independently.
//   Same algorithm as MasteringStrip。
//   - Low Shelf:  x += (gain - 1) × LP(x)
//   - Mid Bell:   x += (gain - 1) × BP(x)
//   - High Shelf: x += (gain - 1) × HP(x)
class EqEngine
{
public:
    struct Params
    {
        // Low Shelf
        bool  enableLow      = true;
        float lowFreqHz      = 200.0f;    // Shelf frequency 20–2000 Hz
        float lowGainDb      = 0.0f;      // gain -12 ~ +12 dB
        float lowResonance   = 0.3f;      // Shelf slope 0.0–1.0

        // Mid Bell (Parametric)
        bool  enableMid      = true;
        float midFreqHz      = 1000.0f;   // Center frequency 100–10000 Hz
        float midGainDb      = 0.0f;      // gain -12 ~ +12 dB
        float midQ           = 0.5f;      // Q width 0.1–1.0 (0.1=wide, 1.0=narrow)

        // High Shelf
        bool  enableHigh     = true;
        float highFreqHz     = 4000.0f;   // Shelf frequency 1000–20000 Hz
        float highGainDb     = 0.0f;      // gain -12 ~ +12 dB
        float highResonance  = 0.3f;      // Shelf slope 0.0–1.0

        // Overall
        float temperature    = 25.0f;     // operating temperature (°C)
        float outputGainDb   = 0.0f;      // Master gain -12 ~ +12 dB
        double supplyVoltage = 9.0;

        bool  pedalMode      = false;   // true=pedal quality(InputBuffer+OutputStage), false=outboard quality
    };

    EqEngine() = default;

    void prepare(const ProcessSpec& spec)
    {
        currentSpec = spec;
        inputBuffer.prepare(spec);
        svfLow.prepare(spec);
        svfMid.prepare(spec);
        svfHigh.prepare(spec);
        outputStage.prepare(spec);
    }

    void reset()
    {
        inputBuffer.reset();
        svfLow.reset();
        svfMid.reset();
        svfHigh.reset();
        outputStage.reset();
    }

    void processBlock(const float* const* input, float* const* output,
                      int numChannels, int numSamples, const Params& params)
    {
        const int nCh = std::min(numChannels, currentSpec.numChannels);
        if (nCh <= 0 || numSamples <= 0) return;
        ScopedDenormalDisable denormalGuard;

        // Update SVF parameters
        if (params.enableLow)
        {
            svfLow.setCutoffHz(params.lowFreqHz);
            svfLow.setResonance(params.lowResonance);
        }
        if (params.enableMid)
        {
            svfMid.setCutoffHz(params.midFreqHz);
            svfMid.setResonance(params.midQ);
        }
        if (params.enableHigh)
        {
            svfHigh.setCutoffHz(params.highFreqHz);
            svfHigh.setResonance(params.highResonance);
        }

        // Convert gain to linear
        const double lowGain  = dbToLinear(params.lowGainDb);
        const double midGain  = dbToLinear(params.midGainDb);
        const double highGain = dbToLinear(params.highGainDb);
        const double outGain  = dbToLinear(params.outputGainDb);

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < nCh; ++ch)
            {
                float x = FastMath::sanitize(input[ch][i]);

                // Input buffer (analog headroom) — pedal mode only
                if (params.pedalMode)
                    x = inputBuffer.process(ch, x);

                // === Parallel summing EQ ===
                // Feed all SVFs with the same input and sum only the gain delta.
                // This prevents crossover interference.
                float eqSum = 0.0f;

                // Low Shelf: x += (gain - 1) × LP(x)
                if (params.enableLow)
                {
                    float lpOut = svfLow.process(ch, x, /*LP*/ 0);
                    if (std::abs(params.lowGainDb) > 0.05f)
                        eqSum += (float)(lowGain - 1.0) * lpOut;
                }

                // Mid Bell: x += (gain - 1) × BP(x)
                if (params.enableMid)
                {
                    float bpOut = svfMid.process(ch, x, /*BP*/ 2);
                    if (std::abs(params.midGainDb) > 0.05f)
                        eqSum += (float)(midGain - 1.0) * bpOut;
                }

                // High Shelf: x += (gain - 1) × HP(x)
                if (params.enableHigh)
                {
                    float hpOut = svfHigh.process(ch, x, /*HP*/ 1);
                    if (std::abs(params.highGainDb) > 0.05f)
                        eqSum += (float)(highGain - 1.0) * hpOut;
                }

                x += eqSum;

                // Master gain
                x = (float)(x * outGain);

                // Output stage (analog soft clipping) — pedal mode only
                if (params.pedalMode)
                    x = outputStage.process(ch, x, params.supplyVoltage);

                output[ch][i] = x;
            }
        }

        // zero-clear surplus channels
        for (int ch = nCh; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                output[ch][i] = 0.0f;
    }

    // internal access (for diagnostics/testing)
    const StateVariableFilter& getLowSvf()  const { return svfLow; }
    const StateVariableFilter& getMidSvf()  const { return svfMid; }
    const StateVariableFilter& getHighSvf() const { return svfHigh; }

private:
    static double dbToLinear(float db) noexcept
    {
        return std::pow(10.0, (double)db / 20.0);
    }

    ProcessSpec currentSpec;

    InputBuffer          inputBuffer;
    StateVariableFilter  svfLow;
    StateVariableFilter  svfMid;
    StateVariableFilter  svfHigh;
    OutputStage          outputStage;
};

} // namespace patina
