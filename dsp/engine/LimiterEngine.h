#pragma once

#include <algorithm>
#include <cmath>
#include "../core/ProcessSpec.h"
#include "../core/DenormalGuard.h"
#include "../core/FastMath.h"
#include "../circuits/dynamics/FetCompressor.h"
#include "../circuits/dynamics/VcaCompressor.h"
#include "../circuits/dynamics/PhotoCompressor.h"
#include "../circuits/drive/InputBuffer.h"
#include "../circuits/drive/OutputStage.h"
#include "../circuits/mixer/DryWetMixer.h"
#include "../constants/PartsConstants.h"

namespace patina {

// Limiter engine — FET / VCA / Opto 3-mode switchable limiter integration layer
//
// Difference between compressor and limiter:
//   Compressor: compresses above threshold by ratio (4:1, 8:1, etc.)
//   Limiter: limits above threshold with ceiling (equivalent to ∞:1, ultra-high ratio fixed)
//
// Signal path:
//   Outboard: Input → [Limiter] → OutputCeiling → Dry/Wet
//   Pedal:    Input → InputBuffer → [Limiter] → OutputCeiling → OutputStage → Dry/Wet
//   pedalMode=false (Default) for outboard quality
//
// Circuits by type:
//   FET:  Classic FET compressor/limiter All-Buttons style — JFET VCA, ultra-fast attack, transformer coloration
//   VCA:  Classic large-format console / classic dynamics processor style — THAT2180, ∞:1 hard knee, transparent
//   Opto: Classic optical compressor/limiter Limit mode - T4B photocell, programme-dependent, tube output
class LimiterEngine
{
public:
    enum Type { Fet = 0, Vca = 1, Opto = 2 };

    struct Params
    {
        int   type            = Vca;        // Limiter type
        float ceiling         = 0.8f;       // Output ceiling 0.0–1.0 → -20dB ~ 0dBFS
        float attack          = 0.1f;       // Attack 0.0–1.0 (FET/VCA)
        float release         = 0.4f;       // Release 0.0–1.0
        float outputGain      = 0.5f;       // Makeup gain 0.0–1.0
        float mix             = 1.0f;       // Dry/Wet 0.0–1.0

        // pedal mode
        bool   pedalMode      = false;
        double supplyVoltage  = 9.0;        // Supply voltage for pedal mode
    };

    LimiterEngine() = default;

    void prepare(const ProcessSpec& spec)
    {
        currentSpec = spec;

        fetComp.prepare(spec);
        vcaComp.prepare(spec);
        photoComp.prepare(spec);
        inputBuffer.prepare(spec);
        outputStage.prepare(spec);
    }

    void reset()
    {
        fetComp.reset();
        vcaComp.reset();
        photoComp.reset();
        inputBuffer.reset();
        outputStage.reset();
    }

    void processBlock(const float* const* input, float* const* output,
                      int numChannels, int numSamples, const Params& params)
    {
        const int nCh = std::min(numChannels, currentSpec.numChannels);
        if (nCh <= 0 || numSamples <= 0) return;
        ScopedDenormalDisable denormalGuard;

        // === ceiling → Map to each L3 circuit's parameters ===
        // ceiling 0.0–1.0 → -20dB ~ 0dBFS
        // Used as compressor threshold
        const float threshNorm = params.ceiling;

        // --- FET limiter: classic FET compressor/limiter All-Buttons mode ---
        FetCompressor::Params fetP;
        fetP.inputGain  = threshNorm;       // FET controls compression amount via input gain
        fetP.outputGain = params.outputGain;
        fetP.attack     = params.attack * 0.3f;  // Shorten attack range for limiter use (20μs–260μs)
        fetP.release    = params.release;
        fetP.ratio      = 4;               // All-Buttons (nuke) mode fixed
        fetP.mix        = 1.0f;

        // --- VCA Limiter: SSL / dbx ∞:1 hard knee ---
        VcaCompressor::Params vcaP;
        vcaP.threshold  = threshNorm;
        vcaP.ratio      = 1.0f;            // Maximum ratio (50:1 ≈ ∞:1)
        vcaP.attack     = params.attack * 0.2f;  // Further shortened for limiter use (0.1ms–16ms)
        vcaP.release    = params.release;
        vcaP.outputGain = params.outputGain;
        vcaP.mix        = 1.0f;
        vcaP.kneeMode   = 1;               // Fixed hard knee

        // --- Opto Limiter: Classic optical compressor/limiter Limit mode ---
        PhotoCompressor::Params optoP;
        optoP.peakReduction = 1.0f - threshNorm;
        optoP.outputGain    = params.outputGain;
        optoP.mode          = 1;            // Fixed Limit mode
        optoP.mix           = 1.0f;

        // === Output ceiling (dBFS → linear gain) ===
        const double ceilingDb = -20.0 + (double)params.ceiling * 20.0; // -20 ~ 0 dBFS
        const double ceilingLin = std::pow(10.0, ceilingDb / 20.0);

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < nCh; ++ch)
            {
                float dry = FastMath::sanitize(input[ch][i]);
                float wet = params.pedalMode ? inputBuffer.process(ch, dry) : dry;

                // Limiter
                switch (params.type)
                {
                    case Opto:
                        wet = photoComp.process(ch, wet, optoP);
                        break;
                    case Fet:
                        wet = fetComp.process(ch, wet, fetP);
                        break;
                    case Vca:
                    default:
                        wet = vcaComp.process(ch, wet, vcaP);
                        break;
                }

                // Output ceiling clamp (final safety net)
                wet = std::clamp((double)wet, -ceilingLin, ceilingLin);

                if (params.pedalMode)
                    wet = outputStage.process(ch, (float)wet, params.supplyVoltage);

                // Dry/Wet Mix
                float gDry, gWet;
                DryWetMixer::equalPowerGainsFast(params.mix, gDry, gWet);
                output[ch][i] = dry * gDry + (float)wet * gWet;
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
        return vcaComp.getGainReductionDb(channel);
    }

    float getFetGainReductionDb(int channel = 0) const { return fetComp.getGainReductionDb(channel); }
    float getVcaGainReductionDb(int channel = 0) const { return vcaComp.getGainReductionDb(channel); }
    float getOptoGainReductionDb(int channel = 0) const { return photoComp.getGainReductionDb(channel); }

    // internal access (for diagnostics/testing)
    const FetCompressor&   getFetComp()   const { return fetComp; }
    const VcaCompressor&   getVcaComp()   const { return vcaComp; }
    const PhotoCompressor& getOptoComp()  const { return photoComp; }

private:
    ProcessSpec currentSpec;

    FetCompressor    fetComp;
    VcaCompressor    vcaComp;
    PhotoCompressor  photoComp;
    InputBuffer      inputBuffer;
    OutputStage      outputStage;
};

} // namespace patina
