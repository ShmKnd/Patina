/*
  ==============================================================================

    patina_c.cpp — Patina DSP Library C API implementation
    Created: 4 Apr 2026

  ==============================================================================
*/

#include "patina_c.h"

// Pull in the entire Patina library
#include "include/patina.h"

#include <new>

/* ========================================================================== */
/*  Helpers: convert between C ↔ C++ param structs                           */
/* ========================================================================== */

static patina::ProcessSpec toCpp(const PatinaProcessSpec& s) {
    return { s.sample_rate, s.max_block_size, s.num_channels };
}

/* ----------  Delay  ---------- */

static patina::BbdDelayEngine::Params toCpp(const PatinaDelayParams& p) {
    patina::BbdDelayEngine::Params r;
    r.delayMs          = p.delay_ms;
    r.feedback         = p.feedback;
    r.tone             = p.tone;
    r.mix              = p.mix;
    r.compAmount       = p.comp_amount;
    r.chorusDepth      = p.chorus_depth;
    r.lfoRateHz        = p.lfo_rate_hz;
    r.supplyVoltage    = p.supply_voltage;
    r.bbdStages        = p.bbd_stages;
    r.emulateBBD       = p.emulate_bbd != 0;
    r.emulateOpAmpSat  = p.emulate_opamp_sat != 0;
    r.emulateToneRC    = p.emulate_tone_rc != 0;
    r.enableAging      = p.enable_aging != 0;
    r.ageYears         = p.age_years;
    r.capacitanceScale = p.capacitance_scale;
    r.pedalMode        = p.pedal_mode != 0;
    return r;
}

/* ----------  Drive  ---------- */

static patina::DriveEngine::Params toCpp(const PatinaDriveParams& p) {
    patina::DriveEngine::Params r;
    r.drive          = p.drive;
    r.clippingMode   = p.clipping_mode;
    r.diodeType      = p.diode_type;
    r.tone           = p.tone;
    r.outputLevel    = p.output_level;
    r.mix            = p.mix;
    r.supplyVoltage  = p.supply_voltage;
    r.temperature    = p.temperature;
    r.enablePowerSag = p.enable_power_sag != 0;
    r.sagAmount      = p.sag_amount;
    r.pedalMode      = p.pedal_mode != 0;
    return r;
}

/* ----------  Reverb  ---------- */

static patina::ReverbEngine::Params toCpp(const PatinaReverbParams& p) {
    patina::ReverbEngine::Params r;
    r.type           = p.type;
    r.decay          = p.decay;
    r.tone           = p.tone;
    r.mix            = p.mix;
    r.supplyVoltage  = p.supply_voltage;
    r.tension        = p.tension;
    r.dripAmount     = p.drip_amount;
    r.numSprings     = p.num_springs;
    r.predelayMs     = p.predelay_ms;
    r.damping        = p.damping;
    r.diffusion      = p.diffusion;
    r.modDepth       = p.mod_depth;
    r.pedalMode      = p.pedal_mode != 0;
    return r;
}

/* ----------  Compressor  ---------- */

static patina::CompressorEngine::Params toCpp(const PatinaCompressorParams& p) {
    patina::CompressorEngine::Params r;
    r.type            = p.type;
    r.inputGain       = p.input_gain;
    r.threshold       = p.threshold;
    r.outputGain      = p.output_gain;
    r.attack          = p.attack;
    r.release         = p.release;
    r.ratio           = p.ratio;
    r.mix             = p.mix;
    r.supplyVoltage   = p.supply_voltage;
    r.enableGate      = p.enable_gate != 0;
    r.gateThresholdDb = p.gate_threshold_db;
    r.photoMode       = p.photo_mode;
    r.pedalMode       = p.pedal_mode != 0;
    return r;
}

/* ----------  Modulation  ---------- */

static patina::ModulationEngine::Params toCpp(const PatinaModulationParams& p) {
    patina::ModulationEngine::Params r;
    r.type              = p.type;
    r.rate              = p.rate;
    r.depth             = p.depth;
    r.feedback          = p.feedback;
    r.mix               = p.mix;
    r.supplyVoltage     = p.supply_voltage;
    r.centerFreqHz      = p.center_freq_hz;
    r.freqSpreadHz      = p.freq_spread_hz;
    r.numStages         = p.num_stages;
    r.temperature       = p.temperature;
    r.tremoloMode       = p.tremolo_mode;
    r.stereoPhaseInvert = p.stereo_phase_invert != 0;
    r.chorusDelayMs     = p.chorus_delay_ms;
    r.stereoWidth       = p.stereo_width;
    r.pedalMode         = p.pedal_mode != 0;
    return r;
}

