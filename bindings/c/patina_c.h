/*
  ==============================================================================

    patina_c.h — Patina DSP Library C API
    Created: 4 Apr 2026

    Pure C interface using opaque handles.
    All engines share the same pattern:
      patina_<engine>_create  → allocate
      patina_<engine>_destroy → deallocate
      patina_<engine>_prepare → set sample rate / block size
      patina_<engine>_reset   → clear internal state
      patina_<engine>_process → process audio block
    Thread-safety: one handle per thread, or use external sync.
    Lifecycle: after patina_<engine>_destroy(), the handle is invalid.
              Set your local handle variable to NULL after calling destroy.

  ==============================================================================
*/

#ifndef PATINA_C_H
#define PATINA_C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*  Build / linkage                                                           */
/* ========================================================================== */

#if defined(_WIN32) && defined(PATINA_SHARED)
  #ifdef PATINA_EXPORT
    #define PATINA_API __declspec(dllexport)
  #else
    #define PATINA_API __declspec(dllimport)
  #endif
#elif defined(__GNUC__) && defined(PATINA_SHARED)
  #define PATINA_API __attribute__((visibility("default")))
#else
  #define PATINA_API
#endif

/* ========================================================================== */
/*  Common types                                                              */
/* ========================================================================== */

typedef struct {
    double sample_rate;       /* Hz (e.g. 44100.0, 48000.0) */
    int    max_block_size;    /* samples per block           */
    int    num_channels;      /* 1=mono, 2=stereo            */
} PatinaProcessSpec;

/* ========================================================================== */
/*  Opaque handles                                                            */
/* ========================================================================== */

typedef struct PatinaDelayEngine_t*       PatinaDelayEngine;
typedef struct PatinaDriveEngine_t*       PatinaDriveEngine;
typedef struct PatinaReverbEngine_t*      PatinaReverbEngine;
typedef struct PatinaCompressorEngine_t*  PatinaCompressorEngine;
typedef struct PatinaModulationEngine_t*  PatinaModulationEngine;
typedef struct PatinaTapeEngine_t*        PatinaTapeEngine;
typedef struct PatinaChannelStrip_t*      PatinaChannelStrip;
typedef struct PatinaEqEngine_t*              PatinaEqEngine;
typedef struct PatinaLimiterEngine_t*         PatinaLimiterEngine;
typedef struct PatinaFilterEngine_t*          PatinaFilterEngine;
typedef struct PatinaEnvelopeGeneratorEngine_t* PatinaEnvelopeGeneratorEngine;

/* ========================================================================== */
/*  BBD Delay Engine                                                          */
/* ========================================================================== */

typedef struct {
    float  delay_ms;            /* default 250 */
    float  feedback;            /* 0.0–1.0     */
    float  tone;                /* 0.0–1.0     */
    float  mix;                 /* 0.0–1.0     */
    float  comp_amount;         /* 0.0–1.0     */
    float  chorus_depth;        /* 0.0–1.0     */
    float  lfo_rate_hz;         /* Hz          */
    double supply_voltage;      /* V           */
    int    bbd_stages;          /* e.g. 512    */
    int    emulate_bbd;         /* bool        */
    int    emulate_opamp_sat;   /* bool        */
    int    emulate_tone_rc;     /* bool        */
    int    enable_aging;        /* bool        */
    double age_years;
    double capacitance_scale;
    int    pedal_mode;          /* bool: 0=outboard, 1=pedal */
} PatinaDelayParams;

PATINA_API PatinaDelayParams patina_delay_default_params(void);

PATINA_API PatinaDelayEngine patina_delay_create(void);
PATINA_API void patina_delay_destroy(PatinaDelayEngine h);
PATINA_API void patina_delay_prepare(PatinaDelayEngine h, const PatinaProcessSpec* spec);
PATINA_API void patina_delay_reset(PatinaDelayEngine h);
PATINA_API void patina_delay_process(PatinaDelayEngine h,
                                     const float* const* input,
                                     float* const* output,
                                     int num_channels, int num_samples,
                                     const PatinaDelayParams* params);

