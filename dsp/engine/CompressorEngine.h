#pragma once

#include <algorithm>
#include <cmath>
#include "../core/ProcessSpec.h"
#include "../core/DenormalGuard.h"
#include "../core/FastMath.h"
#include "../circuits/dynamics/PhotoCompressor.h"
#include "../circuits/dynamics/FetCompressor.h"
#include "../circuits/dynamics/VariableMuCompressor.h"
#include "../circuits/dynamics/VcaCompressor.h"
#include "../circuits/compander/NoiseGate.h"
#include "../circuits/drive/InputBuffer.h"
#include "../circuits/drive/OutputStage.h"
#include "../circuits/mixer/DryWetMixer.h"
#include "../constants/PartsConstants.h"

namespace patina {

// Compressor engine — Photo / FET / Variable-Mu / VCA switchable dynamics integration layer
// Signal path:
//   Outboard: Input → [NoiseGate(opt)] → [Photo | FET | VariableMu | VCA] → Dry/Wet
//   Pedal:    Input → InputBuffer → [NoiseGate(opt)] → [Photo | FET | VariableMu | VCA] → OutputStage → Dry/Wet
//   pedalMode=false (default) for outboard quality, true for pedal quality
class CompressorEngine
{
public:
    enum Type { Photo = 0, Fet = 1, VariableMu = 2, Vca = 3 };

    struct Params
    {
        int   type            = Fet;        // Compressor type
        float inputGain       = 0.5f;       // Input gain 0.0–1.0
        float threshold       = 0.5f;       // Threshold / peak reduction 0.0–1.0
        float outputGain      = 0.5f;       // Makeup gain 0.0–1.0
        float attack          = 0.5f;       // Attack 0.0–1.0 (FET/VCA)
        float release         = 0.5f;       // Release 0.0–1.0 (FET/VCA)
        int   ratio           = 0;          // Ratio selector (FET: 0–4, VariableMu: TC 0–5, VCA: 0–4)
        float mix             = 1.0f;       // Dry/Wet 0.0–1.0

        // Noise gate (optional)
        bool  enableGate      = false;
        float gateThresholdDb = -40.0f;     // Gate threshold (dBFS)

        // Photo-specific
        int   photoMode       = 0;          // 0=Compress, 1=Limit

        // VCA-specific
        int   vcaKneeMode     = 0;          // 0=soft knee, 1=hard knee

        // pedal mode
        bool  pedalMode       = false;      // true=pedal quality(InputBuffer+OutputStage), false=outboard quality
        double supplyVoltage  = 9.0;        // Supply voltage (V) — for OutputStage in pedal mode
    };

    CompressorEngine() = default;

    void prepare(const ProcessSpec& spec)
    {
        currentSpec = spec;

        photoComp.prepare(spec);
        fetComp.prepare(spec);
        varMuComp.prepare(spec);
        vcaComp.prepare(spec);
        noiseGate.prepare(spec);
        inputBuffer.prepare(spec);
        outputStage.prepare(spec);
    }

    void reset()
    {
        photoComp.reset();
        fetComp.reset();
        varMuComp.reset();
        vcaComp.reset();
        noiseGate.reset();
        inputBuffer.reset();
        outputStage.reset();
    }

    void processBlock(const float* const* input, float* const* output,
                      int numChannels, int numSamples, const Params& params)
    {
        const int nCh = std::min(numChannels, currentSpec.numChannels);
        if (nCh <= 0 || numSamples <= 0) return;
        ScopedDenormalDisable denormalGuard;

        // Noise gate parameters
        NoiseGate::Params gateP;
        gateP.thresholdDb = params.gateThresholdDb;

        // Compressor parameters
        PhotoCompressor::Params photoP;
        photoP.peakReduction = 1.0f - params.threshold;
        photoP.outputGain    = params.outputGain;
        photoP.mode          = params.photoMode;
        photoP.mix           = 1.0f;

        FetCompressor::Params fetP;
        fetP.inputGain  = params.inputGain;
        fetP.outputGain = params.outputGain;
        fetP.attack     = params.attack;
        fetP.release    = params.release;
        fetP.ratio      = params.ratio;
        fetP.mix        = 1.0f;

        VariableMuCompressor::Params muP;
        muP.inputGain    = params.inputGain;
        muP.threshold    = params.threshold;
        muP.outputGain   = params.outputGain;
        muP.timeConstant = params.ratio;
        muP.mix          = 1.0f;

        VcaCompressor::Params vcaP;
        vcaP.threshold  = params.threshold;
        vcaP.ratio      = params.inputGain;   // VCA: Uses inputGain knob as ratio
        vcaP.attack     = params.attack;
        vcaP.release    = params.release;
        vcaP.outputGain = params.outputGain;
        vcaP.mix        = 1.0f;
        vcaP.kneeMode   = params.vcaKneeMode;

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < nCh; ++ch)
            {
                float dry = FastMath::sanitize(input[ch][i]);
                float wet = params.pedalMode ? inputBuffer.process(ch, dry) : dry;

                // Noise gate (pre-stage)
                if (params.enableGate)
                    wet = noiseGate.process(ch, wet, gateP);

                // Compressor
                switch (params.type)
                {
                    case Photo:
                        wet = photoComp.process(ch, wet, photoP);
                        break;
                    case VariableMu:
                        wet = varMuComp.process(ch, wet, muP);
                        break;
                    case Vca:
                        wet = vcaComp.process(ch, wet, vcaP);
                        break;
                    case Fet:
                    default:
                        wet = fetComp.process(ch, wet, fetP);
                        break;
                }

                // Dry/Wet Mix
                if (params.pedalMode)
                    wet = outputStage.process(ch, wet, params.supplyVoltage);
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

    // Read gain reduction (dB)
    float getGainReductionDb(int channel = 0) const
    {
        return fetComp.getGainReductionDb(channel);
    }

    float getPhotoGainReductionDb(int channel = 0) const { return photoComp.getGainReductionDb(channel); }
    float getFetGainReductionDb(int channel = 0) const { return fetComp.getGainReductionDb(channel); }
    float getVarMuGainReductionDb(int channel = 0) const { return varMuComp.getGainReductionDb(channel); }
    float getVcaGainReductionDb(int channel = 0) const { return vcaComp.getGainReductionDb(channel); }

    bool isGateOpen(int channel = 0) const { return noiseGate.isGateOpen(channel); }

    // internal access (for diagnostics/testing)
    const PhotoCompressor&      getPhotoComp()  const { return photoComp; }
    const FetCompressor&        getFetComp()    const { return fetComp; }
    const VariableMuCompressor& getVarMuComp()  const { return varMuComp; }
    const VcaCompressor&        getVcaComp()    const { return vcaComp; }
    const NoiseGate&            getNoiseGate()  const { return noiseGate; }

private:
    ProcessSpec currentSpec;

    PhotoCompressor       photoComp;
    FetCompressor         fetComp;
    VariableMuCompressor  varMuComp;
    VcaCompressor         vcaComp;
    NoiseGate             noiseGate;
    InputBuffer           inputBuffer;
    OutputStage           outputStage;
};

} // namespace patina