/* ----------  Tape  ---------- */

static patina::TapeMachineEngine::Params toCpp(const PatinaTapeParams& p) {
    patina::TapeMachineEngine::Params r;
    r.inputGain          = p.input_gain;
    r.saturation         = p.saturation;
    r.biasAmount         = p.bias_amount;
    r.tapeSpeed          = p.tape_speed;
    r.wowFlutter         = p.wow_flutter;
    r.enableHeadBump     = p.enable_head_bump != 0;
    r.enableHfRolloff    = p.enable_hf_rolloff != 0;
    r.headWear           = p.head_wear;
    r.tapeAge            = p.tape_age;
    r.enableTransformer  = p.enable_transformer != 0;
    r.transformerDrive   = p.transformer_drive;
    r.transformerSat     = p.transformer_sat;
    r.tone               = p.tone;
    r.mix                = p.mix;
    r.supplyVoltage      = p.supply_voltage;
    r.pedalMode          = p.pedal_mode != 0;
    return r;
}

/* ----------  EQ  ---------- */

static patina::EqEngine::Params toCpp(const PatinaEqParams& p) {
    patina::EqEngine::Params r;
    r.enableLow      = p.enable_low != 0;
    r.lowFreqHz      = p.low_freq_hz;
    r.lowGainDb      = p.low_gain_db;
    r.lowResonance   = p.low_resonance;
    r.enableMid      = p.enable_mid != 0;
    r.midFreqHz      = p.mid_freq_hz;
    r.midGainDb      = p.mid_gain_db;
    r.midQ           = p.mid_q;
    r.enableHigh     = p.enable_high != 0;
    r.highFreqHz     = p.high_freq_hz;
    r.highGainDb     = p.high_gain_db;
    r.highResonance  = p.high_resonance;
    r.temperature    = p.temperature;
    r.outputGainDb   = p.output_gain_db;
    r.supplyVoltage  = p.supply_voltage;
    r.pedalMode      = p.pedal_mode != 0;
    return r;
}

/* ----------  Limiter  ---------- */

static patina::LimiterEngine::Params toCpp(const PatinaLimiterParams& p) {
    patina::LimiterEngine::Params r;
    r.type         = p.type;
    r.ceiling      = p.ceiling;
    r.attack       = p.attack;
    r.release      = p.release;
    r.outputGain   = p.output_gain;
    r.mix          = p.mix;
    r.pedalMode    = p.pedal_mode != 0;
    r.supplyVoltage = p.supply_voltage;
    return r;
}

/* ----------  Filter  ---------- */

static patina::FilterEngine::Params toCpp(const PatinaFilterParams& p) {
    patina::FilterEngine::Params r;
    r.routing            = p.routing;
    r.filter1CutoffHz    = p.filter1_cutoff_hz;
    r.filter1Resonance   = p.filter1_resonance;
    r.filter1Type        = p.filter1_type;
    r.filter1Slope       = p.filter1_slope;
    r.filter2CutoffHz    = p.filter2_cutoff_hz;
    r.filter2Resonance   = p.filter2_resonance;
    r.filter2Type        = p.filter2_type;
    r.filter2Slope       = p.filter2_slope;
    r.drive1Amount       = p.drive1_amount;
    r.drive1Type         = p.drive1_type;
    r.drive2Amount       = p.drive2_amount;
    r.drive2Type         = p.drive2_type;
    r.drive3Amount       = p.drive3_amount;
    r.drive3Type         = p.drive3_type;
    r.outputLevel        = p.output_level;
    r.mix                = p.mix;
    r.temperature        = p.temperature;
    r.supplyVoltage      = p.supply_voltage;
    r.pedalMode          = p.pedal_mode != 0;
    r.normalize          = p.normalize != 0;
    return r;
}

/* ----------  Envelope Generator  ---------- */

