#pragma once

#include <algorithm>
#include <cmath>
#include "../core/ProcessSpec.h"
#include "../core/DenormalGuard.h"
#include "../core/FastMath.h"
#include "../circuits/modulation/EnvelopeGenerator.h"
#include "../circuits/compander/EnvelopeFollower.h"
#include "../circuits/drive/InputBuffer.h"
#include "../circuits/drive/OutputStage.h"
#include "../circuits/mixer/DryWetMixer.h"
#include "../constants/PartsConstants.h"
#include "../parts/VcaPrimitive.h"

namespace patina {

// Envelope generator engine — VCA control integration layer with ADSR / AD / AR envelopes
//
// Applications:
//   - Synthesizer: amplitude envelope via AnalogVCO → EnvelopeGeneratorEngine
//   - Effect: shape audio input amplitude with ADSR (transient shaper use)
//   - Auto-gate: gate trigger by input level → amplitude control via ADSR
//
// Signal path:
//   Outboard: Input → VCA(ADSR envelope) → OutputGain → Dry/Wet
//   Pedal:    Input → InputBuffer → VCA(ADSR envelope) → OutputGain → OutputStage → Dry/Wet
//   pedalMode=false (default) for outboard quality, true for pedal quality
//
// Trigger modes:
//   External: Trigger via external gateOn()/gateOff() call (for synths/sequencers)
//   Auto:     Auto-trigger when input signal level exceeds threshold (for effects)
//
// L2 parts used by L3 circuit (EnvelopeGenerator):
//   DiodePrimitive (Si1N4148) — stage-switching diode
//   OTA_Primitive (CA3080)    — Charging current source
//   BJT_Primitive (Matched)   — Current mirror
//   RC_Element                — Timing RC circuit
//
// L2 parts used by L4 engine:
//   VcaPrimitive (THAT2180)   — Gain-controlled VCA cell
class EnvelopeGeneratorEngine
{
public:
    enum TriggerMode { External = 0, Auto = 1 };

    struct Params
    {
        // ADSR parameters
        float attack    = 0.3f;     // Attack 0.0–1.0 → 0.5ms ~ 5000ms
        float decay     = 0.3f;     // Decay 0.0–1.0 → 5ms ~ 5000ms
        float sustain   = 0.7f;     // Sustain 0.0–1.0
        float release   = 0.4f;     // Release 0.0–1.0 → 10ms ~ 10000ms
        int   envMode   = 0;        // 0=ADSR, 1=AD, 2=AR
        int   curve     = 0;        // 0=RC (exponential), 1=Linear

        // Trigger control
        int   triggerMode     = External;  // 0=External, 1=Auto
        float autoThresholdDb = -30.0f;    // Auto-trigger threshold (dBFS) -60 ~ 0

        // Velocity
        float velocity   = 1.0f;    // Velocity 0.0–1.0 (envelope amplitude scale)

        // Output control
        float vcaDepth   = 1.0f;    // VCA envelope depth 0.0–1.0 (0=bypass)
        float outputGain = 0.5f;    // Output gain 0.0–1.0
        float mix        = 1.0f;    // Dry/Wet 0.0–1.0

        // Environment
        float temperature = 25.0f;  // operating temperature (°C)

        // pedal mode
        bool   pedalMode      = false;
        double supplyVoltage  = 9.0;
    };

    EnvelopeGeneratorEngine() noexcept
        : vca(VcaPrimitive::THAT2180(), 700)
    {}

    void prepare(const ProcessSpec& spec)
    {
        currentSpec = spec;

        envGen.prepare(spec);
        envFollower.prepare(spec);
        inputBuffer.prepare(spec);
        outputStage.prepare(spec);

        const size_t nCh = (size_t)std::max(1, spec.numChannels);
        autoGateState.assign(nCh, 0);
    }

    void reset()
    {
        envGen.reset();
        envFollower.reset();
        inputBuffer.reset();
        outputStage.reset();
        std::fill(autoGateState.begin(), autoGateState.end(), (char)0);
    }

    // === External gate control (for External trigger mode) ===
    void gateOn(int channel = -1)
    {
        if (channel < 0)
            envGen.gateOnAll();
        else
            envGen.gateOn(channel);
    }

    void gateOff(int channel = -1)
    {
        if (channel < 0)
            envGen.gateOffAll();
        else
            envGen.gateOff(channel);
    }

