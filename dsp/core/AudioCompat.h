/*
  ==============================================================================

    AudioCompat.h
    Created: 27 Mar 2026
    Author:  Patina Team

    Standalone C++ compatibility layer
    - Standard C++17 only (zero external dependencies)
    - Provides general-purpose types such as AudioBuffer, IIRFilter, SIMDRegister
    - Equivalent functionality available without JUCE dependency
    
  ==============================================================================
*/

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>
#include <string>
#include <cstdint>
#include <type_traits>

// Minimal JUCE compatibility shim for standalone build.
// Provides a small subset used by the DSP code (AudioBuffer, basic DSP helpers).

namespace patina {
namespace compat {

template<typename... Args>
inline void ignoreUnused(Args&&...) noexcept {}

template<typename T>
struct MathConstants {
    static constexpr T pi = T(3.14159265358979323846);
    static constexpr T twoPi = T(6.28318530717958647692);
    static constexpr T halfPi = T(1.57079632679489661923);
};

template<typename FloatType>
class AudioBuffer {
public:
    using SampleType = FloatType;
    AudioBuffer() noexcept = default;
    AudioBuffer(int nc, int ns) { setSize(nc, ns); }
    void setSize(int nc, int ns) noexcept {
        if (nc <= 0 || ns <= 0) { channels.clear(); numChannels = numSamples = 0; return; }
        channels.assign(static_cast<size_t>(nc), std::vector<SampleType>(static_cast<size_t>(ns), SampleType(0)));
        numChannels = nc; numSamples = ns;
    }
    void clear() noexcept { for (auto &c : channels) std::fill(c.begin(), c.end(), SampleType(0)); }
    int getNumSamples() const noexcept { return numSamples; }
    int getNumChannels() const noexcept { return numChannels; }
    SampleType* getWritePointer(int ch) noexcept { return (ch>=0 && ch<numChannels) ? channels[(size_t)ch].data() : nullptr; }
    const SampleType* getReadPointer(int ch) const noexcept { return (ch>=0 && ch<numChannels) ? channels[(size_t)ch].data() : nullptr; }
    inline SampleType getSample(int ch, int idx) const noexcept { return (ch>=0 && ch<numChannels && idx>=0 && idx<numSamples) ? channels[(size_t)ch][(size_t)idx] : SampleType(0); }
    inline void setSample(int ch, int idx, SampleType v) noexcept { if (ch>=0 && ch<numChannels && idx>=0 && idx<numSamples) channels[(size_t)ch][(size_t)idx]=v; }
private:
    std::vector<std::vector<SampleType>> channels;
    int numChannels = 0;
    int numSamples = 0;
};

template<typename FloatType>
class SIMDRegister {
public:
    using T = FloatType;
    SIMDRegister() noexcept { v.fill(T(0)); }
    explicit SIMDRegister(T s) noexcept { v.fill(s); }
    static SIMDRegister fromRawArray(const T* arr) noexcept { SIMDRegister r; for (size_t i = 0; i < r.v.size(); ++i) r.v[i] = arr[i]; return r; }
    void copyToRawArray(T* out) const noexcept { for (size_t i = 0; i < v.size(); ++i) out[i] = v[i]; }

    SIMDRegister operator+(const SIMDRegister& o) const noexcept { SIMDRegister r; for (size_t i = 0; i < v.size(); ++i) r.v[i] = v[i] + o.v[i]; return r; }
    SIMDRegister operator-(const SIMDRegister& o) const noexcept { SIMDRegister r; for (size_t i = 0; i < v.size(); ++i) r.v[i] = v[i] - o.v[i]; return r; }
    SIMDRegister operator*(const SIMDRegister& o) const noexcept { SIMDRegister r; for (size_t i = 0; i < v.size(); ++i) r.v[i] = v[i] * o.v[i]; return r; }
    SIMDRegister operator*(T s) const noexcept { SIMDRegister r; for (size_t i = 0; i < v.size(); ++i) r.v[i] = v[i] * s; return r; }
    SIMDRegister operator+(T s) const noexcept { SIMDRegister r; for (size_t i = 0; i < v.size(); ++i) r.v[i] = v[i] + s; return r; }

    SIMDRegister& operator+=(const SIMDRegister& o) noexcept { for (size_t i = 0; i < v.size(); ++i) v[i] += o.v[i]; return *this; }
    SIMDRegister& operator*=(const SIMDRegister& o) noexcept { for (size_t i = 0; i < v.size(); ++i) v[i] *= o.v[i]; return *this; }

    std::array<T,4> v;
};

namespace dsp {
    template<typename FloatType>
    struct IIRCoefficients {
        struct Spec { FloatType b0,b1,b2,a1,a2; };
        static inline Spec makeLowPass(double sampleRate, double cutoffHz) noexcept {
            const double omega = 2.0 * MathConstants<double>::pi * cutoffHz / sampleRate;
            const double sin_omega = std::sin(omega);
            const double cos_omega = std::cos(omega);
            const double alpha = sin_omega / 2.0;
            const double norm = 1.0 + alpha;
            Spec s;
            s.b0 = (FloatType)((1.0 - cos_omega) / (2.0 * norm));
            s.b1 = (FloatType)((1.0 - cos_omega) / norm);
            s.b2 = (FloatType)((1.0 - cos_omega) / (2.0 * norm));
            s.a1 = (FloatType)((-2.0 * cos_omega) / norm);
            s.a2 = (FloatType)((1.0 - alpha) / norm);
            return s;
        }
        static inline Spec makeHighPass(double sampleRate, double cutoffHz) noexcept {
            const double omega = 2.0 * MathConstants<double>::pi * cutoffHz / sampleRate;
            const double sin_omega = std::sin(omega);
            const double cos_omega = std::cos(omega);
            const double alpha = sin_omega / 2.0;
            const double norm = 1.0 + alpha;
            Spec s;
            s.b0 = (FloatType)((1.0 + cos_omega) / (2.0 * norm));
            s.b1 = (FloatType)(-(1.0 + cos_omega) / norm);
            s.b2 = (FloatType)((1.0 + cos_omega) / (2.0 * norm));
            s.a1 = (FloatType)((-2.0 * cos_omega) / norm);
            s.a2 = (FloatType)((1.0 - alpha) / norm);
            return s;
        }
    };

    template<typename FloatType>
    class IIRFilter {
    public:
        using CoefSpec = typename IIRCoefficients<FloatType>::Spec;
        IIRFilter() noexcept = default;
        void setCoefficients(const CoefSpec& c) noexcept { coeffs = c; }
        void reset() noexcept { x1=x2=y1=y2=FloatType(0); }
        inline FloatType processSample(FloatType x) noexcept {
            FloatType y = coeffs.b0 * x + coeffs.b1 * x1 + coeffs.b2 * x2 - coeffs.a1 * y1 - coeffs.a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y; return y;
        }
    private:
        CoefSpec coeffs {FloatType(0),FloatType(0),FloatType(0),FloatType(0),FloatType(0)};
        FloatType x1{}, x2{}, y1{}, y2{};
    };
} // namespace dsp

class String { public: String()=default; explicit String(const char* s):data(s?s:""){} const char* toRawUTF8() const noexcept { return data.c_str(); } std::string toStdString() const noexcept { return data; } String operator+(const String& o) const noexcept { return String((data+o.data).c_str()); } private: std::string data; };

} // namespace compat
} // namespace patina

