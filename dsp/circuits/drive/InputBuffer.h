#pragma once
#include <vector>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <random>
#include "../../constants/PartsConstants.h"
#include "../../core/ProcessSpec.h"
#include "../../parts/OpAmpPrimitive.h"

#ifndef ANALOG_DELAY_DBG
    #define ANALOG_DELAY_DBG(x) ((void)0)
#endif

// TL072-style input buffer model (extended with BBD-level analog behavior)
// - Optional -20dB PAD (for instrument/line switching)
// - 1MΩ || C input impedance (1-pole low-pass from cable + input capacitance)
// - Supply voltage-dependent headroom (soft saturation characteristics)
// - Slew rate limiting (JRC4558D vs TL072CP characteristic difference model)
// - Input bias current noise (~30pA × √bandwidth)
// - Asymmetric saturation (different clipping curves for positive and negative sides)
// - Overload recovery time (gradual recovery after transients)
// - input offset voltage temperature drift
class InputBuffer {
public:
    InputBuffer() noexcept
        : sampleRate(PartsConstants::defaultSampleRate)
        , R_ohm(PartsConstants::R_input)
        , C_f(PartsConstants::C_inputBufferDefault)
        , supplyV(PartsConstants::V_supplyMax)
        , padEnabled(false)
    {}

    void prepare(int numChannels, double sr) noexcept {
        sampleRate = (sr > 1.0 ? sr : 44100.0);
        const size_t nCh = (size_t)std::max(1, numChannels);
        rcState.assign(nCh, 0.0);
        overloadRecovery.assign(nCh, 0.0);
        offsetDrift.assign(nCh, 0.0);
        updateAlpha();

        // OpAmpPrimitive per channel (for slew rate limiting)
        rebuildSlewInstances(nCh);

        overloadFlag.store(false, std::memory_order_relaxed);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept { prepare(spec.numChannels, spec.sampleRate); }

    void reset() noexcept {
        std::fill(rcState.begin(), rcState.end(), 0.0);
        std::fill(overloadRecovery.begin(), overloadRecovery.end(), 0.0);
        std::fill(offsetDrift.begin(), offsetDrift.end(), 0.0);
        for (auto& amp : slewAmps_) amp.reset();
        overloadFlag.store(false, std::memory_order_relaxed);
    }

    void setPadEnabled(bool en) noexcept { padEnabled.store(en, std::memory_order_relaxed); }
    void setInputCapacitance(double cFarads) noexcept {
        double c = std::clamp(cFarads, 1e-12, 5e-9);
        C_f.store(c, std::memory_order_relaxed);
        updateAlpha();
    }
    void setSupplyVoltage(double v) noexcept {
        double vv = (v < 10.0 ? 9.0 : 18.0);
        supplyV.store(vv, std::memory_order_relaxed);
    }

    void setHeadroomKnees(double knee9V, double knee18V) noexcept {
        headroomKnee9V.store(std::clamp(knee9V, 0.1, 1.0), std::memory_order_relaxed);
        headroomKnee18V.store(std::clamp(knee18V, 0.1, 1.0), std::memory_order_relaxed);
    }

    // Slew rate setting (V/s: JRC4558D=0.5e6, TL072=13e6)
    void setSlewRate(double vrPerSec) noexcept {
        slewRateVps.store(std::clamp(vrPerSec, 1e4, 1e8), std::memory_order_relaxed);
        rebuildSlewInstances(slewAmps_.size());
    }

    inline float process(int ch, float x) noexcept {
        if (padEnabled.load(std::memory_order_relaxed))
            x = static_cast<float>(x * 0.1f);

        const size_t nCh = rcState.size();
        if (nCh == 0) return applyHeadroom(ch, x);
        size_t idx = (ch >= 0 ? (size_t)ch : 0);
        if (idx >= nCh) idx = nCh - 1;

        // 1. RC input filter (1MΩ || C)
        const double a = alpha.load(std::memory_order_relaxed);
        double prev = rcState[idx];
        double y = a * static_cast<double>(x) + (1.0 - a) * prev;
        rcState[idx] = y;

        // 2. Input bias current noise (~30pA baseline × normalized by sqrt(BW))
        //    Actual hardware: TL072=30pA typ, JRC4558=50nA typ
        double biasNoise = normalDist(rng) * kBiasCurrentNoise;
        y += biasNoise;

        // 3. Input offset voltage drift (temperature random walk: ±150µV/°C)
        offsetDrift[idx] += normalDist(rng) * kOffsetDriftRate;
        offsetDrift[idx] *= kOffsetDriftDecay;  // Gradual center reversion
        y += offsetDrift[idx];

        return applyHeadroom(ch, static_cast<float>(y));
    }

    double getAlpha() const noexcept { return alpha.load(std::memory_order_relaxed); }

    void processBlock(float* const* io, int numChannels, int numSamples) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i]);
    }

    bool consumeOverloadFlag() const noexcept { return overloadFlag.exchange(false, std::memory_order_acq_rel); }

