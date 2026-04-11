//! Safe Rust wrappers for the Patina DSP library.
//!
//! Each engine type owns its C handle and frees it on drop.
//!
//! # Example
//! ```no_run
//! use patina::{ProcessSpec, DriveEngine, DriveParams};
//!
//! let mut engine = DriveEngine::new().unwrap();
//! let spec = ProcessSpec { sample_rate: 48000.0, max_block_size: 256, num_channels: 2 };
//! engine.prepare(&spec);
//!
//! let params = DriveParams::default();
//! let input = vec![vec![0.0f32; 256]; 2];
//! let mut output = vec![vec![0.0f32; 256]; 2];
//! engine.process(&input, &mut output, &params);
//! ```

pub mod ffi;

use std::ffi::CStr;
use std::ptr::NonNull;

// ============================================================================
//  Re-export enum constants
// ============================================================================

pub use ffi::{
    PATINA_COMP_FET, PATINA_COMP_PHOTO, PATINA_COMP_VARIABLE_MU, PATINA_MOD_CHORUS,
    PATINA_MOD_PHASER, PATINA_MOD_TREMOLO, PATINA_REVERB_PLATE, PATINA_REVERB_SPRING,
};

// ============================================================================
//  ProcessSpec
// ============================================================================

#[derive(Debug, Clone, Copy)]
pub struct ProcessSpec {
    pub sample_rate: f64,
    pub max_block_size: i32,
    pub num_channels: i32,
}

impl ProcessSpec {
    fn to_ffi(&self) -> ffi::PatinaProcessSpec {
        ffi::PatinaProcessSpec {
            sample_rate: self.sample_rate,
            max_block_size: self.max_block_size,
            num_channels: self.num_channels,
        }
    }
}

// ============================================================================
//  Version helper
// ============================================================================

pub fn version() -> &'static str {
    unsafe {
        let ptr = ffi::patina_version();
        CStr::from_ptr(ptr).to_str().unwrap_or("unknown")
    }
}

// ============================================================================
//  Audio buffer helpers (channel-pointer layout)
// ============================================================================

fn channel_ptrs(bufs: &[Vec<f32>]) -> Vec<*const f32> {
    bufs.iter().map(|ch| ch.as_ptr()).collect()
}

fn channel_ptrs_mut(bufs: &mut [Vec<f32>]) -> Vec<*mut f32> {
    bufs.iter_mut().map(|ch| ch.as_mut_ptr()).collect()
}

// ============================================================================
//  Macro: generate safe wrapper for an engine
// ============================================================================

macro_rules! engine_wrapper {
    (
        name: $Name:ident,
        params: $Params:ident,
        ffi_params: $FfiParams:ident,
        create: $create:ident,
        destroy: $destroy:ident,
        prepare: $prepare:ident,
        reset: $reset_fn:ident,
        process: $process:ident,
        default_params: $default_params:ident,
        handle_type: $HandleType:ty,
        $(extra_methods: { $($extra:tt)* },)?
    ) => {
        pub type $Params = ffi::$FfiParams;

        impl Default for $Params {
            fn default() -> Self {
                unsafe { ffi::$default_params() }
            }
        }

        pub struct $Name {
            handle: NonNull<ffi::paste_opaque!($HandleType)>,
        }

        // SAFETY: The C++ engines don't use thread-local state.
        // Caller must ensure no concurrent calls on the same handle.
        unsafe impl Send for $Name {}

        impl $Name {
            pub fn new() -> Option<Self> {
                let ptr = unsafe { ffi::$create() };
                NonNull::new(ptr).map(|handle| Self { handle })
            }

            pub fn prepare(&mut self, spec: &ProcessSpec) {
                let s = spec.to_ffi();
                unsafe { ffi::$prepare(self.handle.as_ptr(), &s) };
            }

            pub fn reset(&mut self) {
                unsafe { ffi::$reset_fn(self.handle.as_ptr()) };
            }

            pub fn process(
                &mut self,
                input: &[Vec<f32>],
                output: &mut [Vec<f32>],
                params: &$Params,
            ) {
                let num_channels = input.len().min(output.len()) as i32;
                let num_samples = if num_channels > 0 {
                    input[0].len().min(output[0].len()) as i32
                } else {
                    0
                };
                let in_ptrs = channel_ptrs(input);
                let mut out_ptrs = channel_ptrs_mut(output);
                unsafe {
                    ffi::$process(
                        self.handle.as_ptr(),
                        in_ptrs.as_ptr(),
                        out_ptrs.as_mut_ptr(),
                        num_channels,
                        num_samples,
                        params,
                    );
                }
            }

            $($($extra)*)?
        }

        impl Drop for $Name {
            fn drop(&mut self) {
                unsafe { ffi::$destroy(self.handle.as_ptr()) };
            }
        }
    };
}

