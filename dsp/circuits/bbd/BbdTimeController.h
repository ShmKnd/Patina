#pragma once
#include <cmath>
#include "../../constants/PartsConstants.h"
#include "../../core/AudioCompat.h"

// time/stage controller — BBD clock constraints and stage count optimization
struct BbdTimeController {
    struct Result {
        double desiredDelaySamples {0.0};    // requested delay in samples
        double actualDelaySamples {0.0};     // actual delay in samples
        int stagesForProcessing {0};         // stage count for processing
        double bbdClockHz {0.0};            // BBD clock frequency
        int lastSmoothedStagesNext {0};     // smoothed stage count for next iteration
    };

    // resolve stage count and clock constraints from time and mode
    Result resolve(double timeMs,
                   int modeIndex,            // 0: Simple, 1: Classic, 2: Expert
                   int stagesParam,          // Raw stage value from APVTS for Expert mode
                   double sampleRate,
                   int lastSmoothedStages,   // previous smoothed stage count for Simple mode
                   bool emulateBBD,
                   double /*clockJitterStd*/,
                   int classicStagesParam = 2) const // selector for Classic mode (0=1024, 1=2048, 2=4096, 3=8192)
    {
        Result r{};
        r.desiredDelaySamples = std::max(0.0, (timeMs * 0.001) * sampleRate);

        // Simple/Classic/Expert mode branching
        int stagesForProcessing = 0;
        if (modeIndex == 0) {
            // Simple: linked to TIME, with smoothing
            const int kMinStages = 1;    // minimum 1 stage (for extreme degradation effects)
            const int kMaxStages = 8192; // maximum 8192 stages (classic BBD delay unit compatible)
            // requested stage count via smoothing steps
            const double t1 = sampleRate * 0.01; // 10msthreshold
            const double t2 = sampleRate * 0.05; // 50msthreshold  
            const double t3 = sampleRate * 0.20; // 200msthreshold
            double stepTarget = 2.0;
            if (r.desiredDelaySamples <= t1) stepTarget = 0.5;        // short delay: high resolution
            else if (r.desiredDelaySamples <= t2) {
                double frac = (r.desiredDelaySamples - t1) / std::max(1.0, t2 - t1);
                stepTarget = 0.5 + frac * (1.0 - 0.5);               // linear transition
            } else if (r.desiredDelaySamples <= t3) {
                double frac = (r.desiredDelaySamples - t2) / std::max(1.0, t3 - t2);
                stepTarget = 1.0 + frac * (2.0 - 1.0);               // linear transition
            }
            int desiredStages = (int) std::round(r.desiredDelaySamples / std::max(0.5, stepTarget));
            desiredStages = std::clamp(desiredStages, kMinStages, kMaxStages);

            int newSmoothed = (lastSmoothedStages < kMinStages) ? desiredStages : lastSmoothedStages;
            if (desiredStages > newSmoothed) newSmoothed = desiredStages; // immediate rise
            else if (desiredStages < newSmoothed) {
                const double alpha = 0.12; // gentle decay with 0.12 coefficient
                newSmoothed = (int) std::round(newSmoothed * (1.0 - alpha) + desiredStages * alpha);
                if (newSmoothed < desiredStages) newSmoothed = desiredStages;
            }
            newSmoothed = std::clamp(newSmoothed, kMinStages, kMaxStages);
            stagesForProcessing = newSmoothed;
            r.lastSmoothedStagesNext = newSmoothed;
        } else if (modeIndex == 1) {
            // Classic: fixed at 1024/2048/4096/8192
            const int classicStagesOptions[] = {1024, 2048, 4096, 8192};
            int idx = std::clamp(classicStagesParam, 0, 3);
            stagesForProcessing = classicStagesOptions[idx];
            r.lastSmoothedStagesNext = lastSmoothedStages; // no change
        } else {
            // Expert: user direct specification
            stagesForProcessing = std::clamp(stagesParam, 1, 8192); // Expert: direct specification
            r.lastSmoothedStagesNext = lastSmoothedStages; // no change
        }

        r.stagesForProcessing = stagesForProcessing;
        r.actualDelaySamples = r.desiredDelaySamples;
        r.actualDelaySamples = std::max(1.5, r.actualDelaySamples); // zero-delay leak avoidance (minimum 1.5 samples)

        // BBD clock calculation (calculated as reference; no constraints needed for software emulation)
        // The 10kHz-100kHz constraint of actual MN3005 is a hardware physical limitation;
        // not applicable to software sample & hold.
        // extreme step values (0.1 or 10000) can be processed numerically safely.
        r.bbdClockHz = 0.0;
        if (emulateBBD && r.desiredDelaySamples > 0.0) {
            r.bbdClockHz = (double) stagesForProcessing * sampleRate / std::max(1.0, r.desiredDelaySamples);
            // clock constraints removed: software can operate at any clock/step value
            // this allows users to freely create creative sounds (extreme bit-crush, etc.)
        }
        r.stagesForProcessing = stagesForProcessing;
        return r;
    }
};
