#pragma once

#include <algorithm>
#include <cmath>
#include "../core/ProcessSpec.h"
#include "../core/DenormalGuard.h"
#include "../core/FastMath.h"
#include "../circuits/drive/InputBuffer.h"
#include "../circuits/drive/OutputStage.h"
#include "../circuits/saturation/TapeSaturation.h"
#include "../circuits/saturation/TransformerModel.h"
#include "../circuits/filters/ToneFilter.h"
#include "../circuits/mixer/DryWetMixer.h"
#include "../circuits/power/BatterySag.h"
#include "../constants/PartsConstants.h"

namespace patina {

// Tape machine engine — studio tape simulation integration layer
// Signal path:
//   Outboard: Input → TapeSaturation → TransformerModel(Output transformer) → ToneFilter → Dry/Wet
//   Pedal:    Input → InputBuffer → TapeSaturation → TransformerModel(Output transformer)
//             → ToneFilter → OutputStage → Dry/Wet
//   pedalMode=false (default) for outboard quality, true for pedal quality
//   (Reproduces power supply variation via BatterySag)
class TapeMachineEngine
{
public:
    struct Params
    {
        // Tape
        float inputGain      = 0.0f;    // Input gain (dB equivalent, 0.0=unity)
        float saturation     = 0.5f;    // Saturation amount 0.0–1.0
        float biasAmount     = 0.5f;    // Bias amount 0.0–1.0
        float tapeSpeed      = 1.0f;    // Tape speed (0.5=7.5ips, 1.0=15ips, 2.0=30ips)
        float wowFlutter     = 0.0f;    // Wow & flutter 0.0–1.0
        bool  enableHeadBump = true;    // Enable head bump
        bool  enableHfRolloff = true;   // Enable HF rolloff
        float headWear       = 0.0f;    // Head wear 0.0–1.0
        float tapeAge        = 0.0f;    // Tape degradation 0.0–1.0

        // Output transformer
        bool  enableTransformer = true;  // Enable output transformer model
        float transformerDrive  = 0.0f;  // Transformer drive (dB)
        float transformerSat    = 0.3f;  // Transformer saturation 0.0–1.0

        // Tone & output
        float tone           = 0.5f;    // tone 0.0–1.0
        float mix            = 1.0f;    // Dry/Wet 0.0–1.0
        double supplyVoltage = 9.0;     // supply voltage (V)

        bool  pedalMode      = false;   // true=pedal quality(InputBuffer+OutputStage), false=outboard quality
    };

    TapeMachineEngine() = default;

    void prepare(const ProcessSpec& spec)
    {
        currentSpec = spec;

        inputBuffer.prepare(spec);
        tapeSat.prepare(spec);
        transformer.prepare(spec);
        toneFilter.prepare(spec);
        outputStage.prepare(spec);
    }

    void reset()
    {
        inputBuffer.reset();
        tapeSat.reset();
        transformer.reset();
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

        // Tape parameters
        TapeSaturation::Params tapeP;
        tapeP.inputGain     = params.inputGain;
        tapeP.saturation    = params.saturation;
        tapeP.biasAmount    = params.biasAmount;
        tapeP.tapeSpeed     = params.tapeSpeed;
        tapeP.wowFlutter    = params.wowFlutter;
        tapeP.enableHeadBump  = params.enableHeadBump;
        tapeP.enableHfRolloff = params.enableHfRolloff;
        tapeP.headWear      = params.headWear;
        tapeP.tapeAge       = params.tapeAge;

        // Transformer parameters
        TransformerModel::Params xfmrP;
        xfmrP.driveDb     = params.transformerDrive;
        xfmrP.saturation  = params.transformerSat;
        xfmrP.enableLfBoost  = true;
        xfmrP.enableHfRolloff = true;

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < nCh; ++ch)
            {
                float dry = FastMath::sanitize(input[ch][i]);

                // Input buffer — pedal mode only
                float wet = params.pedalMode ? inputBuffer.process(ch, dry) : dry;

                // Tape saturation
                wet = tapeSat.process(ch, wet, tapeP);

                // Output transformer
                if (params.enableTransformer)
                    wet = transformer.process(ch, wet, xfmrP);

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
    const TapeSaturation&    getTapeSat()     const { return tapeSat; }
    const TransformerModel&  getTransformer() const { return transformer; }
    const ToneFilter&        getToneFilter()  const { return toneFilter; }

private:
    ProcessSpec currentSpec;

    InputBuffer      inputBuffer;
    TapeSaturation   tapeSat;
    TransformerModel transformer;
    ToneFilter       toneFilter;
    OutputStage      outputStage;
};

} // namespace patina