// Helper macro for opaque type resolution
macro_rules! paste_opaque {
    (*mut PatinaDelayEngine_t) => { ffi::PatinaDelayEngine_t };
    (*mut PatinaDriveEngine_t) => { ffi::PatinaDriveEngine_t };
    (*mut PatinaReverbEngine_t) => { ffi::PatinaReverbEngine_t };
    (*mut PatinaCompressorEngine_t) => { ffi::PatinaCompressorEngine_t };
    (*mut PatinaModulationEngine_t) => { ffi::PatinaModulationEngine_t };
    (*mut PatinaTapeEngine_t) => { ffi::PatinaTapeEngine_t };
    (*mut PatinaChannelStrip_t) => { ffi::PatinaChannelStrip_t };
    (*mut PatinaEqEngine_t) => { ffi::PatinaEqEngine_t };
    (*mut PatinaLimiterEngine_t) => { ffi::PatinaLimiterEngine_t };
    (*mut PatinaFilterEngine_t) => { ffi::PatinaFilterEngine_t };
    (*mut PatinaEnvelopeGeneratorEngine_t) => { ffi::PatinaEnvelopeGeneratorEngine_t };
}

// ============================================================================
//  Engine wrappers
// ============================================================================

engine_wrapper! {
    name: BbdDelayEngine,
    params: DelayParams,
    ffi_params: PatinaDelayParams,
    create: patina_delay_create,
    destroy: patina_delay_destroy,
    prepare: patina_delay_prepare,
    reset: patina_delay_reset,
    process: patina_delay_process,
    default_params: patina_delay_default_params,
    handle_type: *mut PatinaDelayEngine_t,
}

engine_wrapper! {
    name: DriveEngine,
    params: DriveParams,
    ffi_params: PatinaDriveParams,
    create: patina_drive_create,
    destroy: patina_drive_destroy,
    prepare: patina_drive_prepare,
    reset: patina_drive_reset,
    process: patina_drive_process,
    default_params: patina_drive_default_params,
    handle_type: *mut PatinaDriveEngine_t,
}

engine_wrapper! {
    name: ReverbEngine,
    params: ReverbParams,
    ffi_params: PatinaReverbParams,
    create: patina_reverb_create,
    destroy: patina_reverb_destroy,
    prepare: patina_reverb_prepare,
    reset: patina_reverb_reset,
    process: patina_reverb_process,
    default_params: patina_reverb_default_params,
    handle_type: *mut PatinaReverbEngine_t,
}

engine_wrapper! {
    name: CompressorEngine,
    params: CompressorParams,
    ffi_params: PatinaCompressorParams,
    create: patina_compressor_create,
    destroy: patina_compressor_destroy,
    prepare: patina_compressor_prepare,
    reset: patina_compressor_reset,
    process: patina_compressor_process,
    default_params: patina_compressor_default_params,
    handle_type: *mut PatinaCompressorEngine_t,
    extra_methods: {
        pub fn gain_reduction_db(&self, channel: i32) -> f32 {
            unsafe { ffi::patina_compressor_get_gain_reduction_db(self.handle.as_ptr(), channel) }
        }
        pub fn is_gate_open(&self, channel: i32) -> bool {
            unsafe { ffi::patina_compressor_is_gate_open(self.handle.as_ptr(), channel) != 0 }
        }
    },
}

engine_wrapper! {
    name: ModulationEngine,
    params: ModulationParams,
    ffi_params: PatinaModulationParams,
    create: patina_modulation_create,
    destroy: patina_modulation_destroy,
    prepare: patina_modulation_prepare,
    reset: patina_modulation_reset,
    process: patina_modulation_process,
    default_params: patina_modulation_default_params,
    handle_type: *mut PatinaModulationEngine_t,
}

