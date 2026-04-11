#pragma once
#include <cmath>
#include "AnalogLfo.h"

// modulation bus — stereo distribution and chorus modulation control of LFO signals
struct ModulationBus {
    // partial inversion (-0.75) instead of full inversion (-1.0) to suppress in-head localization
    // inverts phase on odd channels to create stereo spread, but prevents excessive separation
    static inline double stereoSign(int ch) noexcept { return (ch & 1) ? -0.75 : 1.0; }

    // triangle wave-based clock modulation value (with stereo sign and depth 0–1)
    static inline double tri(const AnalogLfo& lfo, int ch, float depth01) noexcept {
        if (depth01 <= 0.0f) return 0.0;
        return stereoSign(ch) * static_cast<double>(lfo.getTri(ch));
    }

    // sine-like read position modulation; when dual=true, distributes common value (such as ch0)
    static inline double sinv(const AnalogLfo& lfo, int ch, bool dual, double common, float depth01) noexcept {
        if (depth01 <= 0.0f) return 0.0;
        return dual ? common : (stereoSign(ch) * static_cast<double>(lfo.getSinLike(ch)));
    }

    // for clock modulation: sine wave with read position modulation and phase offset
    // in real BBD chorus, read position and clock frequency don't perfectly synchronize, creating complex "undulation"
    static inline double sinvForClock(const AnalogLfo& lfo, int ch, bool dual, double common, float depth01) noexcept {
        if (depth01 <= 0.0f) return 0.0;
        // adds approximately 20-30° phase difference relative to read position modulation (sinv)
        // shifts phase by mixing 85% sine + 15% triangle (mimicking real BBD asynchrony)
        double sinPart = dual ? common : (stereoSign(ch) * static_cast<double>(lfo.getSinLike(ch)));
        double triPart = stereoSign(ch) * static_cast<double>(lfo.getTri(ch));
        return sinPart * 0.82 + triPart * 0.18;  // equivalent to approximately 25° phase difference
    }

    static inline double commonSinv(const AnalogLfo& lfo) noexcept { return static_cast<double>(lfo.getSinLike(0)); }
};
