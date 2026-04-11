//! Raw FFI declarations for the Patina C API.
//!
//! These map 1:1 to the functions declared in `patina_c.h`.
//! Prefer using the safe wrappers in the parent module.

#![allow(non_camel_case_types)]

use std::os::raw::{c_char, c_float, c_int};

/* ========================================================================== */
/*  Common types                                                              */
/* ========================================================================== */

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PatinaProcessSpec {
    pub sample_rate: f64,
    pub max_block_size: c_int,
    pub num_channels: c_int,
}

/* ========================================================================== */
/*  Opaque handles                                                            */
/* ========================================================================== */

#[repr(C)]
pub struct PatinaDelayEngine_t {
    _opaque: [u8; 0],
}
#[repr(C)]
pub struct PatinaDriveEngine_t {
    _opaque: [u8; 0],
}
#[repr(C)]
pub struct PatinaReverbEngine_t {
    _opaque: [u8; 0],
}
#[repr(C)]
pub struct PatinaCompressorEngine_t {
    _opaque: [u8; 0],
}
#[repr(C)]
pub struct PatinaModulationEngine_t {
    _opaque: [u8; 0],
}
#[repr(C)]
pub struct PatinaTapeEngine_t {
    _opaque: [u8; 0],
}
#[repr(C)]
pub struct PatinaChannelStrip_t {
    _opaque: [u8; 0],
}

pub type PatinaDelayEngine = *mut PatinaDelayEngine_t;
pub type PatinaDriveEngine = *mut PatinaDriveEngine_t;
pub type PatinaReverbEngine = *mut PatinaReverbEngine_t;
pub type PatinaCompressorEngine = *mut PatinaCompressorEngine_t;
pub type PatinaModulationEngine = *mut PatinaModulationEngine_t;
pub type PatinaTapeEngine = *mut PatinaTapeEngine_t;
pub type PatinaChannelStrip = *mut PatinaChannelStrip_t;