static patina::EnvelopeGeneratorEngine::Params toCpp(const PatinaEnvelopeGeneratorParams& p) {
    patina::EnvelopeGeneratorEngine::Params r;
    r.attack           = p.attack;
    r.decay            = p.decay;
    r.sustain          = p.sustain;
    r.release          = p.release;
    r.envMode          = p.env_mode;
    r.curve            = p.curve;
    r.triggerMode      = p.trigger_mode;
    r.autoThresholdDb  = p.auto_threshold_db;
    r.velocity         = p.velocity;
    r.vcaDepth         = p.vca_depth;
    r.outputGain       = p.output_gain;
    r.mix              = p.mix;
    r.temperature      = p.temperature;
    r.pedalMode        = p.pedal_mode != 0;
    r.supplyVoltage    = p.supply_voltage;
    return r;
}

/* ----------  Channel Strip  ---------- */

static patina::ChannelStripEngine::Params toCpp(const PatinaChannelStripParams& p) {
    patina::ChannelStripEngine::Params r;
    r.preampDrive       = p.preamp_drive;
    r.preampBias        = p.preamp_bias;
    r.preampOutput      = p.preamp_output;
    r.tubeAge           = p.tube_age;
    r.enableEq          = p.enable_eq != 0;
    r.eqCutoffHz        = p.eq_cutoff_hz;
    r.eqResonance       = p.eq_resonance;
    r.eqType            = p.eq_type;
    r.eqTemperature     = p.eq_temperature;
    r.enableGate        = p.enable_gate != 0;
    r.gateThresholdDb   = p.gate_threshold_db;
    r.gateHysteresisDb  = p.gate_hysteresis_db;
    r.inputTrimDb       = p.input_trim_db;
    r.outputTrimDb      = p.output_trim_db;
    r.supplyVoltage     = p.supply_voltage;
    r.pedalMode         = p.pedal_mode != 0;
    return r;
}

/* ========================================================================== */
/*  Default params (mirror C++ defaults)                                      */
/* ========================================================================== */

PatinaDelayParams patina_delay_default_params(void) {
    PatinaDelayParams p{};
    p.delay_ms          = 250.0f;
    p.feedback          = 0.3f;
    p.tone              = 0.5f;
    p.mix               = 0.5f;
    p.comp_amount       = 0.5f;
    p.chorus_depth      = 0.0f;
    p.lfo_rate_hz       = 0.5f;
    p.supply_voltage    = 9.0;
    p.bbd_stages        = 8192;
    p.emulate_bbd       = 1;
    p.emulate_opamp_sat = 1;
    p.emulate_tone_rc   = 1;
    p.enable_aging      = 0;
    p.age_years         = 0.0;
    p.capacitance_scale = 1.0;
    p.pedal_mode        = 0;
    return p;
}

PatinaDriveParams patina_drive_default_params(void) {
    PatinaDriveParams p{};
    p.drive          = 0.5f;
    p.clipping_mode  = 1;
    p.diode_type     = 0;
    p.tone           = 0.5f;
    p.output_level   = 0.7f;
    p.mix            = 1.0f;
    p.supply_voltage = 9.0;
    p.temperature    = 25.0f;
    p.enable_power_sag = 0;
    p.sag_amount     = 0.5f;
    p.pedal_mode     = 0;
    return p;
}

PatinaReverbParams patina_reverb_default_params(void) {
    PatinaReverbParams p{};
    p.type           = PATINA_REVERB_SPRING;
    p.decay          = 0.5f;
    p.tone           = 0.5f;
    p.mix            = 0.3f;
    p.supply_voltage = 9.0;
    p.tension        = 0.5f;
    p.drip_amount    = 0.3f;
    p.num_springs    = 3;
    p.predelay_ms    = 10.0f;
    p.damping        = 0.5f;
    p.diffusion      = 0.7f;
    p.mod_depth      = 0.0f;
    p.pedal_mode     = 0;
    return p;
}

PatinaCompressorParams patina_compressor_default_params(void) {
    PatinaCompressorParams p{};
    p.type              = PATINA_COMP_FET;
    p.input_gain        = 0.5f;
    p.threshold         = 0.5f;
    p.output_gain       = 0.5f;
    p.attack            = 0.5f;
    p.release           = 0.5f;
    p.ratio             = 0;
    p.mix               = 1.0f;
    p.supply_voltage    = 9.0;
    p.enable_gate       = 0;
    p.gate_threshold_db = -40.0f;
    p.photo_mode        = 0;
    p.pedal_mode        = 0;
    return p;
}

