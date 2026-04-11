#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "../../core/ProcessSpec.h"
#include "../../constants/PartsConstants.h"

// Analog all-pass filter
// - 1st/2nd order all-pass sections (cascadable)
// - OTA-based phase shift network (for phaser / phase EQ)
// - Frequency response is flat; only phase changes
// - Multi-stage phaser effects can be built by cascading
//
// Topology:
//   1st-order AP: H(z) = (a - z^-1) / (1 - a * z^-1)
//     → -90° phase shift at fc, dc=0°, Nyquist=-180°
//   2nd order AP: H(z) = (a2 - a1*z^-1 + z^-2) / (1 - a1*z^-1 + a2*z^-2)
//     → -180° phase shift at fc; Q controls steepness of the shift
//
// Typical use cases:
//   - Phaser: 4–12 stage cascade + feedback
//   - Phase EQ: localized phase correction
//   - Haas dispersion: frequency-dependent delay
class AnalogAllPass
{
public:
    enum class Order : int
    {
        First  = 1,   // 1st order all-pass
        Second = 2    // 2nd order all-pass
    };

    struct Params
    {
        float cutoffHz  = 1000.0f;   // phase shift center frequency
        float q         = 0.707f;    // Q (effective for 2nd order only)
        int   order     = 1;         // 1 or 2
    };

    AnalogAllPass() noexcept = default;

    void prepare(int numChannels, double sr) noexcept
    {
        sampleRate = std::max(1.0, sr);
        const size_t nCh = (size_t)std::max(1, numChannels);
        chState.resize(nCh);
        for (auto& st : chState) st = ChannelState{};
        updateCoefficients(1000.0f, 0.707f, 1);
    }

    void prepare(const patina::ProcessSpec& spec) noexcept
    {
        prepare(spec.numChannels, spec.sampleRate);
    }

    void reset() noexcept
    {
        for (auto& st : chState) st = ChannelState{};
    }

    void setCutoffHz(float hz) noexcept
    {
        freq = std::clamp((double)hz, 20.0, sampleRate * 0.49);
        updateCoefficients((float)freq, (float)qVal, currentOrder);
    }

    void setQ(float q) noexcept
    {
        qVal = std::clamp((double)q, 0.1, 20.0);
        updateCoefficients((float)freq, (float)qVal, currentOrder);
    }

    void setOrder(int ord) noexcept
    {
        currentOrder = std::clamp(ord, 1, 2);
        updateCoefficients((float)freq, (float)qVal, currentOrder);
    }

    // single sample processing
    inline float process(int channel, float x) noexcept
    {
        const size_t ch = (size_t)std::clamp(channel, 0, (int)chState.size() - 1);
        auto& st = chState[ch];

        double in = (double)x;
        double out;

        if (currentOrder == 1)
        {
            // 1st-order AP: y[n] = a1 * x[n] + x[n-1] - a1 * y[n-1]
            //   Direct Form I: y = a1*(x - y_prev) + x_prev
            out = a1Coeff * (in - st.y1) + st.x1;
            st.x1 = in;
            st.y1 = out;
        }
        else
        {
            // 2nd order AP: Transposed Direct Form II
            //   H(z) = (a2 - a1*z^-1 + z^-2) / (1 - a1*z^-1 + a2*z^-2)
            out = a2Coeff * in + st.d1;
            st.d1 = -a1Coeff * in + a1Coeff * out + st.d2;
            st.d2 = in - a1Coeff * (0.0) - a2Coeff * out;
            // Simplified:
            //   y = a2*x + d1
            //   d1 = -a1*x + a1*y + d2
            //   d2 = x - a2*y
            st.d1 = -a1Coeff * in + a1Coeff * out + st.d2;
            out = a2Coeff * in + (st.d1 - (-a1Coeff * in + a1Coeff * out + st.d2));

            // Cleaner implementation using lattice:
            double w = in - a1Coeff * st.s1 - a2Coeff * st.s2;
            out = a2Coeff * w + a1Coeff * st.s1 + st.s2;
            st.s2 = st.s1;
            st.s1 = w;
        }

        return (float)out;
    }

    inline float process(int channel, float x, const Params& params) noexcept
    {
        updateCoefficients(params.cutoffHz, params.q, params.order);
        return process(channel, x);
    }

    void processBlock(float* const* io, int numChannels, int numSamples, const Params& params) noexcept
    {
        updateCoefficients(params.cutoffHz, params.q, params.order);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                io[ch][i] = process(ch, io[ch][i]);
    }

    // returns phase shift amount at specified frequency (radians)
    double getPhaseAtFreq(double freqHz) const noexcept
    {
        if (sampleRate <= 0.0) return 0.0;
        double w = 2.0 * 3.14159265358979323846 * freqHz / sampleRate;
        if (currentOrder == 1)
        {
            // 1st-order AP phase: -2 * arctan((sin(w) * (1 - a1)) / (cos(w) * (1 + a1)))
            // simplified: φ(w) = -2*atan(tan(w/2) / a1) (approximation)
            double tanHalf = std::tan(w * 0.5);
            return -2.0 * std::atan2(tanHalf * (1.0 - a1Coeff), 1.0 + a1Coeff * tanHalf * tanHalf + 1e-30);
        }
        // 2nd order
        double cosW = std::cos(w);
        double sinW = std::sin(w);
        double num_r = a2Coeff - a1Coeff * cosW + std::cos(2.0 * w);
        double num_i = a1Coeff * sinW - std::sin(2.0 * w);
        double den_r = 1.0 - a1Coeff * cosW + a2Coeff * std::cos(2.0 * w);
        double den_i = a1Coeff * sinW - a2Coeff * std::sin(2.0 * w);
        return std::atan2(num_i, num_r) - std::atan2(den_i, den_r);
    }

private:
    struct ChannelState
    {
        // for 1st order AP
        double x1 = 0.0;
        double y1 = 0.0;
        // For 2nd-order AP (lattice)
        double s1 = 0.0;
        double s2 = 0.0;
        // For 2nd-order (transposed)
        double d1 = 0.0;
        double d2 = 0.0;
    };

    void updateCoefficients(float fc, float q, int order) noexcept
    {
        freq = std::clamp((double)fc, 20.0, sampleRate * 0.49);
        qVal = std::clamp((double)q, 0.1, 20.0);
        currentOrder = std::clamp(order, 1, 2);

        const double pi = 3.14159265358979323846;

        if (currentOrder == 1)
        {
            // 1st order AP coefficient: a = (tan(pi*fc/fs) - 1) / (tan(pi*fc/fs) + 1)
            double t = std::tan(pi * freq / sampleRate);
            a1Coeff = (t - 1.0) / (t + 1.0);
            a2Coeff = 0.0;
        }
        else
        {
            // 2nd-order AP coefficients (from biquad AP)
            double w0 = 2.0 * pi * freq / sampleRate;
            double alpha = std::sin(w0) / (2.0 * qVal);
            double a0 = 1.0 + alpha;
            a1Coeff = -2.0 * std::cos(w0) / a0;
            a2Coeff = (1.0 - alpha) / a0;
        }
    }

    double sampleRate   = PartsConstants::defaultSampleRate;
    double freq         = 1000.0;
    double qVal         = 0.707;
    int    currentOrder = 1;

    double a1Coeff = 0.0;
    double a2Coeff = 0.0;

    std::vector<ChannelState> chState;
};