/* ========================================================================== */
/*  Drive Engine                                                              */
/* ========================================================================== */

typedef struct {
    float  drive;              /* 0.0–1.0 */
    int    clipping_mode;      /* 0=Bypass, 1=Diode, 2=Tanh */
    int    diode_type;         /* 0=Si, 1=Schottky, 2=Ge */
    float  tone;               /* 0.0–1.0 */
    float  output_level;       /* 0.0–1.0 */
    float  mix;                /* 0.0–1.0 */
    double supply_voltage;     /* V */
    float  temperature;        /* °C */
    int    enable_power_sag;   /* bool */
    float  sag_amount;         /* 0.0–1.0 */
    int    pedal_mode;          /* bool: 0=outboard, 1=pedal */
} PatinaDriveParams;

PATINA_API PatinaDriveParams patina_drive_default_params(void);

PATINA_API PatinaDriveEngine patina_drive_create(void);
PATINA_API void patina_drive_destroy(PatinaDriveEngine h);
PATINA_API void patina_drive_prepare(PatinaDriveEngine h, const PatinaProcessSpec* spec);
PATINA_API void patina_drive_reset(PatinaDriveEngine h);
PATINA_API void patina_drive_process(PatinaDriveEngine h,
                                     const float* const* input,
                                     float* const* output,
                                     int num_channels, int num_samples,
                                     const PatinaDriveParams* params);

/* ========================================================================== */
/*  Reverb Engine                                                             */
/* ========================================================================== */

enum {
    PATINA_REVERB_SPRING = 0,
    PATINA_REVERB_PLATE  = 1
};

typedef struct {
    int    type;               /* PATINA_REVERB_SPRING / PATINA_REVERB_PLATE */
    float  decay;              /* 0.0–1.0 */
    float  tone;               /* 0.0–1.0 */
    float  mix;                /* 0.0–1.0 */
    double supply_voltage;     /* V */
    /* Spring-specific */
    float  tension;            /* 0.0–1.0 */
    float  drip_amount;        /* 0.0–1.0 */
    int    num_springs;        /* 1–3 */
    /* Plate-specific */
    float  predelay_ms;        /* ms */
    float  damping;            /* 0.0–1.0 */
    float  diffusion;          /* 0.0–1.0 */
    float  mod_depth;          /* 0.0–1.0 */
    int    pedal_mode;          /* bool: 0=outboard, 1=pedal */
} PatinaReverbParams;

PATINA_API PatinaReverbParams patina_reverb_default_params(void);

PATINA_API PatinaReverbEngine patina_reverb_create(void);
PATINA_API void patina_reverb_destroy(PatinaReverbEngine h);
PATINA_API void patina_reverb_prepare(PatinaReverbEngine h, const PatinaProcessSpec* spec);
PATINA_API void patina_reverb_reset(PatinaReverbEngine h);
PATINA_API void patina_reverb_process(PatinaReverbEngine h,
                                      const float* const* input,
                                      float* const* output,
                                      int num_channels, int num_samples,
                                      const PatinaReverbParams* params);

/* ========================================================================== */
/*  Compressor Engine                                                         */
/* ========================================================================== */

enum {
    PATINA_COMP_PHOTO       = 0,
    PATINA_COMP_FET         = 1,
    PATINA_COMP_VARIABLE_MU = 2
};