PatinaModulationParams patina_modulation_default_params(void) {
    PatinaModulationParams p{};
    p.type               = PATINA_MOD_PHASER;
    p.rate               = 0.5f;
    p.depth              = 0.5f;
    p.feedback           = 0.3f;
    p.mix                = 0.5f;
    p.supply_voltage     = 9.0;
    p.center_freq_hz     = 1000.0f;
    p.freq_spread_hz     = 800.0f;
    p.num_stages         = 4;
    p.temperature        = 25.0f;
    p.tremolo_mode       = 0;
    p.stereo_phase_invert = 0;
    p.chorus_delay_ms    = 7.0f;
    p.stereo_width       = 0.5f;
    p.pedal_mode         = 0;
    return p;
}

PatinaTapeParams patina_tape_default_params(void) {
    PatinaTapeParams p{};
    p.input_gain         = 0.0f;
    p.saturation         = 0.5f;
    p.bias_amount        = 0.5f;
    p.tape_speed         = 1.0f;
    p.wow_flutter        = 0.0f;
    p.enable_head_bump   = 1;
    p.enable_hf_rolloff  = 1;
    p.head_wear          = 0.0f;
    p.tape_age           = 0.0f;
    p.enable_transformer = 1;
    p.transformer_drive  = 0.0f;
    p.transformer_sat    = 0.3f;
    p.tone               = 0.5f;
    p.mix                = 1.0f;
    p.supply_voltage     = 9.0;
    p.pedal_mode         = 0;
    return p;
}

PatinaChannelStripParams patina_channel_strip_default_params(void) {
    PatinaChannelStripParams p{};
    p.preamp_drive       = 0.3f;
    p.preamp_bias        = 0.5f;
    p.preamp_output      = 0.7f;
    p.tube_age           = 0.0f;
    p.enable_eq          = 1;
    p.eq_cutoff_hz       = 1000.0f;
    p.eq_resonance       = 0.5f;
    p.eq_type            = 0;
    p.eq_temperature     = 25.0f;
    p.enable_gate        = 0;
    p.gate_threshold_db  = -50.0f;
    p.gate_hysteresis_db = 6.0f;
    p.input_trim_db      = 0.0f;
    p.output_trim_db     = 0.0f;
    p.supply_voltage     = 9.0;
    p.pedal_mode         = 0;
    return p;
}

PatinaEqParams patina_eq_default_params(void) {
    PatinaEqParams p{};
    p.enable_low     = 1;
    p.low_freq_hz    = 200.0f;
    p.low_gain_db    = 0.0f;
    p.low_resonance  = 0.3f;
    p.enable_mid     = 1;
    p.mid_freq_hz    = 1000.0f;
    p.mid_gain_db    = 0.0f;
    p.mid_q          = 0.5f;
    p.enable_high    = 1;
    p.high_freq_hz   = 4000.0f;
    p.high_gain_db   = 0.0f;
    p.high_resonance = 0.3f;
    p.temperature    = 25.0f;
    p.output_gain_db = 0.0f;
    p.supply_voltage = 9.0;
    p.pedal_mode     = 0;
    return p;
}

PatinaLimiterParams patina_limiter_default_params(void) {
    PatinaLimiterParams p{};
    p.type           = PATINA_LIM_VCA;
    p.ceiling        = 0.8f;
    p.attack         = 0.1f;
    p.release        = 0.4f;
    p.output_gain    = 0.5f;
    p.mix            = 1.0f;
    p.pedal_mode     = 0;
    p.supply_voltage = 9.0;
    return p;
}

PatinaFilterParams patina_filter_default_params(void) {
    PatinaFilterParams p{};
    p.routing            = 0;
    p.filter1_cutoff_hz  = 1000.0f;
    p.filter1_resonance  = 0.5f;
    p.filter1_type       = PATINA_FILTER_LP;
    p.filter1_slope      = PATINA_FILTER_12DB;
    p.filter2_cutoff_hz  = 2000.0f;
    p.filter2_resonance  = 0.5f;
    p.filter2_type       = PATINA_FILTER_LP;
    p.filter2_slope      = PATINA_FILTER_12DB;
    p.drive1_amount      = 0.0f;
    p.drive1_type        = PATINA_DRIVE_TUBE;
    p.drive2_amount      = 0.0f;
    p.drive2_type        = PATINA_DRIVE_TUBE;
    p.drive3_amount      = 0.0f;
    p.drive3_type        = PATINA_DRIVE_TUBE;
    p.output_level       = 0.7f;
    p.mix                = 1.0f;
    p.temperature        = 25.0f;
    p.supply_voltage     = 9.0;
    p.pedal_mode         = 0;
    p.normalize          = 1;
    return p;
}

