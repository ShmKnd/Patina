#pragma once

#include <algorithm>
#include <cmath>
#include "../core/ProcessSpec.h"
#include "../core/DenormalGuard.h"
#include "../core/FastMath.h"
#include "../circuits/saturation/TubePreamp.h"
#include "../circuits/filters/StateVariableFilter.h"
#include "../circuits/compander/NoiseGate.h"
#include "../circuits/compander/EnvelopeFollower.h"
#include "../circuits/drive/InputBuffer.h"
#include "../circuits/drive/OutputStage.h"
#include "../circuits/mixer/DryWetMixer.h"
#include "../constants/PartsConstants.h"

namespace patina {

// Channel strip engine — analog console-style integrated channel processing
// Signal path:
//   Outboard: Input → InputTrim → NoiseGate(opt) → TubePreamp → SVF(EQ) → OutputTrim
//   Pedal:    Input → InputBuffer → InputTrim → NoiseGate(opt) → TubePreamp
//             → SVF(EQ) → OutputTrim → OutputStage
//   pedalMode=false (default) for outboard quality, true for pedal quality
//   (Metering via EnvelopeFollower)
class ChannelStripEngine
{
public:
    struct Params
    {
        // Preamp
        float preampDrive    = 0.15f;   // Preamp drive 0.0–1.0
        float preampBias     = 0.5f;    // Bias point 0.0–1.0
        float preampOutput   = 0.5f;    // PreampOutput level 0.0–1.0
        float tubeAge        = 0.0f;    // Tube aging 0.0–1.0

        // EQ (StateVariableFilter)
        bool  enableEq       = false;   // EQ enable (default: bypass)
        float eqCutoffHz     = 1000.0f; // Cutoff frequency (Hz)
        float eqResonance    = 0.5f;    // Resonance 0.0–1.0
        int   eqType         = 0;       // 0=LP, 1=HP, 2=BP, 3=Notch
        float eqTemperature  = 25.0f;   // EQoperating temperature (°C)

        // Noise gate (pre-stage)
        bool  enableGate      = false;
        float gateThresholdDb = -50.0f;
        float gateHysteresisDb = 6.0f;

        // Input/output trim
        float inputTrimDb    = 0.0f;    // Input trim (dB)
        float outputTrimDb   = 0.0f;    // Output trim (dB)

        // pedal mode
        bool  pedalMode      = false;   // true=pedal quality(InputBuffer+OutputStage), false=outboard quality
        double supplyVoltage = 9.0;     // Supply voltage (V) — for OutputStage in pedal mode
    };

    ChannelStripEngine() = default;

    void prepare(const ProcessSpec& spec)
    {
        currentSpec = spec;

        noiseGate.prepare(spec);
        tubePreamp.prepare(spec);
        svFilter.prepare(spec);
        envFollower.prepare(spec);
        inputBuffer.prepare(spec);
        outputStage.prepare(spec);
    }

    void reset()
    {
        noiseGate.reset();
        tubePreamp.reset();
        svFilter.reset();
        envFollower.reset();
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
        gateP.thresholdDb  = params.gateThresholdDb;
        gateP.hysteresisDb = params.gateHysteresisDb;

        // Preamp Parameters
        // Reduce drive range for console use: 0.0–1.0 → internal 0.0–0.3
        // This gives gain = 1.0 + (drive*0.3)*15 = 1.0–5.5× (was: 1.0–16×)
        TubePreamp::Params preP;
        preP.drive       = params.preampDrive * 0.3f;
        preP.bias        = params.preampBias;
        preP.outputLevel = params.preampOutput;
        preP.tubeAge     = params.tubeAge;

        // SVF settings
        if (params.enableEq)
        {
            svFilter.setCutoffHz(params.eqCutoffHz);
            svFilter.setResonance(params.eqResonance);
        }

        // Envelope follower (for metering)
        EnvelopeFollower::Params envP;
        envP.attackMs  = 5.0f;
        envP.releaseMs = 50.0f;

        // Trim
        const float inputTrim  = dbToLinear(params.inputTrimDb);
        const float outputTrim = dbToLinear(params.outputTrimDb);

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < nCh; ++ch)
            {
                float x = FastMath::sanitize(input[ch][i]);

                // Input buffer — pedal mode only
                if (params.pedalMode)
                    x = inputBuffer.process(ch, x);

                // Input trim
                x *= inputTrim;

                // Noise gate
                if (params.enableGate)
                    x = noiseGate.process(ch, x, gateP);

                // Preamp
                x = tubePreamp.process(ch, x, preP);

                // EQ
                if (params.enableEq)
                    x = svFilter.process(ch, x, params.eqType);

                // Output trim
                x *= outputTrim;

                // Output stage — pedal mode only
                if (params.pedalMode)
                    x = outputStage.process(ch, x, params.supplyVoltage);

                // Envelope tracking
                envFollower.process(ch, x, envP);

                output[ch][i] = x;
            }
        }

        // zero-clear surplus channels
        for (int ch = nCh; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                output[ch][i] = 0.0f;
    }

    // Metering
    float getOutputLevel(int channel = 0) const { return envFollower.getEnvelope(channel); }
    bool  isGateOpen(int channel = 0) const { return noiseGate.isGateOpen(channel); }

    // internal access (for diagnostics/testing)
    const TubePreamp&          getTubePreamp()   const { return tubePreamp; }
    const StateVariableFilter& getSvFilter()     const { return svFilter; }
    const NoiseGate&           getNoiseGate()    const { return noiseGate; }
    const EnvelopeFollower&    getEnvFollower()  const { return envFollower; }

private:
    static float dbToLinear(float dB) noexcept
    {
        return std::pow(10.0f, dB / 20.0f);
    }

    ProcessSpec currentSpec;

    NoiseGate            noiseGate;
    TubePreamp           tubePreamp;
    StateVariableFilter  svFilter;
    EnvelopeFollower     envFollower;
    InputBuffer          inputBuffer;
    OutputStage          outputStage;
};

} // namespace patina
