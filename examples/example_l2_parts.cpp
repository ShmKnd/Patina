/*
    example_l2_parts.cpp — L2 Parts (analog primitives) sample
    ============================================================
    Drive the 9 analog part models in dsp/parts/ individually and
    write WAVs demonstrating their nonlinear behavior and temperature dependence.

    Sections (1 sec each):
        1. DiodePrimitive  — compare Si / Schottky / Ge clipping
        2. TubeTriode      — 12AX7 plate behavior (new vs aged)
        3. OTA_Primitive   — LM13700 saturation curve (25°C vs 50°C)
        4. JFET_Primitive  — 2N5457 soft clip + VCA nonlinearity
        5. TransformerPrimitive — console-style transformer comparison
        6. TapePrimitive   — tape hysteresis saturation
        7. BJT_Primitive + RC_Element — hand-assemble a ladder stage from primitives
        8. PhotocellPrimitive — opto-compressor cell comparison

    Build:
        c++ -std=c++17 -O2 -I.. example_l2_parts.cpp -o example_l2_parts
*/

#include "include/patina.h"
#include "examples/wav_writer.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <random>

int main()
{
    constexpr int    kSampleRate   = 48000;
    constexpr float  kSectionSec   = 1.0f;
    constexpr int    kSectionLen   = (int)(kSampleRate * kSectionSec);
    constexpr int    kNumSections  = 8;
    constexpr int    kTotalSamples = kSectionLen * kNumSections;

    std::vector<std::vector<float>> output(2,
        std::vector<float>((size_t)kTotalSamples, 0.0f));

    // Test signal (440 Hz sine burst)
    auto testSig = wav::sineBurst(kSampleRate, 440.0f,
                                   (float)kNumSections, 0.7f, 30.0f, 200.0f);

    int offset = 0;

    // ================================================================
    // Section 1: DiodePrimitive — Si / Schottky / Ge クリッピング
    // ================================================================
    {
        std::printf("Section 1: DiodePrimitive — 3-diode comparison\n");
        DiodePrimitive si(DiodePrimitive::Si1N4148());
        DiodePrimitive schottky(DiodePrimitive::Schottky1N5818());
        DiodePrimitive ge(DiodePrimitive::GeOA91());

        const int third = kSectionLen / 3;
        for (int i = 0; i < kSectionLen; ++i)
        {
            float x = testSig[(size_t)(offset + i)] * 2.0f;  // drive
            double clipped;
            if (i < third)
                clipped = si.clip(x, 25.0);       // L: Si
            else if (i < third * 2)
                clipped = schottky.clip(x, 25.0);  // L: Schottky
            else
                clipped = ge.clip(x, 25.0);        // L: Ge

            // R channel: tanh soft saturation
            double sat;
            if (i < third)
                sat = si.saturate(x, 25.0);
            else if (i < third * 2)
                sat = schottky.saturate(x, 25.0);
            else
                sat = ge.saturate(x, 25.0);

            output[0][(size_t)(offset + i)] = (float)clipped;
            output[1][(size_t)(offset + i)] = (float)sat;
        }

        // Print temperature-dependent Vf
        std::printf("  Si   Vf@25C=%.3fV  Vf@50C=%.3fV  JunctionCap@0V=%.2fpF\n",
                    si.effectiveVf(25.0), si.effectiveVf(50.0),
                    si.junctionCapacitance(0.0) * 1e12);
        std::printf("  Ge   Vf@25C=%.3fV  Vf@50C=%.3fV  JunctionCap@0V=%.2fpF\n",
                    ge.effectiveVf(25.0), ge.effectiveVf(50.0),
                    ge.junctionCapacitance(0.0) * 1e12);
    }
    offset += kSectionLen;

    // ================================================================
    // Section 2: TubeTriode — 12AX7 (new vs aged)
    // ================================================================
    {
        std::printf("Section 2: TubeTriode — 12AX7 new vs aged\n");
        TubeTriode tube(TubeTriode::T12AX7());
        tube.prepare((double)kSampleRate);

        const int half = kSectionLen / 2;
        for (int i = 0; i < kSectionLen; ++i)
        {
            float x = testSig[(size_t)(offset + i)] * 1.5f;
            double ageFactor = (i < half) ? 1.0 : 0.5;  // 後半は劣化管

            // 非対称 tanh プレート特性
            double plate = TubeTriode::transferFunction((double)x, ageFactor);
            // Miller capacitance LPF
            plate = tube.processMiller(plate);
            // Plate impedance processing
            plate = tube.processPlateImpedance(plate);

            output[0][(size_t)(offset + i)] = (float)plate;

            // R: グリッドコンダクション効果
            double gridOut = TubeTriode::transferFunction((double)x, ageFactor);
            gridOut = tube.processGridConduction(gridOut);
            output[1][(size_t)(offset + i)] = (float)gridOut;
        }

        std::printf("  T12AX7 rp=%.1fkΩ, millerCap=%.1fpF, millerAv=%.0f\n",
                    tube.getSpec().plateR / 1e3,
                    tube.getSpec().millerCap * 1e12,
                    tube.getSpec().millerAv);
    }
    offset += kSectionLen;

    // ================================================================
    // Section 3: OTA_Primitive — LM13700 温度特性
    // ================================================================
    {
        std::printf("Section 3: OTA_Primitive — LM13700 temperature comparison\n");
        OTA_Primitive lm13700(OTA_Primitive::LM13700(), 42);
        OTA_Primitive ca3080(OTA_Primitive::CA3080(), 99);

        const int half = kSectionLen / 2;
        double state = 0.0;
        double gCoeff = 0.3;
        for (int i = 0; i < kSectionLen; ++i)
        {
            float x = testSig[(size_t)(offset + i)] * 1.8f;
            if (i < half)
            {
                // L: LM13700 @ 25°C
                double sat = lm13700.saturate((double)x);
                output[0][(size_t)(offset + i)] = (float)sat;
                // R: LM13700 integrator
                state = lm13700.integrate((double)x, state, gCoeff);
                output[1][(size_t)(offset + i)] = (float)state;
            }
            else
            {
                // L: CA3080 saturation (softer)
                double sat = ca3080.saturate((double)x);
                output[0][(size_t)(offset + i)] = (float)sat;
                // R: CA3080 integrator
                state = ca3080.integrate((double)x, state, gCoeff);
                output[1][(size_t)(offset + i)] = (float)state;
            }
        }

        std::printf("  LM13700 mismatch=%.4f, gmScale@25C=%.3f, gmScale@50C=%.3f\n",
                lm13700.getMismatch(),
                lm13700.gmScale(25.0), lm13700.gmScale(50.0));
        std::printf("  CA3080  satV=%.1fV (vs LM13700 %.1fV)\n",
                ca3080.getSpec().saturationV,
                lm13700.getSpec().saturationV);
    }
    offset += kSectionLen;

    // ================================================================
    // Section 4: JFET_Primitive — 2N5457 soft clip + VCA
    // ================================================================
    {
        std::printf("Section 4: JFET_Primitive — 2N5457 soft clip\n");
        JFET_Primitive n2n5457(JFET_Primitive::N2N5457(), 123);
        JFET_Primitive n2n3819(JFET_Primitive::N2N3819(), 456);

        const int half = kSectionLen / 2;
        for (int i = 0; i < kSectionLen; ++i)
        {
            float x = testSig[(size_t)(offset + i)] * 3.0f;
            if (i < half)
            {
                // L: 2N5457 soft clip
                output[0][(size_t)(offset + i)] = (float)n2n5457.softClip((double)x);
                // R: VCA even-harmonic distortion ("light" compression)
                output[1][(size_t)(offset + i)] = (float)n2n5457.vcaNonlinearity((double)x, 0.3);
            }
            else
            {
                // L: 2N3819 soft clip (deeper pinch-off)
                output[0][(size_t)(offset + i)] = (float)n2n3819.softClip((double)x);
                // R: VCA (stronger compression)
                output[1][(size_t)(offset + i)] = (float)n2n3819.vcaNonlinearity((double)x, 0.7);
            }
        }

        std::printf("  2N5457 Vp=%.1fV, gmScale=%.4f, tempFreq@40C=%.4f\n",
                    n2n5457.getSpec().Vp, n2n5457.getGmScale(),
                    n2n5457.tempFreqScale(40.0));
        std::printf("  2N3819 Vp=%.1fV, gmScale=%.4f\n",
                    n2n3819.getSpec().Vp, n2n3819.getGmScale());
    }
    offset += kSectionLen;

    // ================================================================
    // Section 5: TransformerPrimitive — British vs American
    // ================================================================
    {
        std::printf("Section 5: TransformerPrimitive — console comparison\n");
        TransformerPrimitive brit(TransformerPrimitive::BritishConsole());
        TransformerPrimitive amer(TransformerPrimitive::AmericanConsole());
        brit.prepare((double)kSampleRate);
        amer.prepare((double)kSampleRate);

        const int half = kSectionLen / 2;
        for (int i = 0; i < kSectionLen; ++i)
        {
            float x = testSig[(size_t)(offset + i)] * 2.5f;
            if (i < half)
            {
                // L: British — full chain (sat=0.6)
                double sat = brit.process((double)x, 0.6);
                output[0][(size_t)(offset + i)] = (float)sat;
                // R: American — full chain (sat=0.6)
                double satA = amer.process((double)x, 0.6);
                output[1][(size_t)(offset + i)] = (float)satA;
            }
            else
            {
                // Second half: increase saturation for overdrive
                double sat = brit.process((double)x, 0.9);
                output[0][(size_t)(offset + i)] = (float)sat;

                double satA = amer.process((double)x, 0.9);
                output[1][(size_t)(offset + i)] = (float)satA;
            }
        }

        std::printf("  British: windingR=%.0fΩ, resonance gain=%.2f\n",
                    brit.getSpec().windingR, brit.getSpec().resonanceGain);
        std::printf("  American: windingR=%.0fΩ, resonance gain=%.2f\n",
                    amer.getSpec().windingR, amer.getSpec().resonanceGain);
    }
    offset += kSectionLen;

        // ================================================================
    // Section 6: TapePrimitive — tape hysteresis
    // ================================================================
    {
        std::printf("Section 6: TapePrimitive — tape saturation + hiss\n");
        TapePrimitive tape(TapePrimitive::HighSpeedDeck());

        std::mt19937 rng(777);
        std::normal_distribution<double> whiteDist(0.0, 1.0);

        const int half = kSectionLen / 2;
        for (int i = 0; i < kSectionLen; ++i)
        {
            float x = testSig[(size_t)(offset + i)] * 2.0f;
            double white = whiteDist(rng);

            if (i < half)
            {
                // L: low saturation (clean)
                double hyst = tape.processHysteresis((double)x, 0.2);
                double hiss = tape.generateHiss(white, 0.0);
                output[0][(size_t)(offset + i)] = (float)(hyst + hiss);
                output[1][(size_t)(offset + i)] = (float)(hyst - hiss);  // out-of-phase hiss
            }
            else
            {
                // R: high saturation + age-related hiss
                double hyst = tape.processHysteresis((double)x, 0.8);
                double hiss = tape.generateHiss(white, 0.7);
                output[0][(size_t)(offset + i)] = (float)(hyst + hiss);
                output[1][(size_t)(offset + i)] = (float)(hyst - hiss);
            }
        }

        std::printf("  HighSpeedDeck: gapWidth=%.1fµm, hissLevel=%.1e\n",
                    tape.getSpec().gapWidthNew, tape.getSpec().baseHissLevel);
        std::printf("  gapLossFc @15ips/new=%.0fHz, @15ips/worn=%.0fHz\n",
                tape.gapLossFc(1.0, 0.0), tape.gapLossFc(1.0, 0.8));
    }
    offset += kSectionLen;

    // ================================================================
    // Section 7: BJT_Primitive + RC_Element — hand-build a ladder stage
    // ================================================================
    {
        std::printf("Section 7: BJT + RC — hand-build a ladder stage\n");

        // Construct a 4-pole ladder from 4 BJT primitives
        BJT_Primitive bjt1(BJT_Primitive::Matched(), 101);
        BJT_Primitive bjt2(BJT_Primitive::Matched(), 102);
        BJT_Primitive bjt3(BJT_Primitive::Matched(), 103);
        BJT_Primitive bjt4(BJT_Primitive::Matched(), 104);

        // Inter-stage RC coupling — used as output LPF
        RC_Element rc(PartsConstants::R_outputLPF, PartsConstants::C_outputLPF);
        rc.prepare((double)kSampleRate);

        // 4-pole state variables
        double s1 = 0.0, s2 = 0.0, s3 = 0.0, s4 = 0.0;
        double cap1 = 0.0, cap2 = 0.0, cap3 = 0.0, cap4 = 0.0;

        const int half = kSectionLen / 2;
        for (int i = 0; i < kSectionLen; ++i)
        {
            float x = testSig[(size_t)(offset + i)] * 1.5f;

            // Sweep cutoff frequency (200 → 4000 Hz)
            float t = (float)i / (float)kSectionLen;
            double fc = 200.0 + t * 3800.0;
            double dt = 1.0 / (double)kSampleRate;
            double gCoeff = std::tan(3.14159265 * fc * dt);

            // Resonance: low in first half, high in second half
            double reso = (i < half) ? 0.3 : 0.8;

            // 4-pole ladder: feedback
            double fb = reso * 4.0 * s4;
            double input = (double)x - fb;

            // 4-stage BJT integrator cascade
            s1 = bjt1.integrate(input, s1, gCoeff);
            s1 = bjt1.interStageCoupling(s1, cap1);

            s2 = bjt2.integrate(s1, s2, gCoeff);
            s2 = bjt2.interStageCoupling(s2, cap2);

            s3 = bjt3.integrate(s2, s3, gCoeff);
            s3 = bjt3.interStageCoupling(s3, cap3);

            s4 = bjt4.integrate(s3, s4, gCoeff);
            s4 = bjt4.interStageCoupling(s4, cap4);

            // L: 4-pole LP output (BJT ladder)
            output[0][(size_t)(offset + i)] = (float)s4;

            // R: additional filtering via RC_Element
            double rcOut = rc.processLPF(s4);
            output[1][(size_t)(offset + i)] = (float)rcOut;
        }

        std::printf("  BJT mismatch: [%.4f, %.4f, %.4f, %.4f]\n",
                    bjt1.getMismatch(), bjt2.getMismatch(),
                    bjt3.getMismatch(), bjt4.getMismatch());
        std::printf("  RC fc=%.1fHz (R=%.0fkΩ, C=%.1fnF)\n",
                    rc.cutoffHz(), rc.getR() / 1e3, rc.getC() * 1e9);
    }
    offset += kSectionLen;

    // ================================================================
    // Section 8: PhotocellPrimitive — T4B opto-compressor cell
    // ================================================================
    {
        std::printf("Section 8: PhotocellPrimitive — T4B vs VTL5C3\n");
        PhotocellPrimitive t4b(PhotocellPrimitive::T4B());
        PhotocellPrimitive vtl(PhotocellPrimitive::VTL5C3());
        t4b.prepare((double)kSampleRate);
        vtl.prepare((double)kSampleRate);

        for (int i = 0; i < kSectionLen; ++i)
        {
            float x = testSig[(size_t)(offset + i)] * 2.0f;
            float absX = std::abs(x);

            // Sidechain -> get attenuation -> apply as VCA
            double attenT4b = t4b.process((double)absX);
            double attenVtl = vtl.process((double)absX);

            // L: T4B (slow release, memory effect)
            output[0][(size_t)(offset + i)] = x * (float)(1.0 - attenT4b * 0.8);
            // R: VTL5C3 (faster response)
            output[1][(size_t)(offset + i)] = x * (float)(1.0 - attenVtl * 0.8);
        }

        std::printf("  T4B: attack=%.1fms, releaseMin=%.0fms, releaseMax=%.0fms\n",
                    t4b.getSpec().cdsAttackMs,
                    t4b.getSpec().cdsReleaseMinMs,
                    t4b.getSpec().cdsReleaseMaxMs);
        std::printf("  VTL5C3: attack=%.1fms, releaseMin=%.0fms\n",
                    vtl.getSpec().cdsAttackMs,
                    vtl.getSpec().cdsReleaseMinMs);
    }

    // --- WAV output ---
    if (wav::write("output_l2_parts.wav", output, kSampleRate))
        std::printf("\n=> output_l2_parts.wav written (%d samples, %.1f sec)\n",
                    kTotalSamples, (float)kTotalSamples / kSampleRate);
    else
        std::fprintf(stderr, "Error: failed to write WAV\n");

    return 0;
}
