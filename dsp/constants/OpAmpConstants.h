#pragma once
// OpAmpConstants.h — Op-amp characteristic constants (default values for circuit modules)
//
// Use OpAmpPrimitive::Spec presets for IC-specific datasheet values.
// Remaining constants are circuit context-dependent operating point parameters
// (noise floor scale, headroom reference, drift coefficient, etc.).
namespace PartsConstants {

    // IC-specific parameters (migrated to OpAmpPrimitive::Spec — kept for reference/compatibility)
    inline constexpr double opAmp_openLoopGain    = 5e4;      // → OpAmpPrimitive::Spec::openLoopGain
    inline constexpr double opAmp_gainBandwidthHz = 1.0e6;    // → OpAmpPrimitive::Spec::gbwHz
    inline constexpr double opAmp_slewRate        = 0.5e6;    // → OpAmpPrimitive::Spec::slewRate (JRC4558D baseline)
    inline constexpr double opAmp_satThreshold18V = 0.88;     // → OpAmpPrimitive::Spec::satThreshold18V
    inline constexpr double opAmp_satCurve18V     = 2.0;      // → OpAmpPrimitive::Spec::satCurve18V
    inline constexpr double opAmp_satThreshold9V  = 0.70;     // → OpAmpPrimitive::Spec::satThreshold9V
    inline constexpr double opAmp_satCurve9V      = 3.0;      // → OpAmpPrimitive::Spec::satCurve9V

    // Circuit context-dependent (not in OpAmpPrimitive — referenced by each circuit module)
    inline constexpr double opAmp_outputSaturation = 18.0;    // CompanderModule VCA output rail [V]
    inline constexpr double opAmp_noiseScale18V    = 1.15;    // BbdFeedback: 18V noise floor scale
    inline constexpr double opAmp_noiseScale9V     = 1.0;     // BbdFeedback: 9V noise floor scale
    inline constexpr double opAmp_headroomKnee18V  = 0.85;    // InputBuffer: 18V headroom reference
    inline constexpr double opAmp_headroomKnee9V   = 0.5;     // InputBuffer: 9V headroom reference
    inline constexpr double opAmp_offsetDriftScale = 2000.0;  // BbdFeedback: offset drift coefficient
}
