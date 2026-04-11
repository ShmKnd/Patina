#pragma once

#include <algorithm>
#include <cmath>
#include "../core/ProcessSpec.h"
#include "../core/DenormalGuard.h"
#include "../core/FastMath.h"
#include "../circuits/drive/InputBuffer.h"
#include "../circuits/drive/DiodeClipper.h"
#include "../circuits/drive/OutputStage.h"
#include "../circuits/filters/ToneFilter.h"
#include "../circuits/mixer/DryWetMixer.h"
#include "../circuits/power/PowerSupplySag.h"
#include "../constants/PartsConstants.h"

namespace patina {

// Drive / overdrive engine — integration layer
// Signal path:
//   Outboard: Input → DiodeClipper → ToneFilter → Dry/Wet
//   Pedal:    Input → InputBuffer(TL072) → DiodeClipper → ToneFilter → OutputStage → Dry/Wet
//   (Reproduces dynamic headroom variation from power supply sag)
//   pedalMode=false (default) for outboard quality, true for pedal quality
class DriveEngine
{
public:
    struct Params
    {
        float drive         = 0.5f;     // Drive amount 0.0–1.0
        int   clippingMode  = 1;        // 0=Bypass, 1=Diode, 2=Tanh
        int   diodeType     = 0;        // 0=Si, 1=Schottky, 2=Ge
        float tone          = 0.5f;     // tone 0.0–1.0
        float outputLevel   = 0.7f;     // Output level 0.0–1.0
        float mix           = 1.0f;     // Dry/Wet 0.0–1.0
        double supplyVoltage = 9.0;     // supply voltage (V)
        float temperature   = 25.0f;    // operating temperature (°C)

        bool  enablePowerSag = false;   // Enable power supply sag
        float sagAmount      = 0.5f;    // Sag amount 0.0–1.0

        bool  pedalMode      = false;   // true=pedal quality(InputBuffer+OutputStage), false=outboard quality
    };

    DriveEngine() = default;

    void prepare(const ProcessSpec& spec)
    {
        currentSpec = spec;

        inputBuffer.prepare(spec);
        clipper.prepare(spec);
        toneFilter.prepare(spec);
        outputStage.prepare(spec);
        powerSag.prepare(spec);
    }

    void reset()
    {
        inputBuffer.reset();
        clipper.reset();
        toneFilter.reset();
        outputStage.reset();
        powerSag.reset();
    }

    void processBlock(const float* const* input, float* const* output,
                      int numChannels, int numSamples, const Params& params)
    {
        const int nCh = std::min(numChannels, currentSpec.numChannels);
        if (nCh <= 0 || numSamples <= 0) return;
        ScopedDenormalDisable denormalGuard;

        // Update tone filter
        toneFilter.updateToneFilterIfNeeded(params.tone, 1.0, true);

        // DiodeClipper Parameters
        DiodeClipper::Params clipParams;
        clipParams.drive       = params.drive;
        clipParams.mode        = params.clippingMode;
        clipParams.diodeType   = params.diodeType;
        clipParams.temperature = params.temperature;

        for (int i = 0; i < numSamples; ++i)
        {
            // Power supply sag calculation
            double effectiveVoltage = params.supplyVoltage;
            if (params.enablePowerSag)
            {
                float avgInput = 0.0f;
                for (int ch = 0; ch < nCh; ++ch)
                    avgInput += std::abs(input[ch][i]);
                avgInput /= (float)nCh;

                PowerSupplySag::Params sagParams;
                sagParams.sagDepth = params.sagAmount;
                float sagCoeff = powerSag.process(0, avgInput, sagParams);
                effectiveVoltage = params.supplyVoltage * (double)sagCoeff;
            }

            for (int ch = 0; ch < nCh; ++ch)
            {
                float dry = FastMath::sanitize(input[ch][i]);

                // Input buffer (TL072) — pedal mode only
                float wet = params.pedalMode ? inputBuffer.process(ch, dry) : dry;

                // Clipping stage
                wet = clipper.process(ch, wet, clipParams);

                // tonefilter
                wet = toneFilter.processSample(ch, wet);

                // Output stage (level + LPF) — pedal mode only
                wet *= params.outputLevel;
                if (params.pedalMode)
                    wet = outputStage.process(ch, wet, effectiveVoltage);

                // Dry/Wet Mix
                float gDry, gWet;
                DryWetMixer::equalPowerGainsFast(params.mix, gDry, gWet);
                output[ch][i] = dry * gDry + wet * gWet;
            }
        }

        // zero-clear surplus channels
        for (int ch = nCh; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                output[ch][i] = 0.0f;
    }

    // internal access (for diagnostics/testing)
    const InputBuffer&    getInputBuffer()  const { return inputBuffer; }
    const DiodeClipper&   getClipper()      const { return clipper; }
    const ToneFilter&     getToneFilter()   const { return toneFilter; }
    const OutputStage&    getOutputStage()  const { return outputStage; }

private:
    ProcessSpec currentSpec;

    InputBuffer   inputBuffer;
    DiodeClipper  clipper;
    ToneFilter    toneFilter;
    OutputStage   outputStage;
    PowerSupplySag powerSag;
};

} // namespace patina