    void processBlock(const float* const* input, float* const* output,
                      int numChannels, int numSamples, const Params& params)
    {
        const int nCh = std::min(numChannels, currentSpec.numChannels);
        if (nCh <= 0 || numSamples <= 0) return;
        ScopedDenormalDisable denormalGuard;

        // L3 envelope generator parameters
        EnvelopeGenerator::Params envP;
        envP.attack      = params.attack;
        envP.decay       = params.decay;
        envP.sustain     = params.sustain;
        envP.release     = params.release;
        envP.mode        = params.envMode;
        envP.curve       = params.curve;
        envP.temperature = params.temperature;

        // Envelope follower parameters for auto-trigger
        EnvelopeFollower::Params efP;
        efP.attackMs    = 1.0f;     // Fast detection
        efP.releaseMs   = 50.0f;
        efP.sensitivity = 1.0f;
        efP.mode        = 0;        // Peak

        // Threshold: dBFS → linear
        const double threshLin = std::pow(10.0, (double)params.autoThresholdDb / 20.0);

        // VCA depth
        const double vcaDepth = std::clamp((double)params.vcaDepth, 0.0, 1.0);

        // Velocity scale (multiplied to VCA output)
        const double velocityScale = std::clamp((double)params.velocity, 0.0, 1.0);

        // VCA temperature-dependent gain tracking error (THAT2180: ±0.003 dB/°C)
        const double vcaTempGain = vca.tempGainScale(params.temperature);

        // Output gain (quadratic curve → 0 ~ 4x)
        const double makeupGain = (double)params.outputGain * (double)params.outputGain * 4.0;

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < nCh; ++ch)
            {
                float dry = FastMath::sanitize(input[ch][i]);
                float wet = params.pedalMode ? inputBuffer.process(ch, dry) : dry;

                // === Auto-trigger: input level detection → gate control ===
                if (params.triggerMode == Auto)
                {
                    float level = envFollower.process(ch, wet, efP);
                    char& gateState = autoGateState[(size_t)ch];

                    if (!gateState && (double)level > threshLin)
                    {
                        envGen.gateOn(ch);
                        gateState = 1;
                    }
                    else if (gateState && (double)level < threshLin * kGateHysteresis)
                    {
                        envGen.gateOff(ch);
                        gateState = 0;
                    }
                }

                // === ADSR envelope generation ===
                float envValue = envGen.process(ch, envP);

                // === Velocity × envelope → VCA gain calculation ===
                // velocity=1.0: full envelope, velocity<1.0: reduced envelope amplitude
                double scaledEnv = (double)envValue * velocityScale;

                // depth=0: envelope disabled (full gain), depth=1: full control
                double vcaGain = 1.0 - vcaDepth * (1.0 - scaledEnv);
                vcaGain = std::clamp(vcaGain, 0.0, 1.0);

                // === Apply VCA gain (THAT2180 — with temperature compensation) ===
                double v = vca.applyGain((double)wet, vcaGain * vcaTempGain);

                // Output gain
                v *= makeupGain;

                if (params.pedalMode)
                    v = (double)outputStage.process(ch, (float)v, params.supplyVoltage);

                // Dry/Wet Mix
                float gDry, gWet;
                DryWetMixer::equalPowerGainsFast(params.mix, gDry, gWet);
                output[ch][i] = dry * gDry + (float)v * gWet;
            }
        }

        // zero-clear surplus channels
        for (int ch = nCh; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                output[ch][i] = 0.0f;
    }

    // === Get state ===

    // Current envelope value (0.0–1.0)
    float getEnvelope(int channel = 0) const
    {
        return envGen.getEnvelope(channel);
    }

    // Current stage
    EnvelopeGenerator::Stage getStage(int channel = 0) const
    {
        return envGen.getStage(channel);
    }

    // internal access (for diagnostics/testing)
    const EnvelopeGenerator& getEnvGen() const { return envGen; }
    const EnvelopeFollower&  getEnvFollower() const { return envFollower; }

private:
    static constexpr double kGateHysteresis = 0.7; // Auto-gate hysteresis

    ProcessSpec currentSpec;

    EnvelopeGenerator envGen;
    EnvelopeFollower  envFollower;
    VcaPrimitive      vca;
    InputBuffer       inputBuffer;
    OutputStage       outputStage;

    std::vector<char> autoGateState;
};

} // namespace patina
