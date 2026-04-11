/*
  ==============================================================================

    Oversampler.h
    Created: April 2026
    Author:  Patina Team

    High-quality polyphase oversampler with anti-aliasing filters
    - Supports 2x, 4x, 8x, 16x oversampling
    - Kaiser-windowed sinc FIR filter design
    - Zero JUCE dependency (pure C++17)
    - Efficient polyphase implementation
    
  ==============================================================================
*/

#pragma once

#include <array>
#include <cmath>
#include <vector>
#include <algorithm>
#include <functional>
#include <cassert>
#include "AudioCompat.h"

namespace patina {

// ============================================================================
// Kaiser Window Functions
// ============================================================================

namespace detail {

// Modified Bessel function of the first kind, order 0
// Used for Kaiser window computation
inline double besselI0(double x) noexcept
{
    // Polynomial approximation
    const double ax = std::abs(x);
    
    if (ax < 3.75)
    {
        const double y = (x / 3.75) * (x / 3.75);
        return 1.0 + y * (3.5156229 
                       + y * (3.0899424 
                            + y * (1.2067492 
                                 + y * (0.2659732 
                                      + y * (0.0360768 
                                           + y * 0.0045813)))));
    }
    else
    {
        const double y = 3.75 / ax;
        return (std::exp(ax) / std::sqrt(ax)) 
             * (0.39894228 
              + y * (0.01328592 
                   + y * (0.00225319 
                        + y * (-0.00157565 
                             + y * (0.00916281 
                                  + y * (-0.02057706 
                                       + y * (0.02635537 
                                            + y * (-0.01647633 
                                                 + y * 0.00392377))))))));
    }
}

// Kaiser window coefficient
inline double kaiserWindow(int n, int N, double beta) noexcept
{
    const double alpha = 0.5 * (N - 1);
    const double ratio = (n - alpha) / alpha;
    const double arg = beta * std::sqrt(1.0 - ratio * ratio);
    return besselI0(arg) / besselI0(beta);
}

// Normalized sinc function: sin(pi*x) / (pi*x)
inline double sinc(double x) noexcept
{
    if (std::abs(x) < 1e-10)
        return 1.0;
    
    const double pi_x = compat::MathConstants<double>::pi * x;
    return std::sin(pi_x) / pi_x;
}

} // namespace detail

// ============================================================================
// FIR Filter Coefficient Generator
// ============================================================================

class FIRDesigner
{
public:
    // Design lowpass FIR filter using Kaiser window
    // cutoffNormalized: normalized cutoff frequency (0 to 0.5, where 0.5 = Nyquist)
    // numTaps: filter order (odd recommended for linear phase)
    // beta: Kaiser window shape parameter (larger = steeper rolloff, more ripple)
    //       β ≈ 5-6 for audio anti-aliasing, β ≈ 8-10 for high stopband attenuation
    static std::vector<double> designLowpass(double cutoffNormalized, 
                                              int numTaps, 
                                              double beta = 7.0) noexcept
    {
        std::vector<double> coeffs(static_cast<size_t>(numTaps));
        const int M = numTaps - 1;
        const double alpha = 0.5 * M;
        
        double sum = 0.0;
        
        for (int n = 0; n < numTaps; ++n)
        {
            // Ideal lowpass impulse response (sinc)
            const double sincVal = detail::sinc(2.0 * cutoffNormalized * (n - alpha));
            
            // Apply Kaiser window
            const double window = detail::kaiserWindow(n, numTaps, beta);
            
            coeffs[static_cast<size_t>(n)] = sincVal * window;
            sum += coeffs[static_cast<size_t>(n)];
        }
        
        // Normalize for unity DC gain
        if (sum != 0.0)
        {
            for (auto& c : coeffs)
                c /= sum;
        }
        
        return coeffs;
    }
    
