/*
    example_l1_constants.cpp — L1 Constants layer sample
    ============================================================
    Refer to physical constants and IC specs defined in dsp/constants/ and
    compute derived frequencies, time constants, and thresholds for circuit design.

    The L1 layer provides datasheet numbers without DSP processing.
    This sample prints practical values derived from constants as a parameter-check tool.

    Build:
        c++ -std=c++17 -O2 -I.. example_l1_constants.cpp -o example_l1_constants
*/

#include "include/patina.h"

#include <cstdio>
#include <cmath>

// Compute cutoff frequency for an RC filter
static double rcCutoff(double R, double C)
{
    return 1.0 / (2.0 * M_PI * R * C);
}

int main()
{
    std::printf("=== Patina L1 Constants — Circuit design parameters ===\n\n");

    // --- 1. Power & basic circuit parameters ---
    std::printf("── Power & basic circuit parameters ──\n");
    std::printf("  Supply voltage range:    %.0f V – %.0f V\n",
                PartsConstants::V_supplyMin, PartsConstants::V_supplyMax);
    std::printf("  Input impedance: %.0f kΩ\n",
                PartsConstants::R_input / 1e3);
    std::printf("  Tone pot:          %.0f kΩ\n",
                PartsConstants::R_tonePot / 1e3);
    std::printf("  Tone capacitor:    %.1f nF\n",
                PartsConstants::C_tone * 1e9);
    std::printf("  Input coupling cap: %.1f µF\n",
                PartsConstants::C_inputCoupling * 1e6);
    std::printf("\n");

    // --- 2. Derived cutoff frequencies (from constants) ---
    std::printf("── Derived cutoff frequencies ──\n");
    std::printf("  Drive HP fc:     %.1f Hz  (R=%.0fkΩ, C=%.0fnF)\n",
                PartsConstants::driveHpFcDefault,
                PartsConstants::R_driveHp / 1e3,
                PartsConstants::C_driveHp * 1e9);
    std::printf("  Output LPF fc:   %.1f Hz  (R=%.0fkΩ, C=%.1fnF)\n",
                PartsConstants::outputLpfFcHz,
                PartsConstants::R_outputLPF / 1e3,
                PartsConstants::C_outputLPF * 1e9);
    std::printf("  Input LPF fc:    %.1f Hz  (R=%.0fkΩ, C=%.1fnF)\n",
                PartsConstants::inputLpfFcHz,
                PartsConstants::R_inputLPF / 1e3,
                PartsConstants::C_inputLPF * 1e9);
    std::printf("  Tone range:       %.0f Hz – %.0f Hz\n",
                PartsConstants::toneFreqMin, PartsConstants::toneFreqMax);
    std::printf("\n");

    // --- 3. Diode constants ---
    std::printf("── Diodes ──\n");
    std::printf("  Forward voltage Vf (Si):    %.2f V\n", PartsConstants::diode_forward_v);
    std::printf("  Saturation slope:           %.1f\n", PartsConstants::diode_slope);
    std::printf("\n");

    // --- 4. OpAmp constants (JRC4558 / TL072 family) ---
    std::printf("── OpAmp constants ──\n");
    std::printf("  Open-loop gain:    %.0f (%.0f dB)\n",
                PartsConstants::opAmp_openLoopGain,
                20.0 * std::log10(PartsConstants::opAmp_openLoopGain));
    std::printf("  GBW:               %.1f MHz\n",
                PartsConstants::opAmp_gainBandwidthHz / 1e6);
    std::printf("  Slew rate:         %.1f V/µs\n",
                PartsConstants::opAmp_slewRate / 1e6);
    std::printf("  Sat threshold (9V): %.0f%% / (18V): %.0f%%\n",
                PartsConstants::opAmp_satThreshold9V * 100.0,
                PartsConstants::opAmp_satThreshold18V * 100.0);
    std::printf("  Headroom (9V):     %.0f%% / (18V): %.0f%%\n",
                PartsConstants::opAmp_headroomKnee9V * 100.0,
                PartsConstants::opAmp_headroomKnee18V * 100.0);
    std::printf("\n");

    // --- 5. NE570 / compander constants ---
    std::printf("── NE570 compander ──\n");
    std::printf("  Attack:          %.2f ms (C_T=%.2fµF, I=%.0fµA)\n",
                PartsConstants::ne570AttackSec * 1000.0,
                PartsConstants::C_T_NE570 * 1e6,
                PartsConstants::I_charge_NE570 * 1e6);
    std::printf("  Release:         %.1f ms (R_T=%.0fkΩ)\n",
                PartsConstants::ne570ReleaseSec * 1000.0,
                PartsConstants::R_T_NE570 / 1e3);
    std::printf("  RMS time const:  %.1f ms\n",
                PartsConstants::ne570RmsTimeSec * 1000.0);
    std::printf("  Emphasis fc:     %.0f Hz\n",
                PartsConstants::ne570EmphasisFcHz);
    std::printf("  Ref voltage Vref: %.0f mVrms\n",
                PartsConstants::V_ref_NE570 * 1000.0);
    std::printf("\n");

    // --- 6. BBD (Bucket Brigade) constants ---
    std::printf("── BBD chips ──\n");
    std::printf("  Default stages: %d\n", PartsConstants::bbdStagesDefault);
    std::printf("  Saturation headroom:   %.0f%%\n",
                PartsConstants::bbdSatHeadroomRatio * 100.0);
    std::printf("  Clock range:       %.0f kHz – %.0f kHz\n",
                PartsConstants::bbdClockHzMin / 1e3,
                PartsConstants::bbdClockHzMax / 1e3);
    std::printf("  Base noise:       %.1e\n", PartsConstants::bbdBaseNoise);
    std::printf("  HF comp fc:       %.0f Hz\n", PartsConstants::bbdHfCompFc);
    std::printf("\n");

    // --- 7. Aging parameters ---
    std::printf("── Aging parameters ──\n");
    std::printf("  Cap decrease rate:        %.1f%% / year\n",
                PartsConstants::aging_k_cap_perYear * 100.0);
    std::printf("  DA degradation rate:      %.1f%% / year (max %.0f%%)\n",
                PartsConstants::aging_da_perYear * 100.0,
                PartsConstants::aging_da_max * 100.0);
    std::printf("  Min cap scale:            %.0f%%\n",
                PartsConstants::aging_min_cap_scale * 100.0);

    // --- 8. Example: RC filter network design ---
    std::printf("\n── Example: RC filter network design ──\n");

    // トーンコントロール範囲
    double toneMin  = rcCutoff(PartsConstants::R_tonePot, PartsConstants::C_tone);
    double toneHalf = rcCutoff(PartsConstants::R_tonePot * 0.5, PartsConstants::C_tone);
    double toneMax  = rcCutoff(PartsConstants::tonePotMinOhm, PartsConstants::C_tone);
    std::printf("  Tone pot full (100kΩ): fc = %.1f Hz\n", toneMin);
    std::printf("  Tone pot 50%%    (50kΩ): fc = %.1f Hz\n", toneHalf);
    std::printf("  Tone pot min    (100Ω):  fc = %.0f Hz\n", toneMax);

    // フィードバックバッファ
    std::printf("  Feedback buffer gain: %.2f (%.1f dB)\n",
                PartsConstants::fbBufferGain,
                20.0 * std::log10(PartsConstants::fbBufferGain));

    // BBD クロックから最大ディレイ概算
    double maxDelayMs = (double)PartsConstants::bbdStagesDefault
                      / (2.0 * PartsConstants::bbdClockHzMin) * 1000.0;
    double minDelayMs = (double)PartsConstants::bbdStagesDefault
                      / (2.0 * PartsConstants::bbdClockHzMax) * 1000.0;
    std::printf("  BBD %d-stage delay range: %.1f – %.0f ms\n",
                PartsConstants::bbdStagesDefault, minDelayMs, maxDelayMs);

    std::printf("\n=== Done ===\n");
    return 0;
}
