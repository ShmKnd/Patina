#pragma once

#include <algorithm>
#include <cmath>
#include "../core/ProcessSpec.h"
#include "../core/DenormalGuard.h"
#include "../core/FastMath.h"
#include "../circuits/filters/StateVariableFilter.h"
#include "../circuits/filters/LadderFilter.h"
#include "../circuits/filters/ToneFilter.h"
#include "../circuits/saturation/TubePreamp.h"
#include "../circuits/saturation/WaveFolder.h"
#include "../circuits/saturation/TapeSaturation.h"
#include "../circuits/drive/DiodeClipper.h"
#include "../circuits/drive/InputBuffer.h"
#include "../circuits/drive/OutputStage.h"
#include "../circuits/mixer/DryWetMixer.h"

namespace patina {

// Dual filter + triple drive engine
// Signal path:
//   Serial:   Input → Drive1 → Filter1 → Drive2 → Filter2 → Drive3 → Dry/Wet
//   Parallel: Input → Drive1 → [Filter1 | Filter2] mix → Drive3 → Dry/Wet
//   Pedal:    InputBuffer / OutputStage added to each mode
//
// Filter: StateVariableFilter (OTA-SVF) / LadderFilter — LP/HP/BP/Ladder
// Slope: -6dB / -12dB / -18dB / -24dB (Ladder is always -24dB)
// Drive: Tube / Diode / WaveFolder / Tape selection
class FilterEngine
{
public:
    // Drive type
    enum DriveType { Tube = 0, Diode = 1, Wave = 2, Tape = 3 };

    // Filter type
    enum FilterType { LowPass = 0, HighPass = 1, BandPass = 2, Ladder = 3 };

    // Slope
    enum Slope { Slope_6dB = 0, Slope_12dB = 1, Slope_18dB = 2, Slope_24dB = 3 };

    // Routing
    enum Routing { Serial = 0, Parallel = 1 };

    struct Params
    {
        // Routing
        int routing = 0;               // 0=Serial, 1=Parallel

        // Filter 1
        float filter1CutoffHz = 1000.0f;  // 20–20000 Hz
        float filter1Resonance = 0.5f;    // 0.0–1.0
        int   filter1Type = 0;            // 0=LP, 1=HP, 2=BP, 3=Ladder
        int   filter1Slope = 1;           // 0=-6dB, 1=-12dB, 2=-18dB, 3=-24dB

        // Filter 2
        float filter2CutoffHz = 2000.0f;  // 20–20000 Hz
        float filter2Resonance = 0.5f;    // 0.0–1.0
        int   filter2Type = 0;            // 0=LP, 1=HP, 2=BP, 3=Ladder
        int   filter2Slope = 1;           // 0=-6dB, 1=-12dB, 2=-18dB, 3=-24dB

        // Drive 1 (pre)
        float drive1Amount = 0.0f;        // 0.0–1.0
        int   drive1Type = 0;             // 0=Tube, 1=Diode, 2=Wave, 3=Tape

        // Drive 2 (mid — Serial only)
        float drive2Amount = 0.0f;        // 0.0–1.0
        int   drive2Type = 0;

        // Drive 3 (post)
        float drive3Amount = 0.0f;        // 0.0–1.0
        int   drive3Type = 0;

        // Overall
        float outputLevel = 0.7f;         // 0.0–1.0
        float mix         = 1.0f;         // Dry/Wet 0.0–1.0
        float temperature = 25.0f;        // °C
        double supplyVoltage = 9.0;       // V

        bool  pedalMode   = false;        // true=pedal quality
        bool  normalize   = true;         // true=filter gain compensation ON (musical voicing)
    };

    FilterEngine() = default;

    void prepare(const ProcessSpec& spec)
    {
        currentSpec = spec;

        inputBuffer.prepare(spec);
        outputStage.prepare(spec);

        for (int f = 0; f < 2; ++f)
        {
            svfMain[f].prepare(spec);
            svfCascade[f].prepare(spec);
            onePole[f].prepare(spec);
            ladder[f].prepare(spec);
        }

        for (int s = 0; s < 3; ++s)
        {
            tube[s].prepare(spec);
            diode[s].prepare(spec);
            wave[s].prepare(spec);
            tape[s].prepare(spec);
        }
    }

    void reset()
    {
        inputBuffer.reset();
        outputStage.reset();

        for (int f = 0; f < 2; ++f)
        {
            svfMain[f].reset();
            svfCascade[f].reset();
            onePole[f].reset();
            ladder[f].reset();
        }

        for (int s = 0; s < 3; ++s)
        {
            tube[s].reset();
            diode[s].reset();
            wave[s].reset();
            tape[s].reset();
        }
    }