    // Calculate recommended filter order based on transition bandwidth
    // transitionWidth: normalized transition bandwidth (e.g., 0.1 means 10% of Nyquist)
    // attenuation: desired stopband attenuation in dB (e.g., 96 dB)
    static int estimateFilterOrder(double transitionWidth, double attenuationDb) noexcept
    {
        // Kaiser formula for filter order
        int order = static_cast<int>(std::ceil((attenuationDb - 8.0) / (2.285 * compat::MathConstants<double>::twoPi * transitionWidth)));
        
        // Ensure odd number for symmetric linear phase
        if (order % 2 == 0)
            order++;
        
        // Minimum sensible order
        return std::max(order, 15);
    }
    
    // Calculate Kaiser beta from desired attenuation
    static double estimateKaiserBeta(double attenuationDb) noexcept
    {
        if (attenuationDb > 50.0)
            return 0.1102 * (attenuationDb - 8.7);
        else if (attenuationDb >= 21.0)
            return 0.5842 * std::pow(attenuationDb - 21.0, 0.4) + 0.07886 * (attenuationDb - 21.0);
        else
            return 0.0;
    }
};

// ============================================================================
// Polyphase FIR Filter for Efficient Resampling (Upsample)
// ============================================================================

template<typename FloatType = float>
class PolyphaseFilter
{
public:
    PolyphaseFilter() = default;
    
    // Initialize with FIR coefficients and oversampling factor
    void initialize(const std::vector<double>& coefficients, int oversampleFactor) noexcept
    {
        factor_ = oversampleFactor;
        
        // Pad coefficients to be divisible by factor
        size_t paddedSize = coefficients.size();
        while (paddedSize % static_cast<size_t>(factor_) != 0)
            paddedSize++;
        
        // Calculate number of taps per phase
        tapsPerPhase_ = static_cast<int>(paddedSize / static_cast<size_t>(factor_));
        
        // Decompose into polyphase components
        phases_.resize(static_cast<size_t>(factor_));
        for (int p = 0; p < factor_; ++p)
        {
            phases_[static_cast<size_t>(p)].resize(static_cast<size_t>(tapsPerPhase_), FloatType(0));
            
            for (int t = 0; t < tapsPerPhase_; ++t)
            {
                const size_t idx = static_cast<size_t>(p + t * factor_);
                if (idx < coefficients.size())
                    phases_[static_cast<size_t>(p)][static_cast<size_t>(t)] = static_cast<FloatType>(coefficients[idx]);
            }
        }
        
        // Initialize delay line
        delayLine_.resize(static_cast<size_t>(tapsPerPhase_), FloatType(0));
        delayIndex_ = 0;
    }
    
    void reset() noexcept
    {
        std::fill(delayLine_.begin(), delayLine_.end(), FloatType(0));
        delayIndex_ = 0;
    }
    
    // Push a new sample into the delay line
    void push(FloatType sample) noexcept
    {
        delayLine_[static_cast<size_t>(delayIndex_)] = sample;
        if (--delayIndex_ < 0)
            delayIndex_ = tapsPerPhase_ - 1;
    }
    
    // Compute output for a specific polyphase
    FloatType computePhase(int phase) const noexcept
    {
        const auto& phaseCoeffs = phases_[static_cast<size_t>(phase)];
        FloatType sum = FloatType(0);
        
        int idx = delayIndex_;
        for (int t = 0; t < tapsPerPhase_; ++t)
        {
            sum += delayLine_[static_cast<size_t>(idx)] * phaseCoeffs[static_cast<size_t>(t)];
            if (++idx >= tapsPerPhase_)
                idx = 0;
        }
        
        return sum;
    }
    
    int getFactor() const noexcept { return factor_; }
    int getTapsPerPhase() const noexcept { return tapsPerPhase_; }
    
private:
    int factor_ = 1;
    int tapsPerPhase_ = 0;
    std::vector<std::vector<FloatType>> phases_;
    std::vector<FloatType> delayLine_;
    int delayIndex_ = 0;
};

// ============================================================================
// Direct-form FIR Decimator (for downsampling)
// Accepts every input sample, outputs 1 sample per `factor` inputs.
// ============================================================================

template<typename FloatType = float>
class FIRDecimator
{
public:
    FIRDecimator() = default;