PatinaEnvelopeGeneratorParams patina_envelope_generator_default_params(void) {
    PatinaEnvelopeGeneratorParams p{};
    p.attack            = 0.3f;
    p.decay             = 0.3f;
    p.sustain           = 0.7f;
    p.release           = 0.4f;
    p.env_mode          = PATINA_ENV_MODE_ADSR;
    p.curve             = PATINA_ENV_CURVE_RC;
    p.trigger_mode      = PATINA_ENV_TRIGGER_EXTERNAL;
    p.auto_threshold_db = -30.0f;
    p.velocity          = 1.0f;
    p.vca_depth         = 1.0f;
    p.output_gain       = 0.5f;
    p.mix               = 1.0f;
    p.temperature       = 25.0f;
    p.pedal_mode        = 0;
    p.supply_voltage    = 9.0;
    return p;
}

/* ========================================================================== */
/*  BBD Delay Engine(void) {
    return reinterpret_cast<PatinaDelayEngine>(new(std::nothrow) patina::BbdDelayEngine());
}
void patina_delay_destroy(PatinaDelayEngine h) {
    delete reinterpret_cast<patina::BbdDelayEngine*>(h);
}
void patina_delay_prepare(PatinaDelayEngine h, const PatinaProcessSpec* spec) {
    if (!h || !spec) return;
    reinterpret_cast<patina::BbdDelayEngine*>(h)->prepare(toCpp(*spec));
}
void patina_delay_reset(PatinaDelayEngine h) {
    if (!h) return;
    reinterpret_cast<patina::BbdDelayEngine*>(h)->reset();
}
void patina_delay_process(PatinaDelayEngine h,
                          const float* const* input, float* const* output,
                          int num_channels, int num_samples,
                          const PatinaDelayParams* params) {
    if (!h || !params || !input || !output || num_channels <= 0 || num_samples <= 0) return;
    auto cpp = toCpp(*params);
    reinterpret_cast<patina::BbdDelayEngine*>(h)->processBlock(
        input, output, num_channels, num_samples, cpp);
}

/* ========================================================================== */
/*  Drive Engine                                                              */
/* ========================================================================== */

PatinaDriveEngine patina_drive_create(void) {
    return reinterpret_cast<PatinaDriveEngine>(new(std::nothrow) patina::DriveEngine());
}
void patina_drive_destroy(PatinaDriveEngine h) {
    delete reinterpret_cast<patina::DriveEngine*>(h);
}
void patina_drive_prepare(PatinaDriveEngine h, const PatinaProcessSpec* spec) {
    if (!h || !spec) return;
    reinterpret_cast<patina::DriveEngine*>(h)->prepare(toCpp(*spec));
}
void patina_drive_reset(PatinaDriveEngine h) {
    if (!h) return;
    reinterpret_cast<patina::DriveEngine*>(h)->reset();
}
void patina_drive_process(PatinaDriveEngine h,
                          const float* const* input, float* const* output,
                          int num_channels, int num_samples,
                          const PatinaDriveParams* params) {
    if (!h || !params || !input || !output || num_channels <= 0 || num_samples <= 0) return;
    auto cpp = toCpp(*params);
    reinterpret_cast<patina::DriveEngine*>(h)->processBlock(
        input, output, num_channels, num_samples, cpp);
}

/* ========================================================================== */
/*  Reverb Engine                                                             */
/* ========================================================================== */