    void processBlock(const float* const* input, float* const* output,
                      int numChannels, int numSamples, const Params& params)
    {
        const int nCh = std::min(numChannels, currentSpec.numChannels);
        if (nCh <= 0 || numSamples <= 0) return;
        ScopedDenormalDisable denormalGuard;

        // Update filter parameters
        updateFilterParams(0, params.filter1CutoffHz, params.filter1Resonance);
        updateFilterParams(1, params.filter2CutoffHz, params.filter2Resonance);

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < nCh; ++ch)
            {
                float dry = FastMath::sanitize(input[ch][i]);
                float wet = dry;

                // Input buffer — pedal mode only
                if (params.pedalMode)
                    wet = inputBuffer.process(ch, wet);

                // ─── Drive 1 (Pre-drive) ───
                wet = processDrive(0, ch, wet, params.drive1Amount, params.drive1Type, params);

                if (params.routing == Routing::Serial)
                {
                    // ─── Serial: Drive1 → Filter1 → Drive2 → Filter2 → Drive3 ───
                    wet = processFilter(0, ch, wet, params.filter1Type, params.filter1Slope,
                                        params.filter1CutoffHz, params.filter1Resonance, params.temperature, params.normalize);
                    wet = processDrive(1, ch, wet, params.drive2Amount, params.drive2Type, params);
                    wet = processFilter(1, ch, wet, params.filter2Type, params.filter2Slope,
                                        params.filter2CutoffHz, params.filter2Resonance, params.temperature, params.normalize);
                }
                else
                {
                    // ─── Parallel: Drive1 → [Filter1 | Filter2] → Drive3 ───
                    float f1 = processFilter(0, ch, wet, params.filter1Type, params.filter1Slope,
                                             params.filter1CutoffHz, params.filter1Resonance, params.temperature, params.normalize);
                    float f2 = processFilter(1, ch, wet, params.filter2Type, params.filter2Slope,
                                             params.filter2CutoffHz, params.filter2Resonance, params.temperature, params.normalize);
                    wet = (f1 + f2) * 0.5f;
                }

                // ─── Drive 3 (Post-drive) ───
                wet = processDrive(2, ch, wet, params.drive3Amount, params.drive3Type, params);

                // Output level
                wet *= params.outputLevel;

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

    // internal access (for diagnostics)
    const StateVariableFilter& getFilter1() const { return svfMain[0]; }
    const StateVariableFilter& getFilter2() const { return svfMain[1]; }

private:
    // Batch update filter parameters
    void updateFilterParams(int slot, float cutoffHz, float resonance)
    {
        svfMain[slot].setCutoffHz(cutoffHz);
        svfMain[slot].setResonance(resonance);
        svfCascade[slot].setCutoffHz(cutoffHz);
        svfCascade[slot].setResonance(resonance);
        onePole[slot].updateToneFilter(cutoffHz);
    }

    // Filter gain compensation
    // Normalize volume drop caused by cutoff/resonance (up to ~8dB)
    // No compensation for operations that increase volume (resonance peak distortion, etc.)
    float calcFilterCompensation(int type, int slope, float cutoffHz, float resonance)
    {
        // Normalize on logarithmic scale: 0.0 = 20Hz, 1.0 = 20000Hz
        const float logMin = std::log(20.0f);
        const float logMax = std::log(20000.0f);
        float norm = (std::log(std::max(cutoffHz, 20.0f)) - logMin) / (logMax - logMin);
        norm = std::max(0.0f, std::min(1.0f, norm));

        // Slope coefficient: steeper slope produces larger correction
        float sf;
        switch (slope)
        {
            case Slope_6dB:  sf = 0.4f; break;
            case Slope_12dB: sf = 0.8f; break;
            case Slope_18dB: sf = 1.1f; break;
            case Slope_24dB: sf = 1.4f; break;
            default:         sf = 0.8f; break;
        }

        // --- Volume drop correction by cutoff ---
        float cutoffCompDb = 0.0f;
        switch (type)
        {
            case FilterType::LowPass:
            case FilterType::Ladder:
                cutoffCompDb = sf * (1.0f - norm) * 6.0f;
                break;
            case FilterType::HighPass:
                cutoffCompDb = sf * norm * 6.0f;
                break;
            case FilterType::BandPass:
                cutoffCompDb = sf * 4.0f;
                break;
        }

        // --- Volume drop correction by resonance ---
        // At high resonance, passband energy decreases
        // （Only the peak area is amplified; overall RMS decreases）
        // resonance 0.0→1.0 gives up to ~4dB of compensation
        float resoCompDb = 0.0f;
        if (type != FilterType::Ladder)
        {
            // SVF type: higher resonance narrows the passband, reducing volume
            resoCompDb = resonance * resonance * 4.0f * sf;
        }
        else
        {
            // Ladder: volume drops significantly just before self-oscillation
            resoCompDb = resonance * resonance * 5.0f;
        }

        float compDb = cutoffCompDb + resoCompDb;
        compDb = std::min(compDb, 10.0f);  // Maximum ~10dB

        // Compensation gain is always >= 1.0 (never reduces volume, only boosts)
        float gain = std::pow(10.0f, compDb / 20.0f);
        return std::max(gain, 1.0f);
    }

    // Filter processing dispatch
    // Signal path by slope:
    //   -6dB:  1-pole (ToneFilter LPF / HPF = x - LPF / BPF → SVF -12dB fallback)
    //   -12dB: 1× SVF (2-pole)
    //   -18dB: SVF + 1-pole cascade (3-pole)
    //   -24dB: 2× SVF cascade (4-pole)
    //   Ladder: LadderFilter (-24dB/oct fixed, LP only)
    float processFilter(int slot, int ch, float x, int type, int slope,
                        float cutoffHz, float resonance, float temperature,
                        bool normalize = true)
    {
        float y;

        // Ladder mode — always -24dB/oct
        if (type == FilterType::Ladder)
        {
            LadderFilter::Params lp;
            lp.cutoffHz = cutoffHz;
            lp.resonance = resonance;
            lp.temperature = temperature;
            y = ladder[slot].process(ch, x, lp);
        }
        else
        {
            // SVF base mode
            switch (slope)
            {
            case Slope_6dB:
            {
                float lp = onePole[slot].processSample(ch, x);
                if (type == FilterType::LowPass)       y = lp;
                else if (type == FilterType::HighPass)  y = x - lp;
                else  y = svfMain[slot].process(ch, x, /*BP*/ 2);
                break;
            }
            case Slope_18dB:
            {
                float sv = svfMain[slot].process(ch, x, type);
                if (type == FilterType::LowPass)
                    y = onePole[slot].processSample(ch, sv);
                else if (type == FilterType::HighPass)
                    y = sv - onePole[slot].processSample(ch, sv);
                else
                    y = onePole[slot].processSample(ch, sv);
                break;
            }
            case Slope_24dB:
                y = svfCascade[slot].process(ch,
                        svfMain[slot].process(ch, x, type), type);
                break;
            case Slope_12dB:
            default:
                y = svfMain[slot].process(ch, x, type);
                break;
            }
        }

        // Apply gain compensation (only when normalize=true)
        if (normalize)
        {
            float comp = calcFilterCompensation(type, slope, cutoffHz, resonance);
            y *= comp;
        }
        return y;
    }

    // Drive processing dispatch
    // In the range amount 0→0.2, crossfade dry/wet to prevent abrupt changes
    float processDrive(int slot, int ch, float x, float amount, int type,
                       const Params& params)
    {
        if (amount < 0.001f) return x;

        float driven;
        switch (type)
        {
        case DriveType::Tube:
        {
            TubePreamp::Params tp;
            tp.drive = amount;
            tp.outputLevel = 1.0f;
            tp.tubeAge = 0.0f;
            driven = tube[slot].process(ch, x, tp);
            break;
        }
        case DriveType::Diode:
        {
            DiodeClipper::Params dp;
            dp.drive = amount;
            dp.mode = 1;  // Diode mode
            dp.temperature = params.temperature;
            driven = diode[slot].process(ch, x, dp);
            break;
        }
        case DriveType::Wave:
        {
            WaveFolder::Params wp;
            wp.foldAmount = amount;
            wp.symmetry = 1.0f;
            wp.numStages = 4;
            wp.temperature = params.temperature;
            driven = wave[slot].process(ch, x, wp);
            break;
        }
        case DriveType::Tape:
        {
            TapeSaturation::Params sp;
            sp.saturation = amount;
            sp.inputGain = 0.0f;
            driven = tape[slot].process(ch, x, sp);
            break;
        }
        default:
            return x;
        }

        // Smooth onset: 0→1 over amount 0.0→0.2
        // Smoothly introduce nonlinear processing while preserving stereo width
        float blend = std::min(amount * 5.0f, 1.0f);
        return x + blend * (driven - x);
    }

    ProcessSpec currentSpec;

    InputBuffer   inputBuffer;
    OutputStage   outputStage;

    // Filter — 2 slots
    StateVariableFilter svfMain[2];      // Main SVF (2-pole)
    StateVariableFilter svfCascade[2];   // For cascade (4-pole construction)
    ToneFilter          onePole[2];      // 1-pole (for -6dB / -18dB)
    LadderFilter        ladder[2];       // Ladder (-24dB/oct fixed)

    // Drive — 3 slots x 4 types
    TubePreamp     tube[3];
    DiodeClipper   diode[3];
    WaveFolder     wave[3];
    TapeSaturation tape[3];
};

} // namespace patina
