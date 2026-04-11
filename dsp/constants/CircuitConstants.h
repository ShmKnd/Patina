#pragma once
// CircuitConstants.h — General circuit component values (R, C, supply voltage, filters, tone)
namespace PartsConstants {
    // base sample rate
    inline constexpr double defaultSampleRate = 44100.0;

    // supply voltage
    inline constexpr double V_supplyMin = 9.0;
    inline constexpr double V_supplyMax = 18.0;

    // resistance
    inline constexpr double R_input = 1e6;
    inline constexpr double R_feedbackPotMax = 500000.0;
    inline constexpr double R_feedbackFixed = 470000.0;
    inline constexpr double R_tonePot = 100000.0;
    inline constexpr double R_fbBufferRf = 12000.0;
    inline constexpr double R_fbBufferRg = 100000.0;
    inline constexpr double R_clockRangeLow = 200.0;
    inline constexpr double R_clockRangeHigh = 100000.0;
    inline constexpr double R_driveHp = 12000.0;
    inline constexpr double R_outputLPF = 10000.0;
    inline constexpr double R_inputLPF = 10000.0;

    // capacitor
    inline constexpr double C_tone = 22e-9;
    inline constexpr double C_bbdCoupling = 470e-12;
    inline constexpr double C_inputCoupling = 4.7e-6;
    inline constexpr double C_driveHp = 130e-9;
    inline constexpr double C_clock = 100e-12;
    inline constexpr double C_outputLPF = 3.3e-9;
    inline constexpr double C_inputLPF = 3.5e-9;
    inline constexpr double C_inputBufferDefault = 100e-12;

    // Derived cutoff frequencies
    inline constexpr double driveHpFcDefault = 1.0 / (2.0 * 3.14159265358979323846 * R_driveHp * C_driveHp);
    inline constexpr double outputLpfFcHz = 1.0 / (2.0 * 3.14159265358979323846 * R_outputLPF * C_outputLPF);
    inline constexpr double inputLpfFcHz = 1.0 / (2.0 * 3.14159265358979323846 * R_inputLPF * C_inputLPF);

    // Feedback buffer gain
    inline constexpr double fbBufferGain = 1.0 + R_fbBufferRf / R_fbBufferRg;

    // Noise
    inline constexpr double thermalNoise = 1e-9;

    // PAD gain
    inline constexpr double padGainInstrument = 0.1;
    inline constexpr double padGainLine = 0.25118864;

    // Output stage
    inline constexpr double outputSatLimitRatio = 0.95;
    inline constexpr double outputSatBlend = 0.85;

    // Tone control
    inline constexpr double toneFreqMin = 500.0;
    inline constexpr double toneFreqMax = 8000.0;
    inline constexpr double tonePotMinOhm = 100.0;
    inline constexpr double toneRcMinHz = 200.0;
    inline constexpr double toneRcMaxHz = 12000.0;
    inline constexpr double toneRcLinearBlend = 0.6;

    // Emulation flags
    inline constexpr bool emulateDiodeDistortion = true;
    inline constexpr bool emulateOpAmpSaturation = true;
    inline constexpr bool emulateToneRC = true;
}