    void initialize(const std::vector<double>& coefficients, int decimateFactor) noexcept
    {
        factor_   = decimateFactor;
        numTaps_  = static_cast<int>(coefficients.size());
        coeffs_.resize(static_cast<size_t>(numTaps_));
        for (int i = 0; i < numTaps_; ++i)
            coeffs_[static_cast<size_t>(i)] = static_cast<FloatType>(coefficients[static_cast<size_t>(i)]);
        delayLine_.assign(static_cast<size_t>(numTaps_), FloatType(0));
        delayIndex_ = 0;
        inputCount_ = 0;
    }

    void reset() noexcept
    {
        std::fill(delayLine_.begin(), delayLine_.end(), FloatType(0));
        delayIndex_ = 0;
        inputCount_ = 0;
    }

    // Push one sample. Returns true when an output sample is ready (every `factor` inputs).
    bool push(FloatType sample) noexcept
    {
        delayLine_[static_cast<size_t>(delayIndex_)] = sample;
        if (++delayIndex_ >= numTaps_)
            delayIndex_ = 0;
        ++inputCount_;
        return (inputCount_ % factor_) == 0;
    }

    // Call after push() returns true to get the decimated output.
    FloatType compute() const noexcept
    {
        FloatType sum = FloatType(0);
        int idx = delayIndex_;
        for (int t = 0; t < numTaps_; ++t)
        {
            if (--idx < 0) idx = numTaps_ - 1;
            sum += delayLine_[static_cast<size_t>(idx)] * coeffs_[static_cast<size_t>(t)];
        }
        return sum;
    }

    int getFactor()  const noexcept { return factor_; }
    int getNumTaps() const noexcept { return numTaps_; }

private:
    int factor_   = 1;
    int numTaps_  = 0;
    int delayIndex_ = 0;
    int inputCount_ = 0;
    std::vector<FloatType> coeffs_;
    std::vector<FloatType> delayLine_;
};

// ============================================================================
// Main Oversampler Class
// ============================================================================

enum class OversamplingFactor
{
    None = 1,
    x2   = 2,
    x4   = 4,
    x8   = 8,
    x16  = 16
};

enum class FilterQuality
{
    Low,      // ~48 dB stopband attenuation, lower latency
    Medium,   // ~72 dB stopband attenuation
    High,     // ~96 dB stopband attenuation
    Ultra     // ~120 dB stopband attenuation, highest quality
};

template<typename FloatType = float>
class Oversampler
{
public:
    Oversampler() = default;
    
    // Initialize oversampler
    // maxBlockSize: maximum number of input samples per process call
    // numChannels: number of audio channels
    // factor: oversampling factor (2x, 4x, etc.)
    // quality: filter quality (affects latency and CPU)
    void prepare(int maxBlockSize, 
                 int numChannels, 
                 OversamplingFactor factor = OversamplingFactor::x2,
                 FilterQuality quality = FilterQuality::High) noexcept
    {
        factor_ = static_cast<int>(factor);
        numChannels_ = numChannels;
        maxBlockSize_ = maxBlockSize;
        
        if (factor_ <= 1)
        {
            factor_ = 1;
            latency_ = 0;
            return;
        }
        
        // Design anti-aliasing filter
        // Cutoff at 0.5/factor with some transition band
        const double cutoffNormalized = 0.45 / factor_;  // Slightly below Nyquist/factor
        const double transitionWidth = 0.05 / factor_;
        
        double attenuationDb;
        switch (quality)
        {
            case FilterQuality::Low:    attenuationDb = 48.0;  break;
            case FilterQuality::Medium: attenuationDb = 72.0;  break;
            case FilterQuality::High:   attenuationDb = 96.0;  break;
            case FilterQuality::Ultra:  attenuationDb = 120.0; break;
            default:                    attenuationDb = 96.0;  break;
        }
        
        const int filterOrder = FIRDesigner::estimateFilterOrder(transitionWidth, attenuationDb);
        const double beta = FIRDesigner::estimateKaiserBeta(attenuationDb);
        
        filterCoeffs_ = FIRDesigner::designLowpass(cutoffNormalized, filterOrder, beta);
        
        // Scale coefficients for upsampling (compensate for zero-stuffing)
        std::vector<double> scaledCoeffs = filterCoeffs_;
        for (auto& c : scaledCoeffs)
            c *= factor_;
        
        // Initialize polyphase filters for each channel
        upsampleFilters_.resize(static_cast<size_t>(numChannels));
        downsampleFilters_.resize(static_cast<size_t>(numChannels));
        
        for (int ch = 0; ch < numChannels; ++ch)
        {
            upsampleFilters_[static_cast<size_t>(ch)].initialize(scaledCoeffs, factor_);
            downsampleFilters_[static_cast<size_t>(ch)].initialize(filterCoeffs_, factor_);
        }
        
        // Calculate latency (group delay)
        // For linear phase FIR: (N-1)/2 samples at the higher rate, then /factor for base rate
        latency_ = (filterOrder - 1) / 2 / factor_;
        
        // Allocate oversampled buffer
        oversampledBuffer_.setSize(numChannels, maxBlockSize * factor_);
    }
    