private:
    // === circuit constants ===
    static constexpr double kBiasCurrentNoise = 3e-8;    // 30pA × 1kΩ normalized ≈ 30nV
    static constexpr double kOffsetDriftRate = 1e-7;      // ±150µV/°C Temperature drift normalized
    static constexpr double kOffsetDriftDecay = 0.99999;   // Gradual decay of drift state
    static constexpr double kOverloadRecoveryRate = 0.002; // ~500samples to 95% recovery
    static constexpr double kAsymPosScale = 1.0;           // Positive-side saturation
    static constexpr double kAsymNegScale = 0.92;          // Negative side clips slightly earlier (PNP output stage asymmetry)

    void updateAlpha() noexcept {
        const double sr = sampleRate;
        double c = C_f.load(std::memory_order_relaxed);
        if (sr <= 1.0 || c <= 0.0) { alpha.store(1.0, std::memory_order_relaxed); return; }
        const double dt = 1.0 / sr;
        double a = 1.0 - std::exp(-dt / (R_ohm * c));
        if (!std::isfinite(a)) a = 1.0;
        a = std::clamp(a, 1e-9, 0.999999);
        alpha.store(a, std::memory_order_relaxed);
    }

    void rebuildSlewInstances(size_t nCh) noexcept {
        OpAmpPrimitive::Spec slewSpec;
        slewSpec.slewRate = slewRateVps.load(std::memory_order_relaxed);
        slewAmps_.resize(nCh);
        for (auto& amp : slewAmps_)
        {
            amp = OpAmpPrimitive(slewSpec);
            amp.prepare(sampleRate);
        }
    }

    inline float applyHeadroom(int ch, float x) noexcept {
        const double v = supplyV.load(std::memory_order_relaxed);
        const double k = (v <= PartsConstants::V_supplyMin
            ? headroomKnee9V.load(std::memory_order_relaxed)
            : headroomKnee18V.load(std::memory_order_relaxed));

        // Overload detection
        if (std::fabs((double)x) > (k * 0.95))
        {
            static std::atomic<int> dbgCounter{0};
            int c = dbgCounter.fetch_add(1, std::memory_order_relaxed) & 0x7FFFFFFF;
            if ((c & 127) == 0)
                ANALOG_DELAY_DBG("[InputBuffer] Overload detected ch? x=" << x << " knee=" << k << " threshold=" << (k*0.95));
            overloadFlag.store(true, std::memory_order_release);
        }

        const double invK = 1.0 / k;
        double normalized = static_cast<double>(x) * invK;

        // === Slew rate limiting — delegated to OpAmpPrimitive ===
        const size_t idx = (ch >= 0 && (size_t)ch < slewAmps_.size()) ? (size_t)ch : 0;
        if (idx < slewAmps_.size())
            normalized = slewAmps_[idx].applySlewLimit(normalized);

        // === Asymmetric soft saturation ===
        // Positive side: TL072 output stage NPN side — standard tanh
        // Negative side: PNP side — slightly earlier saturation (output stage asymmetry)
        double y;
        if (normalized >= 0.0)
            y = std::tanh(normalized * kAsymPosScale) * k;
        else
            y = std::tanh(normalized / kAsymNegScale) * k * kAsymNegScale;

        // === Overload recovery (smooth transition after clipping)===
        if (idx < overloadRecovery.size())
        {
            double absNorm = std::abs(normalized);
            if (absNorm > 0.9)
            {
                // Entered clipping region — increase recovery coefficient
                overloadRecovery[idx] = std::min(1.0, overloadRecovery[idx] + 0.05);
            }
            if (overloadRecovery[idx] > 1e-6)
            {
                // During recovery: slightly attenuate output to reproduce hardware recovery transient response
                y *= (1.0 - overloadRecovery[idx] * 0.03);
                overloadRecovery[idx] *= (1.0 - kOverloadRecoveryRate);
            }
        }

        return static_cast<float>(y);
    }

    double sampleRate;
    const double R_ohm;
    std::vector<double> rcState;
    std::vector<OpAmpPrimitive> slewAmps_;          // Per-channel slew rate limiting
    std::vector<double> overloadRecovery;   // Overload recovery state
    std::vector<double> offsetDrift;        // Offset drift state
    std::atomic<double> C_f;
    std::atomic<double> alpha{1.0};
    std::atomic<double> supplyV;
    std::atomic<double> slewRateVps{PartsConstants::opAmp_slewRate};
    std::atomic<bool> padEnabled;
    mutable std::atomic<bool> overloadFlag{false};
    std::atomic<double> headroomKnee9V{PartsConstants::opAmp_headroomKnee9V};
    std::atomic<double> headroomKnee18V{PartsConstants::opAmp_headroomKnee18V};

    // Noise generator (for bias current and drift)
    mutable std::minstd_rand rng{42};
    mutable std::normal_distribution<double> normalDist{0.0, 1.0};
};