PatinaReverbEngine patina_reverb_create(void) {
    return reinterpret_cast<PatinaReverbEngine>(new(std::nothrow) patina::ReverbEngine());
}
void patina_reverb_destroy(PatinaReverbEngine h) {
    delete reinterpret_cast<patina::ReverbEngine*>(h);
}
void patina_reverb_prepare(PatinaReverbEngine h, const PatinaProcessSpec* spec) {
    if (!h || !spec) return;
    reinterpret_cast<patina::ReverbEngine*>(h)->prepare(toCpp(*spec));
}
void patina_reverb_reset(PatinaReverbEngine h) {
    if (!h) return;
    reinterpret_cast<patina::ReverbEngine*>(h)->reset();
}
void patina_reverb_process(PatinaReverbEngine h,
                           const float* const* input, float* const* output,
                           int num_channels, int num_samples,
                           const PatinaReverbParams* params) {
    if (!h || !params || !input || !output || num_channels <= 0 || num_samples <= 0) return;
    auto cpp = toCpp(*params);
    reinterpret_cast<patina::ReverbEngine*>(h)->processBlock(
        input, output, num_channels, num_samples, cpp);
}

/* ========================================================================== */
/*  Compressor Engine                                                         */
/* ========================================================================== */

PatinaCompressorEngine patina_compressor_create(void) {
    return reinterpret_cast<PatinaCompressorEngine>(new(std::nothrow) patina::CompressorEngine());
}
void patina_compressor_destroy(PatinaCompressorEngine h) {
    delete reinterpret_cast<patina::CompressorEngine*>(h);
}
void patina_compressor_prepare(PatinaCompressorEngine h, const PatinaProcessSpec* spec) {
    if (!h || !spec) return;
    reinterpret_cast<patina::CompressorEngine*>(h)->prepare(toCpp(*spec));
}
void patina_compressor_reset(PatinaCompressorEngine h) {
    if (!h) return;
    reinterpret_cast<patina::CompressorEngine*>(h)->reset();
}
void patina_compressor_process(PatinaCompressorEngine h,
                               const float* const* input, float* const* output,
                               int num_channels, int num_samples,
                               const PatinaCompressorParams* params) {
    if (!h || !params || !input || !output || num_channels <= 0 || num_samples <= 0) return;
    auto cpp = toCpp(*params);
    reinterpret_cast<patina::CompressorEngine*>(h)->processBlock(
        input, output, num_channels, num_samples, cpp);
}
float patina_compressor_get_gain_reduction_db(PatinaCompressorEngine h, int channel) {
    if (!h) return 0.0f;
    return reinterpret_cast<patina::CompressorEngine*>(h)->getGainReductionDb(channel);
}
int patina_compressor_is_gate_open(PatinaCompressorEngine h, int channel) {
    if (!h) return 0;
    return reinterpret_cast<patina::CompressorEngine*>(h)->isGateOpen(channel) ? 1 : 0;
}

/* ========================================================================== */
/*  Modulation Engine                                                         */
/* ========================================================================== */

PatinaModulationEngine patina_modulation_create(void) {
    return reinterpret_cast<PatinaModulationEngine>(new(std::nothrow) patina::ModulationEngine());
}
void patina_modulation_destroy(PatinaModulationEngine h) {
    delete reinterpret_cast<patina::ModulationEngine*>(h);
}
void patina_modulation_prepare(PatinaModulationEngine h, const PatinaProcessSpec* spec) {
    if (!h || !spec) return;
    reinterpret_cast<patina::ModulationEngine*>(h)->prepare(toCpp(*spec));
}
void patina_modulation_reset(PatinaModulationEngine h) {
    if (!h) return;
    reinterpret_cast<patina::ModulationEngine*>(h)->reset();
}
void patina_modulation_process(PatinaModulationEngine h,
                               const float* const* input, float* const* output,
                               int num_channels, int num_samples,
                               const PatinaModulationParams* params) {
    if (!h || !params || !input || !output || num_channels <= 0 || num_samples <= 0) return;
    auto cpp = toCpp(*params);
    reinterpret_cast<patina::ModulationEngine*>(h)->processBlock(
        input, output, num_channels, num_samples, cpp);
}

/* ========================================================================== */
/*  Tape Machine Engine                                                       */
/* ========================================================================== */

PatinaTapeEngine patina_tape_create(void) {
    return reinterpret_cast<PatinaTapeEngine>(new(std::nothrow) patina::TapeMachineEngine());
}
void patina_tape_destroy(PatinaTapeEngine h) {
    delete reinterpret_cast<patina::TapeMachineEngine*>(h);
}
void patina_tape_prepare(PatinaTapeEngine h, const PatinaProcessSpec* spec) {
    if (!h || !spec) return;
    reinterpret_cast<patina::TapeMachineEngine*>(h)->prepare(toCpp(*spec));
}
void patina_tape_reset(PatinaTapeEngine h) {
    if (!h) return;
    reinterpret_cast<patina::TapeMachineEngine*>(h)->reset();
}
void patina_tape_process(PatinaTapeEngine h,
                         const float* const* input, float* const* output,
                         int num_channels, int num_samples,
                         const PatinaTapeParams* params) {
    if (!h || !params || !input || !output || num_channels <= 0 || num_samples <= 0) return;
    auto cpp = toCpp(*params);
    reinterpret_cast<patina::TapeMachineEngine*>(h)->processBlock(
        input, output, num_channels, num_samples, cpp);
}