typedef struct {
    int    type;               /* PATINA_COMP_PHOTO / FET / VARIABLE_MU */
    float  input_gain;         /* 0.0–1.0 */
    float  threshold;          /* 0.0–1.0 */
    float  output_gain;        /* 0.0–1.0 */
    float  attack;             /* 0.0–1.0 (FET only) */
    float  release;            /* 0.0–1.0 (FET only) */
    int    ratio;              /* FET: 0–4, VariableMu: 0–5 */
    float  mix;                /* 0.0–1.0 */
    double supply_voltage;     /* V */
    int    enable_gate;        /* bool */
    float  gate_threshold_db;  /* dBFS */
    int    photo_mode;         /* 0=Compress, 1=Limit */
    int    pedal_mode;          /* bool: 0=outboard, 1=pedal */
} PatinaCompressorParams;

PATINA_API PatinaCompressorParams patina_compressor_default_params(void);

PATINA_API PatinaCompressorEngine patina_compressor_create(void);
PATINA_API void patina_compressor_destroy(PatinaCompressorEngine h);
PATINA_API void patina_compressor_prepare(PatinaCompressorEngine h, const PatinaProcessSpec* spec);
PATINA_API void patina_compressor_reset(PatinaCompressorEngine h);
PATINA_API void patina_compressor_process(PatinaCompressorEngine h,
                                          const float* const* input,
                                          float* const* output,
                                          int num_channels, int num_samples,
                                          const PatinaCompressorParams* params);
PATINA_API float patina_compressor_get_gain_reduction_db(PatinaCompressorEngine h, int channel);
PATINA_API int   patina_compressor_is_gate_open(PatinaCompressorEngine h, int channel);

/* ========================================================================== */
/*  Modulation Engine                                                         */
/* ========================================================================== */

enum {
    PATINA_MOD_PHASER  = 0,
    PATINA_MOD_TREMOLO = 1,
    PATINA_MOD_CHORUS  = 2
};

typedef struct {
    int    type;               /* PATINA_MOD_PHASER / TREMOLO / CHORUS */
    float  rate;               /* LFO rate Hz, 0.1–10.0 */
    float  depth;              /* 0.0–1.0 */
    float  feedback;           /* 0.0–0.95 (Phaser) */
    float  mix;                /* 0.0–1.0 */
    double supply_voltage;     /* V */
    /* Phaser-specific */
    float  center_freq_hz;     /* Hz */
    float  freq_spread_hz;     /* Hz */
    int    num_stages;         /* 2–12 */
    float  temperature;        /* °C */
    /* Tremolo-specific */
    int    tremolo_mode;       /* 0=Bias, 1=Optical, 2=VCA */
    int    stereo_phase_invert; /* bool */
    /* Chorus-specific */
    float  chorus_delay_ms;    /* ms */
    float  stereo_width;       /* 0.0–1.0 */
    int    pedal_mode;          /* bool: 0=outboard, 1=pedal */
} PatinaModulationParams;

PATINA_API PatinaModulationParams patina_modulation_default_params(void);

PATINA_API PatinaModulationEngine patina_modulation_create(void);
PATINA_API void patina_modulation_destroy(PatinaModulationEngine h);
PATINA_API void patina_modulation_prepare(PatinaModulationEngine h, const PatinaProcessSpec* spec);
PATINA_API void patina_modulation_reset(PatinaModulationEngine h);
PATINA_API void patina_modulation_process(PatinaModulationEngine h,
                                          const float* const* input,
                                          float* const* output,
                                          int num_channels, int num_samples,
                                          const PatinaModulationParams* params);

/* ========================================================================== */
/*  Tape Machine Engine                                                       */
/* ========================================================================== */

typedef struct {
    float  input_gain;         /* dB-like, 0.0=unity */
    float  saturation;         /* 0.0–1.0 */
    float  bias_amount;        /* 0.0–1.0 */
    float  tape_speed;         /* 0.5=7.5ips, 1.0=15ips, 2.0=30ips */
    float  wow_flutter;        /* 0.0–1.0 */
    int    enable_head_bump;   /* bool */
    int    enable_hf_rolloff;  /* bool */
    float  head_wear;          /* 0.0–1.0 */
    float  tape_age;           /* 0.0–1.0 */
    int    enable_transformer; /* bool */
    float  transformer_drive;  /* dB */
    float  transformer_sat;    /* 0.0–1.0 */
    float  tone;               /* 0.0–1.0 */
    float  mix;                /* 0.0–1.0 */
    double supply_voltage;     /* V */
    int    pedal_mode;          /* bool: 0=outboard, 1=pedal */
} PatinaTapeParams;