/* ========================================================================== */
/*  Param structs                                                             */
/* ========================================================================== */

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PatinaDelayParams {
    pub delay_ms: c_float,
    pub feedback: c_float,
    pub tone: c_float,
    pub mix: c_float,
    pub comp_amount: c_float,
    pub chorus_depth: c_float,
    pub lfo_rate_hz: c_float,
    pub supply_voltage: f64,
    pub bbd_stages: c_int,
    pub emulate_bbd: c_int,
    pub emulate_opamp_sat: c_int,
    pub emulate_tone_rc: c_int,
    pub enable_aging: c_int,
    pub age_years: f64,
    pub capacitance_scale: f64,
    pub pedal_mode: c_int,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PatinaDriveParams {
    pub drive: c_float,
    pub clipping_mode: c_int,
    pub diode_type: c_int,
    pub tone: c_float,
    pub output_level: c_float,
    pub mix: c_float,
    pub supply_voltage: f64,
    pub temperature: c_float,
    pub enable_power_sag: c_int,
    pub sag_amount: c_float,
    pub pedal_mode: c_int,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PatinaReverbParams {
    pub r#type: c_int,
    pub decay: c_float,
    pub tone: c_float,
    pub mix: c_float,
    pub supply_voltage: f64,
    pub tension: c_float,
    pub drip_amount: c_float,
    pub num_springs: c_int,
    pub predelay_ms: c_float,
    pub damping: c_float,
    pub diffusion: c_float,
    pub mod_depth: c_float,
    pub pedal_mode: c_int,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PatinaCompressorParams {
    pub r#type: c_int,
    pub input_gain: c_float,
    pub threshold: c_float,
    pub output_gain: c_float,
    pub attack: c_float,
    pub release: c_float,
    pub ratio: c_int,
    pub mix: c_float,
    pub supply_voltage: f64,
    pub enable_gate: c_int,
    pub gate_threshold_db: c_float,
    pub photo_mode: c_int,
    pub pedal_mode: c_int,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PatinaModulationParams {
    pub r#type: c_int,
    pub rate: c_float,
    pub depth: c_float,
    pub feedback: c_float,
    pub mix: c_float,
    pub supply_voltage: f64,
    pub center_freq_hz: c_float,
    pub freq_spread_hz: c_float,
    pub num_stages: c_int,
    pub temperature: c_float,
    pub tremolo_mode: c_int,
    pub stereo_phase_invert: c_int,
    pub chorus_delay_ms: c_float,
    pub stereo_width: c_float,
    pub pedal_mode: c_int,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PatinaTapeParams {
    pub input_gain: c_float,
    pub saturation: c_float,
    pub bias_amount: c_float,
    pub tape_speed: c_float,
    pub wow_flutter: c_float,
    pub enable_head_bump: c_int,
    pub enable_hf_rolloff: c_int,
    pub head_wear: c_float,
    pub tape_age: c_float,
    pub enable_transformer: c_int,
    pub transformer_drive: c_float,
    pub transformer_sat: c_float,
    pub tone: c_float,
    pub mix: c_float,
    pub supply_voltage: f64,
    pub pedal_mode: c_int,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PatinaChannelStripParams {
    pub preamp_drive: c_float,
    pub preamp_bias: c_float,
    pub preamp_output: c_float,
    pub tube_age: c_float,
    pub enable_eq: c_int,
    pub eq_cutoff_hz: c_float,
    pub eq_resonance: c_float,
    pub eq_type: c_int,
    pub eq_temperature: c_float,
    pub enable_gate: c_int,
    pub gate_threshold_db: c_float,
    pub gate_hysteresis_db: c_float,
    pub input_trim_db: c_float,
    pub output_trim_db: c_float,
    pub supply_voltage: f64,
    pub pedal_mode: c_int,
}

#[repr(C)]
pub struct PatinaEqEngine_t {
    _opaque: [u8; 0],
}
#[repr(C)]
pub struct PatinaLimiterEngine_t {
    _opaque: [u8; 0],
}
#[repr(C)]
pub struct PatinaFilterEngine_t {
    _opaque: [u8; 0],
}
#[repr(C)]
pub struct PatinaEnvelopeGeneratorEngine_t {
    _opaque: [u8; 0],
}

pub type PatinaEqEngine = *mut PatinaEqEngine_t;
pub type PatinaLimiterEngine = *mut PatinaLimiterEngine_t;
pub type PatinaFilterEngine = *mut PatinaFilterEngine_t;
pub type PatinaEnvelopeGeneratorEngine = *mut PatinaEnvelopeGeneratorEngine_t;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PatinaEqParams {
    pub enable_low: c_int,
    pub low_freq_hz: c_float,
    pub low_gain_db: c_float,
    pub low_resonance: c_float,
    pub enable_mid: c_int,
    pub mid_freq_hz: c_float,
    pub mid_gain_db: c_float,
    pub mid_q: c_float,
    pub enable_high: c_int,
    pub high_freq_hz: c_float,
    pub high_gain_db: c_float,
    pub high_resonance: c_float,
    pub temperature: c_float,
    pub output_gain_db: c_float,
    pub supply_voltage: f64,
    pub pedal_mode: c_int,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PatinaLimiterParams {
    pub r#type: c_int,          // 0=Fet, 1=Vca, 2=Opto
    pub ceiling: c_float,
    pub attack: c_float,
    pub release: c_float,
    pub output_gain: c_float,
    pub mix: c_float,
    pub pedal_mode: c_int,
    pub supply_voltage: f64,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PatinaFilterParams {
    pub routing: c_int,
    pub filter1_cutoff_hz: c_float,
    pub filter1_resonance: c_float,
    pub filter1_type: c_int,
    pub filter1_slope: c_int,
    pub filter2_cutoff_hz: c_float,
    pub filter2_resonance: c_float,
    pub filter2_type: c_int,
    pub filter2_slope: c_int,
    pub drive1_amount: c_float,
    pub drive1_type: c_int,
    pub drive2_amount: c_float,
    pub drive2_type: c_int,
    pub drive3_amount: c_float,
    pub drive3_type: c_int,
    pub output_level: c_float,
    pub mix: c_float,
    pub temperature: c_float,
    pub supply_voltage: f64,
    pub pedal_mode: c_int,
    pub normalize: c_int,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PatinaEnvelopeGeneratorParams {
    pub attack: c_float,
    pub decay: c_float,
    pub sustain: c_float,
    pub release: c_float,
    pub env_mode: c_int,        // 0=ADSR, 1=AD, 2=AR
    pub curve: c_int,           // 0=RC (exponential), 1=Linear
    pub trigger_mode: c_int,    // 0=External, 1=Auto
    pub auto_threshold_db: c_float,
    pub velocity: c_float,
    pub vca_depth: c_float,
    pub output_gain: c_float,
    pub mix: c_float,
    pub temperature: c_float,
    pub pedal_mode: c_int,
    pub supply_voltage: f64,
}

/* ========================================================================== */
/*  Enum constants                                                            */
/* ========================================================================== */

pub const PATINA_REVERB_SPRING: c_int = 0;
pub const PATINA_REVERB_PLATE: c_int = 1;

pub const PATINA_COMP_PHOTO: c_int = 0;
pub const PATINA_COMP_FET: c_int = 1;
pub const PATINA_COMP_VARIABLE_MU: c_int = 2;

pub const PATINA_MOD_PHASER: c_int = 0;
pub const PATINA_MOD_TREMOLO: c_int = 1;
pub const PATINA_MOD_CHORUS: c_int = 2;

/* ========================================================================== */
/*  Extern declarations                                                       */
/* ========================================================================== */

extern "C" {
    // Version
    pub fn patina_version() -> *const c_char;

    // --- Delay ---
    pub fn patina_delay_default_params() -> PatinaDelayParams;
    pub fn patina_delay_create() -> PatinaDelayEngine;
    pub fn patina_delay_destroy(h: PatinaDelayEngine);
    pub fn patina_delay_prepare(h: PatinaDelayEngine, spec: *const PatinaProcessSpec);
    pub fn patina_delay_reset(h: PatinaDelayEngine);
    pub fn patina_delay_process(
        h: PatinaDelayEngine,
        input: *const *const c_float,
        output: *const *mut c_float,
        num_channels: c_int,
        num_samples: c_int,
        params: *const PatinaDelayParams,
    );

    // --- Drive ---
    pub fn patina_drive_default_params() -> PatinaDriveParams;
    pub fn patina_drive_create() -> PatinaDriveEngine;
    pub fn patina_drive_destroy(h: PatinaDriveEngine);
    pub fn patina_drive_prepare(h: PatinaDriveEngine, spec: *const PatinaProcessSpec);
    pub fn patina_drive_reset(h: PatinaDriveEngine);
    pub fn patina_drive_process(
        h: PatinaDriveEngine,
        input: *const *const c_float,
        output: *const *mut c_float,
        num_channels: c_int,
        num_samples: c_int,
        params: *const PatinaDriveParams,
    );

    // --- Reverb ---
    pub fn patina_reverb_default_params() -> PatinaReverbParams;
    pub fn patina_reverb_create() -> PatinaReverbEngine;
    pub fn patina_reverb_destroy(h: PatinaReverbEngine);
    pub fn patina_reverb_prepare(h: PatinaReverbEngine, spec: *const PatinaProcessSpec);
    pub fn patina_reverb_reset(h: PatinaReverbEngine);
    pub fn patina_reverb_process(
        h: PatinaReverbEngine,
        input: *const *const c_float,
        output: *const *mut c_float,
        num_channels: c_int,
        num_samples: c_int,
        params: *const PatinaReverbParams,
    );

    // --- Compressor ---
    pub fn patina_compressor_default_params() -> PatinaCompressorParams;
    pub fn patina_compressor_create() -> PatinaCompressorEngine;
    pub fn patina_compressor_destroy(h: PatinaCompressorEngine);
    pub fn patina_compressor_prepare(h: PatinaCompressorEngine, spec: *const PatinaProcessSpec);
    pub fn patina_compressor_reset(h: PatinaCompressorEngine);
    pub fn patina_compressor_process(
        h: PatinaCompressorEngine,
        input: *const *const c_float,
        output: *const *mut c_float,
        num_channels: c_int,
        num_samples: c_int,
        params: *const PatinaCompressorParams,
    );
    pub fn patina_compressor_get_gain_reduction_db(h: PatinaCompressorEngine, channel: c_int) -> c_float;
    pub fn patina_compressor_is_gate_open(h: PatinaCompressorEngine, channel: c_int) -> c_int;

    // --- Modulation ---
    pub fn patina_modulation_default_params() -> PatinaModulationParams;
    pub fn patina_modulation_create() -> PatinaModulationEngine;
    pub fn patina_modulation_destroy(h: PatinaModulationEngine);
    pub fn patina_modulation_prepare(h: PatinaModulationEngine, spec: *const PatinaProcessSpec);
    pub fn patina_modulation_reset(h: PatinaModulationEngine);
    pub fn patina_modulation_process(
        h: PatinaModulationEngine,
        input: *const *const c_float,
        output: *const *mut c_float,
        num_channels: c_int,
        num_samples: c_int,
        params: *const PatinaModulationParams,
    );

    // --- Tape ---
    pub fn patina_tape_default_params() -> PatinaTapeParams;
    pub fn patina_tape_create() -> PatinaTapeEngine;
    pub fn patina_tape_destroy(h: PatinaTapeEngine);
    pub fn patina_tape_prepare(h: PatinaTapeEngine, spec: *const PatinaProcessSpec);
    pub fn patina_tape_reset(h: PatinaTapeEngine);
    pub fn patina_tape_process(
        h: PatinaTapeEngine,
        input: *const *const c_float,
        output: *const *mut c_float,
        num_channels: c_int,
        num_samples: c_int,
        params: *const PatinaTapeParams,
    );

    // --- Channel Strip ---
    pub fn patina_channel_strip_default_params() -> PatinaChannelStripParams;
    pub fn patina_channel_strip_create() -> PatinaChannelStrip;
    pub fn patina_channel_strip_destroy(h: PatinaChannelStrip);
    pub fn patina_channel_strip_prepare(h: PatinaChannelStrip, spec: *const PatinaProcessSpec);
    pub fn patina_channel_strip_reset(h: PatinaChannelStrip);
    pub fn patina_channel_strip_process(
        h: PatinaChannelStrip,
        input: *const *const c_float,
        output: *const *mut c_float,
        num_channels: c_int,
        num_samples: c_int,
        params: *const PatinaChannelStripParams,
    );
    pub fn patina_channel_strip_get_output_level(h: PatinaChannelStrip, channel: c_int) -> c_float;
    pub fn patina_channel_strip_is_gate_open(h: PatinaChannelStrip, channel: c_int) -> c_int;

    // --- EQ ---
    pub fn patina_eq_default_params() -> PatinaEqParams;
    pub fn patina_eq_create() -> PatinaEqEngine;
    pub fn patina_eq_destroy(h: PatinaEqEngine);
    pub fn patina_eq_prepare(h: PatinaEqEngine, spec: *const PatinaProcessSpec);
    pub fn patina_eq_reset(h: PatinaEqEngine);
    pub fn patina_eq_process(
        h: PatinaEqEngine,
        input: *const *const c_float,
        output: *const *mut c_float,
        num_channels: c_int,
        num_samples: c_int,
        params: *const PatinaEqParams,
    );

    // --- Limiter ---
    pub fn patina_limiter_default_params() -> PatinaLimiterParams;
    pub fn patina_limiter_create() -> PatinaLimiterEngine;
    pub fn patina_limiter_destroy(h: PatinaLimiterEngine);
    pub fn patina_limiter_prepare(h: PatinaLimiterEngine, spec: *const PatinaProcessSpec);
    pub fn patina_limiter_reset(h: PatinaLimiterEngine);
    pub fn patina_limiter_process(
        h: PatinaLimiterEngine,
        input: *const *const c_float,
        output: *const *mut c_float,
        num_channels: c_int,
        num_samples: c_int,
        params: *const PatinaLimiterParams,
    );
    pub fn patina_limiter_get_gain_reduction_db(h: PatinaLimiterEngine, channel: c_int) -> c_float;

    // --- Filter ---
    pub fn patina_filter_default_params() -> PatinaFilterParams;
    pub fn patina_filter_create() -> PatinaFilterEngine;
    pub fn patina_filter_destroy(h: PatinaFilterEngine);
    pub fn patina_filter_prepare(h: PatinaFilterEngine, spec: *const PatinaProcessSpec);
    pub fn patina_filter_reset(h: PatinaFilterEngine);
    pub fn patina_filter_process(
        h: PatinaFilterEngine,
        input: *const *const c_float,
        output: *const *mut c_float,
        num_channels: c_int,
        num_samples: c_int,
        params: *const PatinaFilterParams,
    );

    // --- Envelope Generator ---
    pub fn patina_envelope_generator_default_params() -> PatinaEnvelopeGeneratorParams;
    pub fn patina_envelope_generator_create() -> PatinaEnvelopeGeneratorEngine;
    pub fn patina_envelope_generator_destroy(h: PatinaEnvelopeGeneratorEngine);
    pub fn patina_envelope_generator_prepare(h: PatinaEnvelopeGeneratorEngine, spec: *const PatinaProcessSpec);
    pub fn patina_envelope_generator_reset(h: PatinaEnvelopeGeneratorEngine);
    pub fn patina_envelope_generator_process(
        h: PatinaEnvelopeGeneratorEngine,
        input: *const *const c_float,
        output: *const *mut c_float,
        num_channels: c_int,
        num_samples: c_int,
        params: *const PatinaEnvelopeGeneratorParams,
    );
    pub fn patina_envelope_generator_get_envelope(h: PatinaEnvelopeGeneratorEngine, channel: c_int) -> c_float;
    pub fn patina_envelope_generator_gate_on(h: PatinaEnvelopeGeneratorEngine);
    pub fn patina_envelope_generator_gate_off(h: PatinaEnvelopeGeneratorEngine);
}