/* ========================================================================== */
/*  Channel Strip Engine                                                      */
/* ========================================================================== */

PatinaChannelStrip patina_channel_strip_create(void) {
    return reinterpret_cast<PatinaChannelStrip>(new(std::nothrow) patina::ChannelStripEngine());
}
void patina_channel_strip_destroy(PatinaChannelStrip h) {
    delete reinterpret_cast<patina::ChannelStripEngine*>(h);
}
void patina_channel_strip_prepare(PatinaChannelStrip h, const PatinaProcessSpec* spec) {
    if (!h || !spec) return;
    reinterpret_cast<patina::ChannelStripEngine*>(h)->prepare(toCpp(*spec));
}
void patina_channel_strip_reset(PatinaChannelStrip h) {
    if (!h) return;
    reinterpret_cast<patina::ChannelStripEngine*>(h)->reset();
}
void patina_channel_strip_process(PatinaChannelStrip h,
                                  const float* const* input, float* const* output,
                                  int num_channels, int num_samples,
                                  const PatinaChannelStripParams* params) {
    if (!h || !params || !input || !output || num_channels <= 0 || num_samples <= 0) return;
    auto cpp = toCpp(*params);
    reinterpret_cast<patina::ChannelStripEngine*>(h)->processBlock(
        input, output, num_channels, num_samples, cpp);
}
float patina_channel_strip_get_output_level(PatinaChannelStrip h, int channel) {
    if (!h) return 0.0f;
    return reinterpret_cast<patina::ChannelStripEngine*>(h)->getOutputLevel(channel);
}
int patina_channel_strip_is_gate_open(PatinaChannelStrip h, int channel) {
    if (!h) return 0;
    return reinterpret_cast<patina::ChannelStripEngine*>(h)->isGateOpen(channel) ? 1 : 0;
}

/* ========================================================================== */
/*  EQ Engine                                                                 */
/* ========================================================================== */

PatinaEqEngine patina_eq_create(void) {
    return reinterpret_cast<PatinaEqEngine>(new(std::nothrow) patina::EqEngine());
}
void patina_eq_destroy(PatinaEqEngine h) {
    delete reinterpret_cast<patina::EqEngine*>(h);
}
void patina_eq_prepare(PatinaEqEngine h, const PatinaProcessSpec* spec) {
    if (!h || !spec) return;
    reinterpret_cast<patina::EqEngine*>(h)->prepare(toCpp(*spec));
}
void patina_eq_reset(PatinaEqEngine h) {
    if (!h) return;
    reinterpret_cast<patina::EqEngine*>(h)->reset();
}
void patina_eq_process(PatinaEqEngine h,
                       const float* const* input, float* const* output,
                       int num_channels, int num_samples,
                       const PatinaEqParams* params) {
    if (!h || !params || !input || !output || num_channels <= 0 || num_samples <= 0) return;
    auto cpp = toCpp(*params);
    reinterpret_cast<patina::EqEngine*>(h)->processBlock(
        input, output, num_channels, num_samples, cpp);
}

/* ========================================================================== */
/*  Limiter Engine                                                            */
/* ========================================================================== */