PATINA_API PatinaTapeParams patina_tape_default_params(void);

PATINA_API PatinaTapeEngine patina_tape_create(void);
PATINA_API void patina_tape_destroy(PatinaTapeEngine h);
PATINA_API void patina_tape_prepare(PatinaTapeEngine h, const PatinaProcessSpec* spec);
PATINA_API void patina_tape_reset(PatinaTapeEngine h);
PATINA_API void patina_tape_process(PatinaTapeEngine h,
                                    const float* const* input,
                                    float* const* output,
                                    int num_channels, int num_samples,
                                    const PatinaTapeParams* params);

/* ========================================================================== */
/*  Channel Strip Engine                                                      */
/* ========================================================================== */

typedef struct {
    /* Preamp */
    float  preamp_drive;       /* 0.0–1.0 */
    float  preamp_bias;        /* 0.0–1.0 */
    float  preamp_output;      /* 0.0–1.0 */
    float  tube_age;           /* 0.0–1.0 */
    /* EQ */
    int    enable_eq;          /* bool */
    float  eq_cutoff_hz;       /* Hz */
    float  eq_resonance;       /* 0.0–1.0 */
    int    eq_type;            /* 0=LP, 1=HP, 2=BP, 3=Notch */
    float  eq_temperature;     /* °C */
    /* Noise gate */
    int    enable_gate;        /* bool */
    float  gate_threshold_db;  /* dBFS */
    float  gate_hysteresis_db;
    /* Trim */
    float  input_trim_db;      /* dB */
    float  output_trim_db;     /* dB */
    /* Output */
    double supply_voltage;     /* V */
    int    pedal_mode;          /* bool: 0=outboard, 1=pedal */
} PatinaChannelStripParams;

PATINA_API PatinaChannelStripParams patina_channel_strip_default_params(void);

PATINA_API PatinaChannelStrip patina_channel_strip_create(void);
PATINA_API void patina_channel_strip_destroy(PatinaChannelStrip h);
PATINA_API void patina_channel_strip_prepare(PatinaChannelStrip h, const PatinaProcessSpec* spec);
PATINA_API void patina_channel_strip_reset(PatinaChannelStrip h);
PATINA_API void patina_channel_strip_process(PatinaChannelStrip h,
                                             const float* const* input,
                                             float* const* output,
                                             int num_channels, int num_samples,
                                             const PatinaChannelStripParams* params);
PATINA_API float patina_channel_strip_get_output_level(PatinaChannelStrip h, int channel);
PATINA_API int   patina_channel_strip_is_gate_open(PatinaChannelStrip h, int channel);

/* ========================================================================== */
/*  EQ Engine                                                                 */
/* ========================================================================== */

typedef struct {
    int    enable_low;         /* bool */
    float  low_freq_hz;        /* Hz, default 200 */
    float  low_gain_db;        /* -12 ~ +12 dB */
    float  low_resonance;      /* 0.0–1.0 */
    int    enable_mid;         /* bool */
    float  mid_freq_hz;        /* Hz, default 1000 */
    float  mid_gain_db;        /* -12 ~ +12 dB */
    float  mid_q;              /* 0.1–1.0 */
    int    enable_high;        /* bool */
    float  high_freq_hz;       /* Hz, default 4000 */
    float  high_gain_db;       /* -12 ~ +12 dB */
    float  high_resonance;     /* 0.0–1.0 */
    float  temperature;        /* °C */
    float  output_gain_db;     /* -12 ~ +12 dB */
    double supply_voltage;     /* V */
    int    pedal_mode;         /* bool: 0=outboard, 1=pedal */
} PatinaEqParams;

