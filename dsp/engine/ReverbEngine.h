#pragma once

#include <algorithm>
#include <cmath>
#include "../core/ProcessSpec.h"
#include "../core/DenormalGuard.h"
#include "../core/FastMath.h"
#include "../circuits/drive/InputBuffer.h"
#include "../circuits/drive/OutputStage.h"
#include "../circuits/delay/SpringReverbModel.h"
#include "../circuits/delay/PlateReverb.h"
#include "../circuits/filters/ToneFilter.h"
#include "../circuits/mixer/DryWetMixer.h"
#include "../constants/PartsConstants.h"

namespace patina {

// Reverb engine — spring / plate reverb integration layer
// Signal path:
//   Outboard: Input → [SpringReverb | PlateReverb] → ToneFilter → Dry/Wet
//   Pedal:    Input → InputBuffer → [SpringReverb | PlateReverb] → ToneFilter → OutputStage → Dry/Wet
//   pedalMode=false (default) for outboard quality, true for pedal quality
class ReverbEngine
{
public:
    enum Type { Spring = 0, Plate = 1 };

    struct Params
    {
        int   type           = Spring;  // Reverb type
        float decay          = 0.5f;    // Decay 0.0–1.0
        float tone           = 0.5f;    // tone 0.0–1.0
        float mix            = 0.3f;    // Dry/Wet 0.0–1.0
        double supplyVoltage = 9.0;     // supply voltage (V)

        // Spring-specific
        float tension        = 0.5f;    // Spring tension 0.0–1.0
        float dripAmount     = 0.3f;    // Drip effect amount 0.0–1.0
        int   numSprings     = 3;       // Number of springs (1-3)

        // Plate-specific
        float predelayMs     = 10.0f;   // Predelay (ms)
        float damping        = 0.5f;    // Damping 0.0–1.0
        float diffusion      = 0.7f;    // Diffusion 0.0–1.0
        float modDepth       = 0.0f;    // Internal modulation depth 0.0–1.0

        bool  pedalMode      = false;   // true=pedal quality(InputBuffer+OutputStage), false=outboard quality
    };

    ReverbEngine() = default;

    void prepare(const ProcessSpec& spec)
    {
        currentSpec = spec;

        inputBuffer.prepare(spec);
        springReverb.prepare(spec);
        plateReverb.prepare(spec);
        toneFilter.prepare(spec);
        outputStage.prepare(spec);
    }

    void reset()
    {
        inputBuffer.reset();
        springReverb.reset();
        plateReverb.reset();
        toneFilter.reset();
        outputStage.reset();
    }

    void processBlock(const float* const* input, float* const* output,
                      int numChannels, int numSamples, const Params& params)
    {
        const int nCh = std::min(numChannels, currentSpec.numChannels);
        if (nCh <= 0 || numSamples <= 0) return;
        ScopedDenormalDisable denormalGuard;

        toneFilter.updateToneFilterIfNeeded(params.tone, 1.0, true);

        // Spring reverb parameters
        SpringReverbModel::Params springP;
        springP.decay      = params.decay;
        springP.tone       = params.tone;
        springP.mix        = 1.0f;  // Internal is 100% wet; mixing done at the end
        springP.tension    = params.tension;
        springP.dripAmount = params.dripAmount;
        springP.numSprings = params.numSprings;

        // Plate reverb parameters
        PlateReverb::Params plateP;
        plateP.decay      = params.decay;
        plateP.predelayMs = params.predelayMs;
        plateP.damping    = params.damping;
        plateP.mix        = 1.0f;  // Internal is 100% wet
        plateP.diffusion  = params.diffusion;
        plateP.modDepth   = params.modDepth;

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < nCh; ++ch)
            {
                float dry = FastMath::sanitize(input[ch][i]);

                // Input buffer — pedal mode only
                float wet = params.pedalMode ? inputBuffer.process(ch, dry) : dry;

                // Reverb processing
                if (params.type == Plate)
                    wet = plateReverb.process(ch, wet, plateP);
                else
                    wet = springReverb.process(ch, wet, springP);

                // tonefilter
                wet = toneFilter.processSample(ch, wet);

                // Output stage — pedal mode only
                if (params.pedalMode)
                    wet = outputStage.process(ch, wet, params.supplyVoltage);

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
    const SpringReverbModel& getSpringReverb() const { return springReverb; }
    const PlateReverb&       getPlateReverb()  const { return plateReverb; }
    const ToneFilter&        getToneFilter()   const { return toneFilter; }

private:
    ProcessSpec currentSpec;

    InputBuffer      inputBuffer;
    SpringReverbModel springReverb;
    PlateReverb      plateReverb;
    ToneFilter       toneFilter;
    OutputStage      outputStage;
};

} // namespace patina