engine_wrapper! {
    name: TapeMachineEngine,
    params: TapeParams,
    ffi_params: PatinaTapeParams,
    create: patina_tape_create,
    destroy: patina_tape_destroy,
    prepare: patina_tape_prepare,
    reset: patina_tape_reset,
    process: patina_tape_process,
    default_params: patina_tape_default_params,
    handle_type: *mut PatinaTapeEngine_t,
}

engine_wrapper! {
    name: ChannelStripEngine,
    params: ChannelStripParams,
    ffi_params: PatinaChannelStripParams,
    create: patina_channel_strip_create,
    destroy: patina_channel_strip_destroy,
    prepare: patina_channel_strip_prepare,
    reset: patina_channel_strip_reset,
    process: patina_channel_strip_process,
    default_params: patina_channel_strip_default_params,
    handle_type: *mut PatinaChannelStrip_t,
    extra_methods: {
        pub fn output_level(&self, channel: i32) -> f32 {
            unsafe { ffi::patina_channel_strip_get_output_level(self.handle.as_ptr(), channel) }
        }
        pub fn is_gate_open(&self, channel: i32) -> bool {
            unsafe { ffi::patina_channel_strip_is_gate_open(self.handle.as_ptr(), channel) != 0 }
        }
    },
}

engine_wrapper! {
    name: EqEngine,
    params: EqParams,
    ffi_params: PatinaEqParams,
    create: patina_eq_create,
    destroy: patina_eq_destroy,
    prepare: patina_eq_prepare,
    reset: patina_eq_reset,
    process: patina_eq_process,
    default_params: patina_eq_default_params,
    handle_type: *mut PatinaEqEngine_t,
}

engine_wrapper! {
    name: LimiterEngine,
    params: LimiterParams,
    ffi_params: PatinaLimiterParams,
    create: patina_limiter_create,
    destroy: patina_limiter_destroy,
    prepare: patina_limiter_prepare,
    reset: patina_limiter_reset,
    process: patina_limiter_process,
    default_params: patina_limiter_default_params,
    handle_type: *mut PatinaLimiterEngine_t,
    extra_methods: {
        pub fn gain_reduction_db(&self, channel: i32) -> f32 {
            unsafe { ffi::patina_limiter_get_gain_reduction_db(self.handle.as_ptr(), channel) }
        }
    },
}

engine_wrapper! {
    name: FilterEngine,
    params: FilterParams,
    ffi_params: PatinaFilterParams,
    create: patina_filter_create,
    destroy: patina_filter_destroy,
    prepare: patina_filter_prepare,
    reset: patina_filter_reset,
    process: patina_filter_process,
    default_params: patina_filter_default_params,
    handle_type: *mut PatinaFilterEngine_t,
}

engine_wrapper! {
    name: EnvelopeGeneratorEngine,
    params: EnvelopeGeneratorParams,
    ffi_params: PatinaEnvelopeGeneratorParams,
    create: patina_envelope_generator_create,
    destroy: patina_envelope_generator_destroy,
    prepare: patina_envelope_generator_prepare,
    reset: patina_envelope_generator_reset,
    process: patina_envelope_generator_process,
    default_params: patina_envelope_generator_default_params,
    handle_type: *mut PatinaEnvelopeGeneratorEngine_t,
    extra_methods: {
        pub fn get_envelope(&self, channel: i32) -> f32 {
            unsafe { ffi::patina_envelope_generator_get_envelope(self.handle.as_ptr(), channel) }
        }
        pub fn gate_on(&mut self) {
            unsafe { ffi::patina_envelope_generator_gate_on(self.handle.as_ptr()) };
        }
        pub fn gate_off(&mut self) {
            unsafe { ffi::patina_envelope_generator_gate_off(self.handle.as_ptr()) };
        }
    },
}

// ============================================================================
//  Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_version() {
        let v = version();
        assert!(v.starts_with("1."));
    }

    #[test]
    fn test_default_params() {
        let p = DelayParams::default();
        assert!((p.delay_ms - 250.0).abs() < f32::EPSILON);

        let p = DriveParams::default();
        assert!((p.drive - 0.5).abs() < f32::EPSILON);

        let p = ReverbParams::default();
        assert_eq!(p.r#type, PATINA_REVERB_SPRING);

        let p = CompressorParams::default();
        assert_eq!(p.r#type, PATINA_COMP_FET);

        let p = ModulationParams::default();
        assert_eq!(p.r#type, PATINA_MOD_PHASER);
    }
}