PATINA_API PatinaEqParams patina_eq_default_params(void);

PATINA_API PatinaEqEngine patina_eq_create(void);
PATINA_API void patina_eq_destroy(PatinaEqEngine h);
PATINA_API void patina_eq_prepare(PatinaEqEngine h, const PatinaProcessSpec* spec);
PATINA_API void patina_eq_reset(PatinaEqEngine h);
PATINA_API void patina_eq_process(PatinaEqEngine h,
                                  const float* const* input,
                                  float* const* output,
                                  int num_channels, int num_samples,
                                  const PatinaEqParams* params);

/* ========================================================================== */
/*  Limiter Engine                                                            */
/* ========================================================================== */

enum {
    PATINA_LIM_FET  = 0,
    PATINA_LIM_VCA  = 1,
    PATINA_LIM_OPTO = 2
};

typedef struct {
    int    type;               /* PATINA_LIM_FET / VCA / OPTO */
    float  ceiling;            /* 0.0–1.0 */
    float  attack;             /* 0.0–1.0 */
    float  release;            /* 0.0–1.0 */
    float  output_gain;        /* 0.0–1.0 */
    float  mix;                /* 0.0–1.0 */
    int    pedal_mode;         /* bool: 0=outboard, 1=pedal */
    double supply_voltage;     /* V */
} PatinaLimiterParams;

PATINA_API PatinaLimiterParams patina_limiter_default_params(void);

PATINA_API PatinaLimiterEngine patina_limiter_create(void);
PATINA_API void patina_limiter_destroy(PatinaLimiterEngine h);
PATINA_API void patina_limiter_prepare(PatinaLimiterEngine h, const PatinaProcessSpec* spec);
PATINA_API void patina_limiter_reset(PatinaLimiterEngine h);
PATINA_API void patina_limiter_process(PatinaLimiterEngine h,
                                       const float* const* input,
                                       float* const* output,
                                       int num_channels, int num_samples,
                                       const PatinaLimiterParams* params);
PATINA_API float patina_limiter_get_gain_reduction_db(PatinaLimiterEngine h, int channel);

/* ========================================================================== */
/*  Filter Engine                                                             */
/* ========================================================================== */

enum {
    PATINA_FILTER_LP     = 0,
    PATINA_FILTER_HP     = 1,
    PATINA_FILTER_BP     = 2,
    PATINA_FILTER_LADDER = 3
};

enum {
    PATINA_FILTER_6DB  = 0,
    PATINA_FILTER_12DB = 1,
    PATINA_FILTER_18DB = 2,
    PATINA_FILTER_24DB = 3
};

enum {
    PATINA_DRIVE_TUBE  = 0,
    PATINA_DRIVE_DIODE = 1,
    PATINA_DRIVE_WAVE  = 2,
    PATINA_DRIVE_TAPE  = 3
};

typedef struct {
    int    routing;               /* 0=Serial, 1=Parallel */
    float  filter1_cutoff_hz;     /* Hz */
    float  filter1_resonance;     /* 0.0–1.0 */
    int    filter1_type;          /* PATINA_FILTER_LP/HP/BP/LADDER */
    int    filter1_slope;         /* PATINA_FILTER_6DB/12DB/18DB/24DB */
    float  filter2_cutoff_hz;     /* Hz */
    float  filter2_resonance;     /* 0.0–1.0 */
    int    filter2_type;
    int    filter2_slope;
    float  drive1_amount;         /* 0.0–1.0 */
    int    drive1_type;           /* PATINA_DRIVE_TUBE/DIODE/WAVE/TAPE */
    float  drive2_amount;         /* 0.0–1.0 (Serial only) */
    int    drive2_type;
    float  drive3_amount;         /* 0.0–1.0 */
    int    drive3_type;
    float  output_level;          /* 0.0–1.0 */
    float  mix;                   /* 0.0–1.0 */
    float  temperature;           /* °C */
    double supply_voltage;        /* V */
    int    pedal_mode;            /* bool: 0=outboard, 1=pedal */
    int    normalize;             /* bool: 1=filter gain compensation ON */
} PatinaFilterParams;

