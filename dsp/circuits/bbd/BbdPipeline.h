#pragma once

#ifndef ANALOG_DELAY_DBG
    #define ANALOG_DELAY_DBG(x) ((void)0)
#endif
#include <vector>
#include <cmath>
#include "BbdStageEmulator.h"

struct BbdPipeline {
    // BBD stage low-pass filter application and safe fallback processing
    // ・apply BBD stage low-pass via emulator
    // ・fall back to pre-LPF output during extreme attenuation (energy loss)
    //   1e-8: lower bound for input energy (silence judgment)
    //   1e-12: lower bound for output energy (loss judgment)
    //   1e-6: abnormal attenuation judgment by I/O ratio (less than one millionth)
    //   0xFF: warning log output every 256 times
    static inline void process(
        std::vector<float>& delayedOut,
        std::vector<float>& preLpfTmp,
        BbdStageEmulator& emulator,
        double step,
        int stagesForProcessing,
        double effectiveSupplyV,
        bool enableAging,
        double ageYears)
    {
        preLpfTmp = delayedOut; // temporarily save input signal
        emulator.process(delayedOut, step, stagesForProcessing, effectiveSupplyV, enableAging, ageYears);

        // judge energy change by sum of absolute values of input/output
        double energyBefore = 0.0, energyAfter = 0.0;
        for (size_t k = 0; k < preLpfTmp.size(); ++k) {
            energyBefore += std::fabs((double)preLpfTmp[k]);
            energyAfter  += std::fabs((double)delayedOut[k]);
        }
        // fall back to pre-LPF output on extreme (abnormal) attenuation
        if (energyBefore > 1e-8 && energyAfter < 1e-12 && energyAfter < energyBefore * 1e-6) {
            static int lpfMuteDiag = 0;
            if ((++lpfMuteDiag & 0xFF) == 0) {
                ANALOG_DELAY_DBG(patina::compat::String("[AnalogDelay][WARN] LPF caused delayed signal to vanish (stages=")
                    + patina::compat::String(std::to_string(stagesForProcessing).c_str())
                    + patina::compat::String(") - reverting to pre-LPF output"));
            }
            delayedOut = preLpfTmp; // fallback
        }
    }
};
