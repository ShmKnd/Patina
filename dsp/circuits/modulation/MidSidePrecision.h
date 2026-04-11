#pragma once
#include <cmath>
#include <algorithm>
#include "../../parts/OpAmpPrimitive.h"
#include "../../core/ProcessSpec.h"

// =============================================================================
//  MidSidePrecision — Active OpAmp M/S Matrix (The Precision Method)
//
//  アクティブ・オペアンプ方式の Mid/Side エンコーダー/デコーダー。
//  オペアンプ（反転増幅器）を使って電気的に足し算と引き算を行う。
//  トランス方式に比べて非常にクリーンで正確。
//  ハイエンド・マスタリング・コンソールで採用される透明度の高い方式。
//
//  Encode:
//    Mid  = OpAmp( (L + R) × 0.5 )   — 加算回路のバッファ段
//    Side = OpAmp( (L - R) × 0.5 )   — 差動回路のバッファ段
//
//  Decode:
//    L = OpAmp( Mid + Side )
//    R = OpAmp( Mid - Side )
//
//  4-layer architecture:
//    Parts: OpAmpPrimitive × 4
//      opampEnc[0] = encode Mid path
//      opampEnc[1] = encode Side path
//      opampDec[0] = decode L path
//      opampDec[1] = decode R path
//    → Circuit: MidSidePrecision (clean M/S matrix with IC coloration)
//
//  Note: encode and decode use separate op-amp instances so that
//  each stage accumulates its own slew state independently —
//  matching real hardware where four separate op-amp stages are used.
//
//  Usage:
//    MidSidePrecision msEnc;
//    msEnc.prepare(spec);
//    auto [mid, side] = msEnc.encode(left, right);
//    // ... process mid/side independently ...
//    auto [outL, outR] = msEnc.decode(mid, side);
// =============================================================================
class MidSidePrecision
{
public:

    struct Params
    {
        bool highVoltage;
        constexpr Params (bool hv = true) noexcept : highVoltage (hv) {}
    };

    MidSidePrecision() noexcept = default;

    explicit MidSidePrecision (const OpAmpPrimitive::Spec& spec) noexcept
    {
        for (int i = 0; i < 2; ++i)
        {
            opampEnc[i] = OpAmpPrimitive (spec);
            opampDec[i] = OpAmpPrimitive (spec);
        }
    }

    void prepare (double sr) noexcept
    {
        for (int i = 0; i < 2; ++i)
        {
            opampEnc[i].prepare (sr);
            opampDec[i].prepare (sr);
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
            opampEnc[i].reset();
            opampDec[i].reset();
        }
    }

    // =====================================================================
    //  Encode: L/R → Mid/Side (through dedicated encode op-amps)
    // =====================================================================
    struct MidSide { float mid; float side; };

    inline MidSide encode (float left, float right,
                           const Params& p = Params()) noexcept
    {
        // Precision sum/difference with 0.5 scaling (prevents clipping)
        const double sum  = ((double)left + (double)right) * 0.5;
        const double diff = ((double)left - (double)right) * 0.5;

        // Op-amp buffer: slew rate limiting + saturation character
        float mid  = (float) opampEnc[0].process (sum,  p.highVoltage);
        float side = (float) opampEnc[1].process (diff, p.highVoltage);

        return { mid, side };
    }

    // =====================================================================
    //  Decode: Mid/Side → L/R (through dedicated decode op-amps)
    // =====================================================================
    struct Stereo { float left; float right; };

    inline Stereo decode (float mid, float side,
                          const Params& p = Params()) noexcept
    {
        const double sumBack  = (double)mid + (double)side;
        const double diffBack = (double)mid - (double)side;

        float left  = (float) opampDec[0].process (sumBack,  p.highVoltage);
        float right = (float) opampDec[1].process (diffBack, p.highVoltage);

        return { left, right };
    }

    // =====================================================================
    //  Combined encode + decode (round-trip with op-amp coloration)
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
    const OpAmpPrimitive& getEncodeOpAmp (int index) const noexcept
    {
        return opampEnc[std::clamp (index, 0, 1)];
    }

    const OpAmpPrimitive& getDecodeOpAmp (int index) const noexcept
    {
        return opampDec[std::clamp (index, 0, 1)];
    }

private:
    // Encode stage: opampEnc[0]=Mid path, opampEnc[1]=Side path
    // Decode stage: opampDec[0]=L path,   opampDec[1]=R path
    // Separate instances ensure independent slew state accumulation.
    OpAmpPrimitive opampEnc[2];
    OpAmpPrimitive opampDec[2];
};