PATINA_API PatinaFilterParams patina_filter_default_params(void);

PATINA_API PatinaFilterEngine patina_filter_create(void);
PATINA_API void patina_filter_destroy(PatinaFilterEngine h);
PATINA_API void patina_filter_prepare(PatinaFilterEngine h, const PatinaProcessSpec* spec);
PATINA_API void patina_filter_reset(PatinaFilterEngine h);
PATINA_API void patina_filter_process(PatinaFilterEngine h,
                                      const float* const* input,
                                      float* const* output,
                                      int num_channels, int num_samples,
                                      const PatinaFilterParams* params);

/* ========================================================================== */
/*  Envelope Generator Engine                                                 */
/* ========================================================================== */

enum {
    PATINA_ENV_MODE_ADSR = 0,
    PATINA_ENV_MODE_AD   = 1,
    PATINA_ENV_MODE_AR   = 2
};

enum {
    PATINA_ENV_CURVE_RC     = 0,
    PATINA_ENV_CURVE_LINEAR = 1
};

enum {
    PATINA_ENV_TRIGGER_EXTERNAL = 0,
    PATINA_ENV_TRIGGER_AUTO     = 1
};

typedef struct {
    float  attack;              /* 0.0–1.0 */
    float  decay;               /* 0.0–1.0 */
    float  sustain;             /* 0.0–1.0 */
    float  release;             /* 0.0–1.0 */
    int    env_mode;            /* PATINA_ENV_MODE_ADSR/AD/AR */
    int    curve;               /* PATINA_ENV_CURVE_RC/LINEAR */
    int    trigger_mode;        /* PATINA_ENV_TRIGGER_EXTERNAL/AUTO */
    float  auto_threshold_db;   /* dBFS */
    float  velocity;            /* 0.0–1.0 */
    float  vca_depth;           /* 0.0–1.0 */
    float  output_gain;         /* 0.0–1.0 */
    float  mix;                 /* 0.0–1.0 */
    float  temperature;         /* °C */
    int    pedal_mode;          /* bool: 0=outboard, 1=pedal */
    double supply_voltage;      /* V */
} PatinaEnvelopeGeneratorParams;

PATINA_API PatinaEnvelopeGeneratorParams patina_envelope_generator_default_params(void);

PATINA_API PatinaEnvelopeGeneratorEngine patina_envelope_generator_create(void);
PATINA_API void patina_envelope_generator_destroy(PatinaEnvelopeGeneratorEngine h);
PATINA_API void patina_envelope_generator_prepare(PatinaEnvelopeGeneratorEngine h, const PatinaProcessSpec* spec);
PATINA_API void patina_envelope_generator_reset(PatinaEnvelopeGeneratorEngine h);
PATINA_API void patina_envelope_generator_process(PatinaEnvelopeGeneratorEngine h,
                                                   const float* const* input,
                                                   float* const* output,
                                                   int num_channels, int num_samples,
                                                   const PatinaEnvelopeGeneratorParams* params);
PATINA_API float patina_envelope_generator_get_envelope(PatinaEnvelopeGeneratorEngine h, int channel);
PATINA_API void  patina_envelope_generator_gate_on(PatinaEnvelopeGeneratorEngine h);
PATINA_API void  patina_envelope_generator_gate_off(PatinaEnvelopeGeneratorEngine h);

/* ========================================================================== */
/*  Version                                                                   */
/* ========================================================================== */

PATINA_API const char* patina_version(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PATINA_C_H */
