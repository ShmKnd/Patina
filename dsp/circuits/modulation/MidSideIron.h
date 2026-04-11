#pragma once
#include <cmath>
#include <algorithm>
#include "../../parts/TransformerPrimitive.h"
#include "../../core/ProcessSpec.h"

// =============================================================================
//  MidSideIron — Passive Transformer M/S Matrix (The Iron Method)
//
//  パッシブ・トランス方式の Mid/Side エンコーダー/デコーダー。
//  2 つのトランスの巻線を利用して物理的に信号を合成・反転し、
//  トランス固有の磁気飽和・位相特性が「太くまとまる」質感を生む。
//
//  Encode:
//    Mid  = Xfmr( L + R )     — 同相合成をトランスに通過
//    Side = Xfmr( L - R )     — 逆相合成をトランスに通過
//
//  Decode:
//    L = Xfmr( Mid + Side )
//    R = Xfmr( Mid - Side )
//
//  4-layer architecture:
//    Parts: TransformerPrimitive × 4
//      xfmrEnc[0] = encode Mid path
//      xfmrEnc[1] = encode Side path
//      xfmrDec[0] = decode L path
//      xfmrDec[1] = decode R path
//    → Circuit: MidSideIron (M/S matrix with transformer coloration)
//
//  Note: encode and decode use separate transformer instances so that
//  each stage accumulates its own magnetic state independently —
//  matching real hardware where four separate transformer windings are used.
//
//  Usage:
//    MidSideIron msEnc;
//    msEnc.prepare(spec);
//    auto [mid, side] = msEnc.encode(left, right);
//    // ... process mid/side independently ...
//    auto [outL, outR] = msEnc.decode(mid, side);
// =============================================================================
class MidSideIron
{
public:

    struct Params
    {
        float satAmount;
        float temperature;
        constexpr Params (float sat = 0.15f, float temp = 25.0f) noexcept
            : satAmount (sat), temperature (temp) {}
    };

    // =====================================================================
    //  Transformer preset for M/S matrix use
    //  Neve-style 1:1 with moderate core saturation for mid warmth
    // =====================================================================
    static constexpr TransformerPrimitive::Spec MidSideConsole()
    {
        return { 600.0, 2e-3, 200e-12, 1e-14, -0.001, 0.05, 0.10, 1e-6,
                 1.0, 10.0, 0.995, 50.0, 50.0, 150.0, 1.2, 80.0, 2.5, false };
    }

    MidSideIron() noexcept = default;

    explicit MidSideIron (const TransformerPrimitive::Spec& spec) noexcept
    {
        for (int i = 0; i < 2; ++i)
        {
            xfmrEnc[i] = TransformerPrimitive (spec);
            xfmrDec[i] = TransformerPrimitive (spec);
        }
    }

    void prepare (double sr) noexcept
    {
        for (int i = 0; i < 2; ++i)
        {
            xfmrEnc[i].prepare (sr);
            xfmrDec[i].prepare (sr);
        }
    }

    void prepare (const patina::ProcessSpec& spec) noexcept
    {
        prepare (spec.sampleRate);
    }

    void reset() noexcept
    {
        for (int i = 0; i < 2; ++i)
        {
            xfmrEnc[i].reset();
            xfmrDec[i].reset();
        }
    }

    // =====================================================================
    //  Encode: L/R → Mid/Side (through dedicated encode transformers)
    // =====================================================================
    struct MidSide { float mid; float side; };

    inline MidSide encode (float left, float right,
                           const Params& p = Params()) noexcept
    {
        // Sum/difference matrix (analog summation through transformer)
        const double sum  = (double)(left + right)  * 0.5;
        const double diff = (double)(left - right)  * 0.5;

        // Pass through encode transformers — adds saturation, LF warmth, HF character
        float mid  = (float) xfmrEnc[0].process (sum,  p.satAmount, p.temperature);
        float side = (float) xfmrEnc[1].process (diff, p.satAmount, p.temperature);

        return { mid, side };
    }

    // =====================================================================
    //  Decode: Mid/Side → L/R (through dedicated decode transformers)
    // =====================================================================
    struct Stereo { float left; float right; };

    inline Stereo decode (float mid, float side,
                          const Params& p = Params()) noexcept
    {
        const double sumBack  = (double)(mid + side);
        const double diffBack = (double)(mid - side);

        float left  = (float) xfmrDec[0].process (sumBack,  p.satAmount, p.temperature);
        float right = (float) xfmrDec[1].process (diffBack, p.satAmount, p.temperature);

        return { left, right };
    }

    // =====================================================================
    //  Combined encode + decode (round-trip with transformer coloration)
    //  For pre/post M/S processing in a single call
    // =====================================================================
    inline Stereo process (float left, float right,
                           float midGain, float sideGain,
                           const Params& p = Params()) noexcept
    {
        auto ms = encode (left, right, p);
        ms.mid  *= midGain;
        ms.side *= sideGain;
        return decode (ms.mid, ms.side, p);
    }

    // index 0 = Mid/L path, index 1 = Side/R path
    const TransformerPrimitive& getEncodeTransformer (int index) const noexcept
    {
        return xfmrEnc[std::clamp (index, 0, 1)];
    }

    const TransformerPrimitive& getDecodeTransformer (int index) const noexcept
    {
        return xfmrDec[std::clamp (index, 0, 1)];
    }

private:
    // Encode stage: xfmrEnc[0]=Mid path, xfmrEnc[1]=Side path
    // Decode stage: xfmrDec[0]=L path,   xfmrDec[1]=R path
    // Separate instances ensure independent magnetic state accumulation.
    TransformerPrimitive xfmrEnc[2];
    TransformerPrimitive xfmrDec[2];
};
