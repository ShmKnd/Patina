#pragma once
#include "DuckingMixer.h"
#include "../drive/OutputStage.h"

// GainUtils — Level compensation, output stage saturation, FS conversion
struct GainUtils : DuckingMixer {

    static inline float applyLevelComp(float v, bool isInstrument) noexcept {
        if (isInstrument)  return v * 10.0f;
        else               return v * 3.98107171f;
    }

    static inline float applyMasterAndSaturation(float v, int channel, double masterGain, OutputStage& outMod, bool emulateOpAmpSaturation, double effectiveSupplyV) noexcept {
        v *= (float) masterGain;
        if (emulateOpAmpSaturation)
            v = outMod.process(channel, v, effectiveSupplyV);
        return v;
    }

    static inline float toFS(float v, float vToFsGain) noexcept { return v * vToFsGain; }

    static inline float mixToFs(int channel,
                                float dryV, float wetV,
                                double mix01, double masterGain,
                                bool isInstrument,
                                OutputStage& outMod,
                                bool emulateOpAmpSaturation,
                                double effectiveSupplyV,
                                float vToFsGain,
                                bool enableAnalogDucking = true) noexcept
    {
        float v = enableAnalogDucking
            ? analogDuckingMix(dryV, wetV, mix01)
            : equalPowerMixVolt(dryV, wetV, mix01);

        constexpr float kPlus3dB = 1.41253754f;
        v *= kPlus3dB;
        v = applyLevelComp(v, isInstrument);
        v = applyMasterAndSaturation(v, channel, masterGain, outMod, emulateOpAmpSaturation, effectiveSupplyV);
        return toFS(v, vToFsGain);
    }
};