    void reset() noexcept
    {
        for (auto& f : upsampleFilters_)
            f.reset();
        for (auto& f : downsampleFilters_)
            f.reset();
        
        oversampledBuffer_.clear();
    }
    
    // Get pointer to oversampled buffer for processing
    compat::AudioBuffer<FloatType>& getOversampledBuffer() noexcept
    {
        return oversampledBuffer_;
    }
    
    // Get oversampling factor
    int getFactor() const noexcept { return factor_; }
    
    // Get latency in base-rate samples
    int getLatency() const noexcept { return latency_; }
    
    // Upsample input block
    // After calling this, process the oversampled data in getOversampledBuffer()
    // Returns the number of oversampled samples
    int upsample(const compat::AudioBuffer<FloatType>& input, int numSamples) noexcept
    {
        if (factor_ <= 1)
        {
            // No oversampling - just copy
            for (int ch = 0; ch < numChannels_; ++ch)
            {
                const FloatType* src = input.getReadPointer(ch);
                FloatType* dst = oversampledBuffer_.getWritePointer(ch);
                if (src && dst)
                {
                    for (int i = 0; i < numSamples; ++i)
                        dst[i] = src[i];
                }
            }
            return numSamples;
        }
        
        const int oversampledLength = numSamples * factor_;
        
        for (int ch = 0; ch < numChannels_; ++ch)
        {
            const FloatType* src = input.getReadPointer(ch);
            FloatType* dst = oversampledBuffer_.getWritePointer(ch);
            
            if (!src || !dst)
                continue;
            
            auto& filter = upsampleFilters_[static_cast<size_t>(ch)];
            
            int outIdx = 0;
            for (int i = 0; i < numSamples; ++i)
            {
                // Push input sample to polyphase filter
                filter.push(src[i]);
                
                // Generate factor_ output samples per input sample
                for (int p = 0; p < factor_; ++p)
                {
                    dst[outIdx++] = filter.computePhase(p);
                }
            }
        }
        
        return oversampledLength;
    }
    
    // Downsample oversampled buffer back to original rate
    void downsample(compat::AudioBuffer<FloatType>& output, int numOutputSamples) noexcept
    {
        if (factor_ <= 1)
        {
            // No oversampling - just copy
            for (int ch = 0; ch < numChannels_; ++ch)
            {
                const FloatType* src = oversampledBuffer_.getReadPointer(ch);
                FloatType* dst = output.getWritePointer(ch);
                if (src && dst)
                {
                    for (int i = 0; i < numOutputSamples; ++i)
                        dst[i] = src[i];
                }
            }
            return;
        }
        
        for (int ch = 0; ch < numChannels_; ++ch)
        {
            const FloatType* src = oversampledBuffer_.getReadPointer(ch);
            FloatType* dst = output.getWritePointer(ch);
            
            if (!src || !dst)
                continue;
            
            auto& decimator = downsampleFilters_[static_cast<size_t>(ch)];
            int outIdx = 0;
            const int totalIn = numOutputSamples * factor_;
            for (int i = 0; i < totalIn && outIdx < numOutputSamples; ++i)
            {
                if (decimator.push(src[i]))
                    dst[outIdx++] = decimator.compute();
            }
        }
    }
    