PatinaLimiterEngine patina_limiter_create(void) {
    return reinterpret_cast<PatinaLimiterEngine>(new(std::nothrow) patina::LimiterEngine());
}
void patina_limiter_destroy(PatinaLimiterEngine h) {
    delete reinterpret_cast<patina::LimiterEngine*>(h);
}
void patina_limiter_prepare(PatinaLimiterEngine h, const PatinaProcessSpec* spec) {
    if (!h || !spec) return;
    reinterpret_cast<patina::LimiterEngine*>(h)->prepare(toCpp(*spec));
}
void patina_limiter_reset(PatinaLimiterEngine h) {
    if (!h) return;
    reinterpret_cast<patina::LimiterEngine*>(h)->reset();
}
void patina_limiter_process(PatinaLimiterEngine h,
                            const float* const* input, float* const* output,
                            int num_channels, int num_samples,
                            const PatinaLimiterParams* params) {
    if (!h || !params || !input || !output || num_channels <= 0 || num_samples <= 0) return;
    auto cpp = toCpp(*params);
    reinterpret_cast<patina::LimiterEngine*>(h)->processBlock(
        input, output, num_channels, num_samples, cpp);
}
float patina_limiter_get_gain_reduction_db(PatinaLimiterEngine h, int channel) {
    if (!h) return 0.0f;
    return reinterpret_cast<patina::LimiterEngine*>(h)->getGainReductionDb(channel);
}

/* ========================================================================== */
/*  Filter Engine                                                             */
/* ========================================================================== */

PatinaFilterEngine patina_filter_create(void) {
    return reinterpret_cast<PatinaFilterEngine>(new(std::nothrow) patina::FilterEngine());
}
void patina_filter_destroy(PatinaFilterEngine h) {
    delete reinterpret_cast<patina::FilterEngine*>(h);
}
void patina_filter_prepare(PatinaFilterEngine h, const PatinaProcessSpec* spec) {
    if (!h || !spec) return;
    reinterpret_cast<patina::FilterEngine*>(h)->prepare(toCpp(*spec));
}
void patina_filter_reset(PatinaFilterEngine h) {
    if (!h) return;
    reinterpret_cast<patina::FilterEngine*>(h)->reset();
}
void patina_filter_process(PatinaFilterEngine h,
                           const float* const* input, float* const* output,
                           int num_channels, int num_samples,
                           const PatinaFilterParams* params) {
    if (!h || !params || !input || !output || num_channels <= 0 || num_samples <= 0) return;
    auto cpp = toCpp(*params);
    reinterpret_cast<patina::FilterEngine*>(h)->processBlock(
        input, output, num_channels, num_samples, cpp);
}

/* ========================================================================== */
/*  Envelope Generator Engine                                                 */
/* ========================================================================== */

PatinaEnvelopeGeneratorEngine patina_envelope_generator_create(void) {
    return reinterpret_cast<PatinaEnvelopeGeneratorEngine>(new(std::nothrow) patina::EnvelopeGeneratorEngine());
}
void patina_envelope_generator_destroy(PatinaEnvelopeGeneratorEngine h) {
    delete reinterpret_cast<patina::EnvelopeGeneratorEngine*>(h);
}
void patina_envelope_generator_prepare(PatinaEnvelopeGeneratorEngine h, const PatinaProcessSpec* spec) {
    if (!h || !spec) return;
    reinterpret_cast<patina::EnvelopeGeneratorEngine*>(h)->prepare(toCpp(*spec));
}
void patina_envelope_generator_reset(PatinaEnvelopeGeneratorEngine h) {
    if (!h) return;
    reinterpret_cast<patina::EnvelopeGeneratorEngine*>(h)->reset();
}
void patina_envelope_generator_process(PatinaEnvelopeGeneratorEngine h,
                                       const float* const* input, float* const* output,
                                       int num_channels, int num_samples,
                                       const PatinaEnvelopeGeneratorParams* params) {
    if (!h || !params || !input || !output || num_channels <= 0 || num_samples <= 0) return;
    auto cpp = toCpp(*params);
    reinterpret_cast<patina::EnvelopeGeneratorEngine*>(h)->processBlock(
        input, output, num_channels, num_samples, cpp);
}
float patina_envelope_generator_get_envelope(PatinaEnvelopeGeneratorEngine h, int channel) {
    if (!h) return 0.0f;
    return reinterpret_cast<patina::EnvelopeGeneratorEngine*>(h)->getEnvelope(channel);
}
void patina_envelope_generator_gate_on(PatinaEnvelopeGeneratorEngine h) {
    if (!h) return;
    reinterpret_cast<patina::EnvelopeGeneratorEngine*>(h)->gateOn();
}
void patina_envelope_generator_gate_off(PatinaEnvelopeGeneratorEngine h) {
    if (!h) return;
    reinterpret_cast<patina::EnvelopeGeneratorEngine*>(h)->gateOff();
}

const char* patina_version(void) {
    return "1.0.0";
}
