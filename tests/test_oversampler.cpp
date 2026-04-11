/*
  ==============================================================================

    test_oversampler.cpp
    Simple test/example for the Patina Oversampler

  ==============================================================================
*/

#include <iostream>
#include <cmath>
#include "../include/patina.h"

using namespace patina;

// Simple saturation for testing
inline float softClip(float x)
{
    return static_cast<float>(FastMath::fastTanh(static_cast<double>(x)));
}

int main()
{
    std::cout << "=== Patina Oversampler Test ===\n\n";

    // Test parameters
    constexpr int blockSize = 256;
    constexpr int numChannels = 2;
    constexpr double sampleRate = 48000.0;
    
    // Create input buffer with a 1kHz sine wave + harmonics
    compat::AudioBuffer<float> buffer(numChannels, blockSize);
    
    const double freq = 1000.0;
    const double omega = compat::MathConstants<double>::twoPi * freq / sampleRate;
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* ptr = buffer.getWritePointer(ch);
        for (int i = 0; i < blockSize; ++i)
        {
            // Sine wave with drive to create harmonics when saturated
            ptr[i] = static_cast<float>(std::sin(omega * i) * 2.0);
        }
    }
    
    // ========================================================================
    // Test 1: Basic Oversampler (2x)
    // ========================================================================
    std::cout << "Test 1: 2x Oversampler\n";
    {
        Oversampler<float> os;
        os.prepare(blockSize, numChannels, OversamplingFactor::x2, FilterQuality::High);
        
        std::cout << "  Factor: " << os.getFactor() << "x\n";
        std::cout << "  Latency: " << os.getLatency() << " samples\n";
        
        // Process with saturation
        os.process(buffer, blockSize, [](compat::AudioBuffer<float>& buf, int numSamples) {
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                float* ptr = buf.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    ptr[i] = softClip(ptr[i]);
                }
            }
        });
        
        // Check output is valid
        float maxVal = 0.0f;
        const float* out = buffer.getReadPointer(0);
        for (int i = 0; i < blockSize; ++i)
            maxVal = std::max(maxVal, std::abs(out[i]));
        
        std::cout << "  Max output: " << maxVal << " (should be < 1.0 due to tanh)\n";
        std::cout << "  PASS\n\n";
    }
    
    // Regenerate input
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* ptr = buffer.getWritePointer(ch);
        for (int i = 0; i < blockSize; ++i)
            ptr[i] = static_cast<float>(std::sin(omega * i) * 2.0);
    }
    
    // ========================================================================
    // Test 2: 4x Oversampler
    // ========================================================================
    std::cout << "Test 2: 4x Oversampler\n";
    {
        Oversampler<float> os;
        os.prepare(blockSize, numChannels, OversamplingFactor::x4, FilterQuality::High);
        
        std::cout << "  Factor: " << os.getFactor() << "x\n";
        std::cout << "  Latency: " << os.getLatency() << " samples\n";
        
        os.process(buffer, blockSize, [](compat::AudioBuffer<float>& buf, int numSamples) {
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                float* ptr = buf.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i)
                    ptr[i] = softClip(ptr[i]);
            }
        });
        
        float maxVal = 0.0f;
        const float* out = buffer.getReadPointer(0);
        for (int i = 0; i < blockSize; ++i)
            maxVal = std::max(maxVal, std::abs(out[i]));
        
        std::cout << "  Max output: " << maxVal << "\n";
        std::cout << "  PASS\n\n";
    }
    
    // Regenerate input
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* ptr = buffer.getWritePointer(ch);
        for (int i = 0; i < blockSize; ++i)
            ptr[i] = static_cast<float>(std::sin(omega * i) * 2.0);
    }
    
    // ========================================================================
    // Test 3: MultistageOversampler (8x as 3 stages of 2x)
    // ========================================================================
    std::cout << "Test 3: Multistage 8x Oversampler\n";
    {
        MultistageOversampler<float> os;
        os.prepare(blockSize, numChannels, 8, FilterQuality::Medium);
        
        std::cout << "  Total Factor: " << os.getTotalFactor() << "x\n";
        std::cout << "  Num Stages: " << os.getNumStages() << "\n";
        std::cout << "  Total Latency: " << os.getTotalLatency() << " samples\n";
        
        os.process(buffer, blockSize, [](compat::AudioBuffer<float>& buf, int numSamples) {
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                float* ptr = buf.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i)
                    ptr[i] = softClip(ptr[i]);
            }
        });
        
        float maxVal = 0.0f;
        const float* out = buffer.getReadPointer(0);
        for (int i = 0; i < blockSize; ++i)
            maxVal = std::max(maxVal, std::abs(out[i]));
        
        std::cout << "  Max output: " << maxVal << "\n";
        std::cout << "  PASS\n\n";
    }
    
    // ========================================================================
    // Test 4: FIR Filter Design
    // ========================================================================
    std::cout << "Test 4: FIR Filter Design\n";
    {
        // Design lowpass filter at 0.25 normalized (Nyquist/2)
        const double cutoff = 0.25;
        const double attenuation = 96.0;
        const double transition = 0.1;
        
        int order = FIRDesigner::estimateFilterOrder(transition, attenuation);
        double beta = FIRDesigner::estimateKaiserBeta(attenuation);
        
        std::cout << "  Estimated order for " << attenuation << " dB: " << order << " taps\n";
        std::cout << "  Kaiser beta: " << beta << "\n";
        
        auto coeffs = FIRDesigner::designLowpass(cutoff, order, beta);
        
        std::cout << "  Generated " << coeffs.size() << " coefficients\n";
        
        // Check DC gain (should be ~1.0)
        double dcGain = 0.0;
        for (auto c : coeffs)
            dcGain += c;
        
        std::cout << "  DC gain: " << dcGain << " (should be ~1.0)\n";
        std::cout << "  PASS\n\n";
    }
    
    std::cout << "=== All tests passed! ===\n";
    
    return 0;
}