    // Convenience method: process with a lambda/callback
    // The callback receives the oversampled buffer and oversampled sample count
    template<typename ProcessFunc>
    void process(compat::AudioBuffer<FloatType>& buffer, 
                 int numSamples, 
                 ProcessFunc&& processFunc) noexcept
    {
        if (factor_ <= 1)
        {
            // No oversampling - process directly
            processFunc(buffer, numSamples);
            return;
        }
        
        // Upsample
        const int oversampledSamples = upsample(buffer, numSamples);
        
        // Process at oversampled rate
        processFunc(oversampledBuffer_, oversampledSamples);
        
        // Downsample
        downsample(buffer, numSamples);
    }
    
private:
    int factor_ = 1;
    int numChannels_ = 0;
    int maxBlockSize_ = 0;
    int latency_ = 0;
    
    std::vector<double> filterCoeffs_;
    std::vector<PolyphaseFilter<FloatType>> upsampleFilters_;
    std::vector<FIRDecimator<FloatType>> downsampleFilters_;
    
    compat::AudioBuffer<FloatType> oversampledBuffer_;
};

// ============================================================================
// Multi-stage Oversampler (for very high ratios like 16x or 32x)
// More efficient than single-stage for high ratios
// ============================================================================

template<typename FloatType = float>
class MultistageOversampler
{
public:
    MultistageOversampler() = default;
    
    // Initialize with total oversampling factor
    // Will automatically split into 2x stages
    void prepare(int maxBlockSize, 
                 int numChannels, 
                 int totalFactor,
                 FilterQuality quality = FilterQuality::High) noexcept
    {
        numChannels_ = numChannels;
        maxBlockSize_ = maxBlockSize;
        totalFactor_ = totalFactor;
        
        // Determine number of 2x stages needed
        int remaining = totalFactor;
        stages_.clear();
        stageBuffers_.clear();
        
        int currentBlockSize = maxBlockSize;
        
        while (remaining > 1)
        {
            stages_.emplace_back();
            stages_.back().prepare(currentBlockSize, numChannels, OversamplingFactor::x2, quality);
            
            currentBlockSize *= 2;
            remaining /= 2;
        }
        
        // Allocate intermediate buffers
        currentBlockSize = maxBlockSize;
        for (size_t i = 0; i < stages_.size(); ++i)
        {
            currentBlockSize *= 2;
            stageBuffers_.emplace_back();
            stageBuffers_.back().setSize(numChannels, currentBlockSize);
        }
        
        // Calculate total latency
        totalLatency_ = 0;
        int latencyMultiplier = 1;
        for (auto& stage : stages_)
        {
            totalLatency_ += stage.getLatency() * latencyMultiplier;
            latencyMultiplier *= 2;
        }
    }
    
    void reset() noexcept
    {
        for (auto& stage : stages_)
            stage.reset();
        for (auto& buf : stageBuffers_)
            buf.clear();
    }
    
    int getTotalFactor() const noexcept { return totalFactor_; }
    int getTotalLatency() const noexcept { return totalLatency_; }
    int getNumStages() const noexcept { return static_cast<int>(stages_.size()); }
    
    // Get final oversampled buffer
    compat::AudioBuffer<FloatType>& getOversampledBuffer() noexcept
    {
        if (stageBuffers_.empty())
        {
            static compat::AudioBuffer<FloatType> dummy;
            return dummy;
        }
        return stageBuffers_.back();
    }
    
