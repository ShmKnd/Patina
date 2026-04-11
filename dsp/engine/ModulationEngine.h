#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include "../core/ProcessSpec.h"
#include "../core/DenormalGuard.h"
#include "../core/FastMath.h"
#include "../circuits/drive/InputBuffer.h"
#include "../circuits/drive/OutputStage.h"
#include "../circuits/filters/PhaserStage.h"
#include "../circuits/compander/TremoloVCA.h"
#include "../circuits/modulation/AnalogLfo.h"
#include "../circuits/modulation/StereoImage.h"
#include "../circuits/mixer/DryWetMixer.h"
#include "../constants/PartsConstants.h"

namespace patina {

// Modulation engine — phaser / tremolo / chorus integration layer
// Signal path:
//   Outboard: Input → [Phaser|Tremolo|Chorus] → Dry/Wet
//   Pedal:    Input → InputBuffer → [Phaser|Tremolo|Chorus] → OutputStage → Dry/Wet
//   pedalMode=false (default) for outboard quality, true for pedal quality
class ModulationEngine
{
public:
    enum Type { Phaser = 0, Tremolo = 1, Chorus = 2 };

    struct Params
    {
        int   type          = Phaser;   // Effect type
        float rate          = 0.5f;     // LFO rate (Hz) 0.1–10.0
        float depth         = 0.5f;     // Depth 0.0–1.0
        float feedback      = 0.3f;     // Feedback 0.0–0.95 (Phaser)
        float mix           = 0.5f;     // Dry/Wet 0.0–1.0
        double supplyVoltage = 9.0;     // supply voltage (V)

        // Phaser-specific
        float centerFreqHz  = 1000.0f;  // Center frequency (Hz)
        float freqSpreadHz  = 800.0f;   // Frequency spread (Hz)
        int   numStages     = 4;        // Number of stages (2–12)
        float temperature   = 25.0f;    // operating temperature (°C)

        // Tremolo-specific
        int   tremoloMode   = 0;        // 0=Bias, 1=Optical, 2=VCA
        bool  stereoPhaseInvert = false; // Stereo phase inversion

        // Chorus-specific
        float chorusDelayMs = 7.0f;     // Base delay (ms)
        float stereoWidth   = 0.5f;     // Stereo width 0.0–1.0

        bool  pedalMode      = false;   // true=pedal quality(InputBuffer+OutputStage), false=outboard quality
    };

    ModulationEngine() = default;

    void prepare(const ProcessSpec& spec)
    {
        currentSpec = spec;
        const int nCh = spec.numChannels;
        const double sr = spec.sampleRate;

        inputBuffer.prepare(spec);
        phaserStage.prepare(spec);
        tremoloVca.prepare(spec);
        lfo.prepare(spec);
        outputStage.prepare(spec);

        // Chorus delay buffer (max 50ms)
        maxChorusDelay = static_cast<int>(sr * 0.05) + 1;
        chorusBuffer.resize((size_t)nCh);
        for (auto& ch : chorusBuffer)
            ch.assign((size_t)maxChorusDelay, 0.0f);
        chorusWritePos = 0;
    }

    void reset()
    {
        inputBuffer.reset();
        phaserStage.reset();
        tremoloVca.reset();
        lfo.reset();
        outputStage.reset();

        for (auto& ch : chorusBuffer)
            std::fill(ch.begin(), ch.end(), 0.0f);
        chorusWritePos = 0;
    }

    void processBlock(const float* const* input, float* const* output,
                      int numChannels, int numSamples, const Params& params)
    {
        const int nCh = std::min(numChannels, currentSpec.numChannels);
        if (nCh <= 0 || numSamples <= 0) return;
        ScopedDenormalDisable denormalGuard;

        lfo.setRateHz(params.rate);
        lfo.setSupplyVoltage(params.supplyVoltage);

        // Phaser Parameters
        PhaserStage::Params phaserP;
        phaserP.depth        = params.depth;
        phaserP.feedback     = params.feedback;
        phaserP.centerFreqHz = params.centerFreqHz;
        phaserP.freqSpreadHz = params.freqSpreadHz;
        phaserP.numStages    = params.numStages;
        phaserP.temperature  = params.temperature;

        // Tremolo Parameters
        TremoloVCA::Params tremP;
        tremP.depth             = params.depth;
        tremP.mode              = params.tremoloMode;
        tremP.stereoPhaseInvert = params.stereoPhaseInvert;

        for (int i = 0; i < numSamples; ++i)
        {
            lfo.stepAll();

            for (int ch = 0; ch < nCh; ++ch)
            {
                float dry = FastMath::sanitize(input[ch][i]);
                float wet = params.pedalMode ? inputBuffer.process(ch, dry) : dry;

                switch (params.type)
                {
                    case Phaser:
                    {
                        phaserP.lfoValue = lfo.getSinLike(ch);
                        wet = phaserStage.process(ch, wet, phaserP);
                        break;
                    }
                    case Tremolo:
                    {
                        tremP.lfoValue = lfo.getSinLike(ch);
                        wet = tremoloVca.process(ch, wet, tremP);
                        break;
                    }
                    case Chorus:
                    {
                        // Chorus: short delay + LFO modulation + stereo width
                        chorusBuffer[(size_t)ch][(size_t)chorusWritePos] = wet;

                        double baseDelay = (double)params.chorusDelayMs * 0.001
                                           * currentSpec.sampleRate;
                        double modulation = (double)lfo.getSinLike(ch) * params.depth
                                            * baseDelay * 0.5;
                        double readPos = (double)chorusWritePos - baseDelay - modulation;
                        while (readPos < 0.0) readPos += maxChorusDelay;

                        int rp0 = (int)readPos;
                        double frac = readPos - rp0;
                        int rp1 = (rp0 + 1) % maxChorusDelay;
                        wet = (float)((1.0 - frac) * chorusBuffer[(size_t)ch][(size_t)rp0]
                                     + frac * chorusBuffer[(size_t)ch][(size_t)rp1]);
                        break;
                    }
                }

                if (params.pedalMode)
                    wet = outputStage.process(ch, wet, params.supplyVoltage);

                // Dry/Wet Mix
                float gDry, gWet;
                DryWetMixer::equalPowerGainsFast(params.mix, gDry, gWet);
                output[ch][i] = dry * gDry + wet * gWet;
            }

            // Stereo width (Chorus)
            if (params.type == Chorus && nCh >= 2)
                StereoImage::widenEqualPowerSIMD(output[0][i], output[1][i], params.stereoWidth);

            if (params.type == Chorus)
                chorusWritePos = (chorusWritePos + 1) % maxChorusDelay;
        }

        // zero-clear surplus channels
        for (int ch = nCh; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                output[ch][i] = 0.0f;
    }

    const PhaserStage&  getPhaserStage() const { return phaserStage; }
    const TremoloVCA&   getTremoloVca()  const { return tremoloVca; }
    const AnalogLfo&    getLfo()         const { return lfo; }

private:
    ProcessSpec currentSpec;

    InputBuffer   inputBuffer;
    PhaserStage   phaserStage;
    TremoloVCA    tremoloVca;
    AnalogLfo     lfo;
    OutputStage   outputStage;

    // Chorus delay buffer
    std::vector<std::vector<float>> chorusBuffer;
    int maxChorusDelay = 0;
    int chorusWritePos = 0;
};

} // namespace patina
