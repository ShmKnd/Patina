#pragma once
#include <vector>
#include "ToneFilter.h"

// Tone shaper — utility for applying tone filters to delay output
struct ToneShaper {
    // Apply tone filter to delay output vector (with exception-safe handling)
    static inline void process(ToneFilter& bank, int numChannels, std::vector<float>& delayedOut) {
        for (int ch = 0; ch < numChannels && (size_t) ch < delayedOut.size(); ++ch) {
            try { delayedOut[(size_t)ch] = bank.processSample(ch, delayedOut[(size_t)ch]); }
            catch (...) { /* absorbs per-sample errors */ }
        }
    }
};