    // Upsample through all stages
    int upsample(const compat::AudioBuffer<FloatType>& input, int numSamples) noexcept
    {
        if (stages_.empty())
            return numSamples;
        
        // First stage uses input
        int currentSamples = stages_[0].upsample(input, numSamples);
        
        // Copy to first stage buffer
        copyBuffer(stages_[0].getOversampledBuffer(), stageBuffers_[0], currentSamples);
        
        // Subsequent stages
        for (size_t i = 1; i < stages_.size(); ++i)
        {
            currentSamples = stages_[i].upsample(stageBuffers_[i - 1], currentSamples);
            copyBuffer(stages_[i].getOversampledBuffer(), stageBuffers_[i], currentSamples);
        }
        
        return currentSamples;
    }
    
    // Downsample through all stages (reverse order)
    void downsample(compat::AudioBuffer<FloatType>& output, int numOutputSamples) noexcept
    {
        if (stages_.empty())
            return;
        
        int currentSamples = numOutputSamples * totalFactor_;
        
        // Process stages in reverse
        for (int i = static_cast<int>(stages_.size()) - 1; i >= 0; --i)
        {
            currentSamples /= 2;
            
            if (i > 0)
            {
                stages_[static_cast<size_t>(i)].downsample(stageBuffers_[static_cast<size_t>(i) - 1], currentSamples);
                // Copy the decimated result into the previous stage's internal buffer
                // so that stage (i-1) downsamples the *processed* signal, not the
                // original upsampled data that was stored there during upsample().
                copyBuffer(stageBuffers_[static_cast<size_t>(i) - 1],
                           stages_[static_cast<size_t>(i) - 1].getOversampledBuffer(),
                           currentSamples);
            }
            else
            {
                stages_[0].downsample(output, currentSamples);
            }
        }
    }
    
    // Process with callback
    template<typename ProcessFunc>
    void process(compat::AudioBuffer<FloatType>& buffer, 
                 int numSamples, 
                 ProcessFunc&& processFunc) noexcept
    {
        if (stages_.empty())
        {
            processFunc(buffer, numSamples);
            return;
        }
        
        const int oversampledSamples = upsample(buffer, numSamples);

        // processFunc writes into stageBuffers_.back() (the public oversampled buffer).
        // After processing we must copy the result back into stages_.back().oversampledBuffer_
        // so that downsample() — which reads from that internal buffer — sees the DSP output.
        processFunc(getOversampledBuffer(), oversampledSamples);

        // Write processed stageBuffer back into the last stage's internal oversampledBuffer_
        {
            auto& lastStage  = stages_.back();
            auto& stageBuf   = stageBuffers_.back();
            auto& internalBuf = lastStage.getOversampledBuffer();
            for (int ch = 0; ch < numChannels_; ++ch)
            {
                const FloatType* src = stageBuf.getReadPointer(ch);
                FloatType*       dst = internalBuf.getWritePointer(ch);
                if (src && dst)
                    std::copy(src, src + oversampledSamples, dst);
            }
        }

        downsample(buffer, numSamples);
    }
    
private:
    void copyBuffer(const compat::AudioBuffer<FloatType>& src,
                    compat::AudioBuffer<FloatType>& dst,
                    int numSamples) noexcept
    {
        for (int ch = 0; ch < numChannels_; ++ch)
        {
            const FloatType* srcPtr = src.getReadPointer(ch);
            FloatType* dstPtr = dst.getWritePointer(ch);
            if (srcPtr && dstPtr)
            {
                for (int i = 0; i < numSamples; ++i)
                    dstPtr[i] = srcPtr[i];
            }
        }
    }
    
    int numChannels_ = 0;
    int maxBlockSize_ = 0;
    int totalFactor_ = 1;
    int totalLatency_ = 0;
    
    std::vector<Oversampler<FloatType>> stages_;
    std::vector<compat::AudioBuffer<FloatType>> stageBuffers_;
};

} // namespace patina
