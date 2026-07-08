/*
  ==============================================================================

    patina_pd.cpp - Pure Data external for the Patina C API

    Object: [patina~ <engine>]
    Aliases: [patina-drive~], [patina-filter~], [patina-delay~], ...
    Engines: drive, filter, delay, reverb, compressor, modulation, tape, eq,
             limiter, channelstrip, envelope

  ==============================================================================
*/

#include "m_pd.h"
#include "patina_c.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {

enum class Engine {
    delay,
    drive,
    reverb,
    compressor,
    modulation,
    tape,
    channelStrip,
    eq,
    limiter,
    filter,
    envelope
};

struct PatinaPd {
    t_object object;
    t_float signalInlet = 0;
    t_outlet* outlet = nullptr;
    Engine engine = Engine::drive;
    double sampleRate = 44100.0;
    int blockSize = 64;
    std::vector<float> input;
    std::vector<float> output;

    PatinaDelayEngine delay = nullptr;
    PatinaDriveEngine drive = nullptr;
    PatinaReverbEngine reverb = nullptr;
    PatinaCompressorEngine compressor = nullptr;
    PatinaModulationEngine modulation = nullptr;
    PatinaTapeEngine tape = nullptr;
    PatinaChannelStrip channelStrip = nullptr;
    PatinaEqEngine eq = nullptr;
    PatinaLimiterEngine limiter = nullptr;
    PatinaFilterEngine filter = nullptr;
    PatinaEnvelopeGeneratorEngine envelope = nullptr;

    PatinaDelayParams delayParams{};
    PatinaDriveParams driveParams{};
    PatinaReverbParams reverbParams{};
    PatinaCompressorParams compressorParams{};
    PatinaModulationParams modulationParams{};
    PatinaTapeParams tapeParams{};
    PatinaChannelStripParams channelStripParams{};
    PatinaEqParams eqParams{};
    PatinaLimiterParams limiterParams{};
    PatinaFilterParams filterParams{};
    PatinaEnvelopeGeneratorParams envelopeParams{};
};

t_class* patinaClass = nullptr;
t_class* patinaDriveClass = nullptr;
t_class* patinaFilterClass = nullptr;
t_class* patinaDelayClass = nullptr;
t_class* patinaReverbClass = nullptr;
t_class* patinaCompressorClass = nullptr;
t_class* patinaModulationClass = nullptr;
t_class* patinaTapeClass = nullptr;
t_class* patinaEqClass = nullptr;
t_class* patinaLimiterClass = nullptr;
t_class* patinaChannelStripClass = nullptr;
t_class* patinaEnvelopeClass = nullptr;

bool symbolEquals(t_symbol* s, const char* text)
{
    return s && std::strcmp(s->s_name, text) == 0;
}

Engine parseEngine(t_symbol* s)
{
    if (symbolEquals(s, "delay")) return Engine::delay;
    if (symbolEquals(s, "reverb")) return Engine::reverb;
    if (symbolEquals(s, "compressor")) return Engine::compressor;
    if (symbolEquals(s, "modulation")) return Engine::modulation;
    if (symbolEquals(s, "tape")) return Engine::tape;
    if (symbolEquals(s, "channelstrip") || symbolEquals(s, "channel_strip")) return Engine::channelStrip;
    if (symbolEquals(s, "eq")) return Engine::eq;
    if (symbolEquals(s, "limiter")) return Engine::limiter;
    if (symbolEquals(s, "filter")) return Engine::filter;
    if (symbolEquals(s, "envelope")) return Engine::envelope;
    return Engine::drive;
}

void prepare(PatinaPd* x)
{
    PatinaProcessSpec spec { x->sampleRate, x->blockSize, 1 };

    switch (x->engine) {
        case Engine::delay: patina_delay_prepare(x->delay, &spec); break;
        case Engine::drive: patina_drive_prepare(x->drive, &spec); break;
        case Engine::reverb: patina_reverb_prepare(x->reverb, &spec); break;
        case Engine::compressor: patina_compressor_prepare(x->compressor, &spec); break;
        case Engine::modulation: patina_modulation_prepare(x->modulation, &spec); break;
        case Engine::tape: patina_tape_prepare(x->tape, &spec); break;
        case Engine::channelStrip: patina_channel_strip_prepare(x->channelStrip, &spec); break;
        case Engine::eq: patina_eq_prepare(x->eq, &spec); break;
        case Engine::limiter: patina_limiter_prepare(x->limiter, &spec); break;
        case Engine::filter: patina_filter_prepare(x->filter, &spec); break;
        case Engine::envelope: patina_envelope_generator_prepare(x->envelope, &spec); break;
    }
}

void reset(PatinaPd* x)
{
    switch (x->engine) {
        case Engine::delay: patina_delay_reset(x->delay); break;
        case Engine::drive: patina_drive_reset(x->drive); break;
        case Engine::reverb: patina_reverb_reset(x->reverb); break;
        case Engine::compressor: patina_compressor_reset(x->compressor); break;
        case Engine::modulation: patina_modulation_reset(x->modulation); break;
        case Engine::tape: patina_tape_reset(x->tape); break;
        case Engine::channelStrip: patina_channel_strip_reset(x->channelStrip); break;
        case Engine::eq: patina_eq_reset(x->eq); break;
        case Engine::limiter: patina_limiter_reset(x->limiter); break;
        case Engine::filter: patina_filter_reset(x->filter); break;
        case Engine::envelope: patina_envelope_generator_reset(x->envelope); break;
    }
}

template <typename T>
bool setIf(const char* name, const char* target, T& field, float value)
{
    if (std::strcmp(name, target) != 0) return false;
    field = static_cast<T>(value);
    return true;
}

bool setDrive(PatinaDriveParams& p, const char* name, float value)
{
    return setIf(name, "drive", p.drive, value)
        || setIf(name, "tone", p.tone, value)
        || setIf(name, "output", p.output_level, value)
        || setIf(name, "output_level", p.output_level, value)
        || setIf(name, "mix", p.mix, value)
        || setIf(name, "temperature", p.temperature, value)
        || setIf(name, "sag", p.sag_amount, value)
        || setIf(name, "sag_amount", p.sag_amount, value)
        || setIf(name, "clipping", p.clipping_mode, value)
        || setIf(name, "clipping_mode", p.clipping_mode, value)
        || setIf(name, "diode", p.diode_type, value)
        || setIf(name, "diode_type", p.diode_type, value)
        || setIf(name, "power_sag", p.enable_power_sag, value)
        || setIf(name, "pedal_mode", p.pedal_mode, value)
        || setIf(name, "supply", p.supply_voltage, value)
        || setIf(name, "supply_voltage", p.supply_voltage, value);
}

bool setFilter(PatinaFilterParams& p, const char* name, float value)
{
    return setIf(name, "cutoff", p.filter1_cutoff_hz, value)
        || setIf(name, "cutoff1", p.filter1_cutoff_hz, value)
        || setIf(name, "resonance", p.filter1_resonance, value)
        || setIf(name, "resonance1", p.filter1_resonance, value)
        || setIf(name, "type", p.filter1_type, value)
        || setIf(name, "slope", p.filter1_slope, value)
        || setIf(name, "cutoff2", p.filter2_cutoff_hz, value)
        || setIf(name, "resonance2", p.filter2_resonance, value)
        || setIf(name, "type2", p.filter2_type, value)
        || setIf(name, "slope2", p.filter2_slope, value)
        || setIf(name, "drive", p.drive1_amount, value)
        || setIf(name, "drive1", p.drive1_amount, value)
        || setIf(name, "drive2", p.drive2_amount, value)
        || setIf(name, "drive3", p.drive3_amount, value)
        || setIf(name, "output", p.output_level, value)
        || setIf(name, "mix", p.mix, value)
        || setIf(name, "normalize", p.normalize, value)
        || setIf(name, "routing", p.routing, value);
}

bool setDelay(PatinaDelayParams& p, const char* name, float value)
{
    return setIf(name, "delay", p.delay_ms, value)
        || setIf(name, "delay_ms", p.delay_ms, value)
        || setIf(name, "feedback", p.feedback, value)
        || setIf(name, "tone", p.tone, value)
        || setIf(name, "mix", p.mix, value)
        || setIf(name, "comp", p.comp_amount, value)
        || setIf(name, "comp_amount", p.comp_amount, value)
        || setIf(name, "chorus_depth", p.chorus_depth, value)
        || setIf(name, "rate", p.lfo_rate_hz, value)
        || setIf(name, "lfo_rate", p.lfo_rate_hz, value)
        || setIf(name, "lfo_rate_hz", p.lfo_rate_hz, value)
        || setIf(name, "supply", p.supply_voltage, value)
        || setIf(name, "supply_voltage", p.supply_voltage, value)
        || setIf(name, "stages", p.bbd_stages, value)
        || setIf(name, "bbd_stages", p.bbd_stages, value)
        || setIf(name, "emulate_bbd", p.emulate_bbd, value)
        || setIf(name, "emulate_opamp_sat", p.emulate_opamp_sat, value)
        || setIf(name, "emulate_tone_rc", p.emulate_tone_rc, value)
        || setIf(name, "aging", p.enable_aging, value)
        || setIf(name, "enable_aging", p.enable_aging, value)
        || setIf(name, "age_years", p.age_years, value)
        || setIf(name, "capacitance_scale", p.capacitance_scale, value)
        || setIf(name, "pedal_mode", p.pedal_mode, value);
}

bool setCommon(PatinaPd* x, const char* name, float value)
{
    switch (x->engine) {
        case Engine::drive: return setDrive(x->driveParams, name, value);
        case Engine::filter: return setFilter(x->filterParams, name, value);
        case Engine::delay: return setDelay(x->delayParams, name, value);
        case Engine::reverb:
            return setIf(name, "decay", x->reverbParams.decay, value)
                || setIf(name, "tone", x->reverbParams.tone, value)
                || setIf(name, "mix", x->reverbParams.mix, value)
                || setIf(name, "type", x->reverbParams.type, value)
                || setIf(name, "supply", x->reverbParams.supply_voltage, value)
                || setIf(name, "supply_voltage", x->reverbParams.supply_voltage, value)
                || setIf(name, "tension", x->reverbParams.tension, value)
                || setIf(name, "drip", x->reverbParams.drip_amount, value)
                || setIf(name, "drip_amount", x->reverbParams.drip_amount, value)
                || setIf(name, "springs", x->reverbParams.num_springs, value)
                || setIf(name, "num_springs", x->reverbParams.num_springs, value)
                || setIf(name, "predelay", x->reverbParams.predelay_ms, value)
                || setIf(name, "predelay_ms", x->reverbParams.predelay_ms, value)
                || setIf(name, "damping", x->reverbParams.damping, value)
                || setIf(name, "diffusion", x->reverbParams.diffusion, value)
                || setIf(name, "mod_depth", x->reverbParams.mod_depth, value)
                || setIf(name, "pedal_mode", x->reverbParams.pedal_mode, value);
        case Engine::compressor:
            return setIf(name, "threshold", x->compressorParams.threshold, value)
                || setIf(name, "input", x->compressorParams.input_gain, value)
                || setIf(name, "input_gain", x->compressorParams.input_gain, value)
                || setIf(name, "output", x->compressorParams.output_gain, value)
                || setIf(name, "output_gain", x->compressorParams.output_gain, value)
                || setIf(name, "attack", x->compressorParams.attack, value)
                || setIf(name, "release", x->compressorParams.release, value)
                || setIf(name, "ratio", x->compressorParams.ratio, value)
                || setIf(name, "mix", x->compressorParams.mix, value)
                || setIf(name, "type", x->compressorParams.type, value)
                || setIf(name, "supply", x->compressorParams.supply_voltage, value)
                || setIf(name, "supply_voltage", x->compressorParams.supply_voltage, value)
                || setIf(name, "gate", x->compressorParams.enable_gate, value)
                || setIf(name, "enable_gate", x->compressorParams.enable_gate, value)
                || setIf(name, "gate_threshold", x->compressorParams.gate_threshold_db, value)
                || setIf(name, "gate_threshold_db", x->compressorParams.gate_threshold_db, value)
                || setIf(name, "photo_mode", x->compressorParams.photo_mode, value)
                || setIf(name, "pedal_mode", x->compressorParams.pedal_mode, value);
        case Engine::modulation:
            return setIf(name, "rate", x->modulationParams.rate, value)
                || setIf(name, "depth", x->modulationParams.depth, value)
                || setIf(name, "feedback", x->modulationParams.feedback, value)
                || setIf(name, "mix", x->modulationParams.mix, value)
                || setIf(name, "type", x->modulationParams.type, value)
                || setIf(name, "supply", x->modulationParams.supply_voltage, value)
                || setIf(name, "supply_voltage", x->modulationParams.supply_voltage, value)
                || setIf(name, "center_freq", x->modulationParams.center_freq_hz, value)
                || setIf(name, "center_freq_hz", x->modulationParams.center_freq_hz, value)
                || setIf(name, "freq_spread", x->modulationParams.freq_spread_hz, value)
                || setIf(name, "freq_spread_hz", x->modulationParams.freq_spread_hz, value)
                || setIf(name, "stages", x->modulationParams.num_stages, value)
                || setIf(name, "num_stages", x->modulationParams.num_stages, value)
                || setIf(name, "temperature", x->modulationParams.temperature, value)
                || setIf(name, "tremolo_mode", x->modulationParams.tremolo_mode, value)
                || setIf(name, "stereo_phase", x->modulationParams.stereo_phase_invert, value)
                || setIf(name, "stereo_phase_invert", x->modulationParams.stereo_phase_invert, value)
                || setIf(name, "chorus_delay", x->modulationParams.chorus_delay_ms, value)
                || setIf(name, "chorus_delay_ms", x->modulationParams.chorus_delay_ms, value)
                || setIf(name, "stereo_width", x->modulationParams.stereo_width, value)
                || setIf(name, "pedal_mode", x->modulationParams.pedal_mode, value);
        case Engine::tape:
            return setIf(name, "input", x->tapeParams.input_gain, value)
                || setIf(name, "input_gain", x->tapeParams.input_gain, value)
                || setIf(name, "saturation", x->tapeParams.saturation, value)
                || setIf(name, "bias", x->tapeParams.bias_amount, value)
                || setIf(name, "bias_amount", x->tapeParams.bias_amount, value)
                || setIf(name, "speed", x->tapeParams.tape_speed, value)
                || setIf(name, "tape_speed", x->tapeParams.tape_speed, value)
                || setIf(name, "wow_flutter", x->tapeParams.wow_flutter, value)
                || setIf(name, "head_bump", x->tapeParams.enable_head_bump, value)
                || setIf(name, "enable_head_bump", x->tapeParams.enable_head_bump, value)
                || setIf(name, "hf_rolloff", x->tapeParams.enable_hf_rolloff, value)
                || setIf(name, "enable_hf_rolloff", x->tapeParams.enable_hf_rolloff, value)
                || setIf(name, "head_wear", x->tapeParams.head_wear, value)
                || setIf(name, "tape_age", x->tapeParams.tape_age, value)
                || setIf(name, "transformer", x->tapeParams.enable_transformer, value)
                || setIf(name, "enable_transformer", x->tapeParams.enable_transformer, value)
                || setIf(name, "transformer_drive", x->tapeParams.transformer_drive, value)
                || setIf(name, "transformer_sat", x->tapeParams.transformer_sat, value)
                || setIf(name, "tone", x->tapeParams.tone, value)
                || setIf(name, "mix", x->tapeParams.mix, value)
                || setIf(name, "supply", x->tapeParams.supply_voltage, value)
                || setIf(name, "supply_voltage", x->tapeParams.supply_voltage, value)
                || setIf(name, "pedal_mode", x->tapeParams.pedal_mode, value);
        case Engine::channelStrip:
            return setIf(name, "drive", x->channelStripParams.preamp_drive, value)
                || setIf(name, "preamp_drive", x->channelStripParams.preamp_drive, value)
                || setIf(name, "bias", x->channelStripParams.preamp_bias, value)
                || setIf(name, "preamp_bias", x->channelStripParams.preamp_bias, value)
                || setIf(name, "output", x->channelStripParams.preamp_output, value)
                || setIf(name, "preamp_output", x->channelStripParams.preamp_output, value)
                || setIf(name, "tube_age", x->channelStripParams.tube_age, value)
                || setIf(name, "eq", x->channelStripParams.enable_eq, value)
                || setIf(name, "enable_eq", x->channelStripParams.enable_eq, value)
                || setIf(name, "eq_cutoff", x->channelStripParams.eq_cutoff_hz, value)
                || setIf(name, "eq_cutoff_hz", x->channelStripParams.eq_cutoff_hz, value)
                || setIf(name, "eq_resonance", x->channelStripParams.eq_resonance, value)
                || setIf(name, "eq_type", x->channelStripParams.eq_type, value)
                || setIf(name, "eq_temperature", x->channelStripParams.eq_temperature, value)
                || setIf(name, "gate", x->channelStripParams.enable_gate, value)
                || setIf(name, "enable_gate", x->channelStripParams.enable_gate, value)
                || setIf(name, "gate_threshold", x->channelStripParams.gate_threshold_db, value)
                || setIf(name, "gate_threshold_db", x->channelStripParams.gate_threshold_db, value)
                || setIf(name, "gate_hysteresis", x->channelStripParams.gate_hysteresis_db, value)
                || setIf(name, "gate_hysteresis_db", x->channelStripParams.gate_hysteresis_db, value)
                || setIf(name, "input_trim", x->channelStripParams.input_trim_db, value)
                || setIf(name, "input_trim_db", x->channelStripParams.input_trim_db, value)
                || setIf(name, "output_trim", x->channelStripParams.output_trim_db, value)
                || setIf(name, "output_trim_db", x->channelStripParams.output_trim_db, value)
                || setIf(name, "supply", x->channelStripParams.supply_voltage, value)
                || setIf(name, "supply_voltage", x->channelStripParams.supply_voltage, value)
                || setIf(name, "pedal_mode", x->channelStripParams.pedal_mode, value);
        case Engine::eq:
            return setIf(name, "low", x->eqParams.enable_low, value)
                || setIf(name, "enable_low", x->eqParams.enable_low, value)
                || setIf(name, "low_freq", x->eqParams.low_freq_hz, value)
                || setIf(name, "low_freq_hz", x->eqParams.low_freq_hz, value)
                || setIf(name, "low_gain", x->eqParams.low_gain_db, value)
                || setIf(name, "low_gain_db", x->eqParams.low_gain_db, value)
                || setIf(name, "low_resonance", x->eqParams.low_resonance, value)
                || setIf(name, "mid", x->eqParams.enable_mid, value)
                || setIf(name, "enable_mid", x->eqParams.enable_mid, value)
                || setIf(name, "mid_freq", x->eqParams.mid_freq_hz, value)
                || setIf(name, "mid_freq_hz", x->eqParams.mid_freq_hz, value)
                || setIf(name, "mid_gain", x->eqParams.mid_gain_db, value)
                || setIf(name, "mid_gain_db", x->eqParams.mid_gain_db, value)
                || setIf(name, "mid_q", x->eqParams.mid_q, value)
                || setIf(name, "high", x->eqParams.enable_high, value)
                || setIf(name, "enable_high", x->eqParams.enable_high, value)
                || setIf(name, "high_freq", x->eqParams.high_freq_hz, value)
                || setIf(name, "high_freq_hz", x->eqParams.high_freq_hz, value)
                || setIf(name, "high_gain", x->eqParams.high_gain_db, value)
                || setIf(name, "high_gain_db", x->eqParams.high_gain_db, value)
                || setIf(name, "high_resonance", x->eqParams.high_resonance, value)
                || setIf(name, "temperature", x->eqParams.temperature, value)
                || setIf(name, "output", x->eqParams.output_gain_db, value)
                || setIf(name, "output_gain", x->eqParams.output_gain_db, value)
                || setIf(name, "output_gain_db", x->eqParams.output_gain_db, value)
                || setIf(name, "supply", x->eqParams.supply_voltage, value)
                || setIf(name, "supply_voltage", x->eqParams.supply_voltage, value)
                || setIf(name, "pedal_mode", x->eqParams.pedal_mode, value);
        case Engine::limiter:
            return setIf(name, "ceiling", x->limiterParams.ceiling, value)
                || setIf(name, "attack", x->limiterParams.attack, value)
                || setIf(name, "release", x->limiterParams.release, value)
                || setIf(name, "output", x->limiterParams.output_gain, value)
                || setIf(name, "output_gain", x->limiterParams.output_gain, value)
                || setIf(name, "mix", x->limiterParams.mix, value)
                || setIf(name, "type", x->limiterParams.type, value)
                || setIf(name, "pedal_mode", x->limiterParams.pedal_mode, value)
                || setIf(name, "supply", x->limiterParams.supply_voltage, value)
                || setIf(name, "supply_voltage", x->limiterParams.supply_voltage, value);
        case Engine::envelope:
            return setIf(name, "attack", x->envelopeParams.attack, value)
                || setIf(name, "decay", x->envelopeParams.decay, value)
                || setIf(name, "sustain", x->envelopeParams.sustain, value)
                || setIf(name, "release", x->envelopeParams.release, value)
                || setIf(name, "mode", x->envelopeParams.env_mode, value)
                || setIf(name, "env_mode", x->envelopeParams.env_mode, value)
                || setIf(name, "curve", x->envelopeParams.curve, value)
                || setIf(name, "trigger", x->envelopeParams.trigger_mode, value)
                || setIf(name, "trigger_mode", x->envelopeParams.trigger_mode, value)
                || setIf(name, "auto_threshold", x->envelopeParams.auto_threshold_db, value)
                || setIf(name, "auto_threshold_db", x->envelopeParams.auto_threshold_db, value)
                || setIf(name, "velocity", x->envelopeParams.velocity, value)
                || setIf(name, "depth", x->envelopeParams.vca_depth, value)
                || setIf(name, "vca_depth", x->envelopeParams.vca_depth, value)
                || setIf(name, "output", x->envelopeParams.output_gain, value)
                || setIf(name, "output_gain", x->envelopeParams.output_gain, value)
                || setIf(name, "mix", x->envelopeParams.mix, value)
                || setIf(name, "temperature", x->envelopeParams.temperature, value)
                || setIf(name, "pedal_mode", x->envelopeParams.pedal_mode, value)
                || setIf(name, "supply", x->envelopeParams.supply_voltage, value)
                || setIf(name, "supply_voltage", x->envelopeParams.supply_voltage, value);
    }

    return false;
}

void process(PatinaPd* x, int n, const t_sample* in, t_sample* out)
{
    x->input.resize(static_cast<size_t>(n));
    x->output.resize(static_cast<size_t>(n));

    for (int i = 0; i < n; ++i) {
        x->input[static_cast<size_t>(i)] = static_cast<float>(in[i]);
    }

    const float* inputs[] = { x->input.data() };
    float* outputs[] = { x->output.data() };

    switch (x->engine) {
        case Engine::delay: patina_delay_process(x->delay, inputs, outputs, 1, n, &x->delayParams); break;
        case Engine::drive: patina_drive_process(x->drive, inputs, outputs, 1, n, &x->driveParams); break;
        case Engine::reverb: patina_reverb_process(x->reverb, inputs, outputs, 1, n, &x->reverbParams); break;
        case Engine::compressor: patina_compressor_process(x->compressor, inputs, outputs, 1, n, &x->compressorParams); break;
        case Engine::modulation: patina_modulation_process(x->modulation, inputs, outputs, 1, n, &x->modulationParams); break;
        case Engine::tape: patina_tape_process(x->tape, inputs, outputs, 1, n, &x->tapeParams); break;
        case Engine::channelStrip: patina_channel_strip_process(x->channelStrip, inputs, outputs, 1, n, &x->channelStripParams); break;
        case Engine::eq: patina_eq_process(x->eq, inputs, outputs, 1, n, &x->eqParams); break;
        case Engine::limiter: patina_limiter_process(x->limiter, inputs, outputs, 1, n, &x->limiterParams); break;
        case Engine::filter: patina_filter_process(x->filter, inputs, outputs, 1, n, &x->filterParams); break;
        case Engine::envelope: patina_envelope_generator_process(x->envelope, inputs, outputs, 1, n, &x->envelopeParams); break;
    }

    for (int i = 0; i < n; ++i) {
        out[i] = static_cast<t_sample>(x->output[static_cast<size_t>(i)]);
    }
}

t_int* perform(t_int* w)
{
    auto* x = reinterpret_cast<PatinaPd*>(w[1]);
    auto* in = reinterpret_cast<t_sample*>(w[2]);
    auto* out = reinterpret_cast<t_sample*>(w[3]);
    const int n = static_cast<int>(w[4]);

    process(x, n, in, out);
    return w + 5;
}

void dsp(PatinaPd* x, t_signal** sp)
{
    x->sampleRate = sp[0]->s_sr > 0 ? sp[0]->s_sr : 44100.0;
    x->blockSize = sp[0]->s_n;
    prepare(x);
    dsp_add(perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

void param(PatinaPd* x, t_symbol* s, t_floatarg value)
{
    if (!setCommon(x, s->s_name, static_cast<float>(value))) {
        pd_error(x, "patina~: unknown parameter '%s'", s->s_name);
    }
}

void floatMessage(PatinaPd* x, t_floatarg value)
{
    switch (x->engine) {
        case Engine::drive: x->driveParams.drive = static_cast<float>(value); break;
        case Engine::filter: x->filterParams.filter1_cutoff_hz = static_cast<float>(value); break;
        case Engine::delay: x->delayParams.delay_ms = static_cast<float>(value); break;
        default: break;
    }
}

void gate(PatinaPd* x, t_floatarg value)
{
    if (x->engine != Engine::envelope) return;
    if (value != 0) patina_envelope_generator_gate_on(x->envelope);
    else patina_envelope_generator_gate_off(x->envelope);
}

void freeObject(PatinaPd* x)
{
    patina_delay_destroy(x->delay);
    patina_drive_destroy(x->drive);
    patina_reverb_destroy(x->reverb);
    patina_compressor_destroy(x->compressor);
    patina_modulation_destroy(x->modulation);
    patina_tape_destroy(x->tape);
    patina_channel_strip_destroy(x->channelStrip);
    patina_eq_destroy(x->eq);
    patina_limiter_destroy(x->limiter);
    patina_filter_destroy(x->filter);
    patina_envelope_generator_destroy(x->envelope);
}

void* newObjectWithClass(t_class* klass, Engine engine)
{
    auto* x = reinterpret_cast<PatinaPd*>(pd_new(klass));
    x->outlet = outlet_new(&x->object, &s_signal);
    x->engine = engine;

    x->delayParams = patina_delay_default_params();
    x->driveParams = patina_drive_default_params();
    x->reverbParams = patina_reverb_default_params();
    x->compressorParams = patina_compressor_default_params();
    x->modulationParams = patina_modulation_default_params();
    x->tapeParams = patina_tape_default_params();
    x->channelStripParams = patina_channel_strip_default_params();
    x->eqParams = patina_eq_default_params();
    x->limiterParams = patina_limiter_default_params();
    x->filterParams = patina_filter_default_params();
    x->envelopeParams = patina_envelope_generator_default_params();

    switch (x->engine) {
        case Engine::delay: x->delay = patina_delay_create(); break;
        case Engine::drive: x->drive = patina_drive_create(); break;
        case Engine::reverb: x->reverb = patina_reverb_create(); break;
        case Engine::compressor: x->compressor = patina_compressor_create(); break;
        case Engine::modulation: x->modulation = patina_modulation_create(); break;
        case Engine::tape: x->tape = patina_tape_create(); break;
        case Engine::channelStrip: x->channelStrip = patina_channel_strip_create(); break;
        case Engine::eq: x->eq = patina_eq_create(); break;
        case Engine::limiter: x->limiter = patina_limiter_create(); break;
        case Engine::filter: x->filter = patina_filter_create(); break;
        case Engine::envelope: x->envelope = patina_envelope_generator_create(); break;
    }

    prepare(x);
    return x;
}

void* newObject(t_symbol* engineSymbol)
{
    return newObjectWithClass(patinaClass, parseEngine(engineSymbol));
}

void* newDriveObject() { return newObjectWithClass(patinaDriveClass, Engine::drive); }
void* newFilterObject() { return newObjectWithClass(patinaFilterClass, Engine::filter); }
void* newDelayObject() { return newObjectWithClass(patinaDelayClass, Engine::delay); }
void* newReverbObject() { return newObjectWithClass(patinaReverbClass, Engine::reverb); }
void* newCompressorObject() { return newObjectWithClass(patinaCompressorClass, Engine::compressor); }
void* newModulationObject() { return newObjectWithClass(patinaModulationClass, Engine::modulation); }
void* newTapeObject() { return newObjectWithClass(patinaTapeClass, Engine::tape); }
void* newEqObject() { return newObjectWithClass(patinaEqClass, Engine::eq); }
void* newLimiterObject() { return newObjectWithClass(patinaLimiterClass, Engine::limiter); }
void* newChannelStripObject() { return newObjectWithClass(patinaChannelStripClass, Engine::channelStrip); }
void* newEnvelopeObject() { return newObjectWithClass(patinaEnvelopeClass, Engine::envelope); }

void addMethods(t_class* klass)
{
    CLASS_MAINSIGNALIN(klass, PatinaPd, signalInlet);
    class_addmethod(klass, reinterpret_cast<t_method>(dsp), gensym("dsp"), A_CANT, A_NULL);
    class_addmethod(klass, reinterpret_cast<t_method>(param), gensym("param"), A_SYMBOL, A_FLOAT, A_NULL);
    class_addmethod(klass, reinterpret_cast<t_method>(reset), gensym("reset"), A_NULL);
    class_addmethod(klass, reinterpret_cast<t_method>(gate), gensym("gate"), A_FLOAT, A_NULL);
    class_addfloat(klass, reinterpret_cast<t_method>(floatMessage));
}

t_class* makeAliasClass(const char* name, t_newmethod newMethod)
{
    auto* klass = class_new(gensym(name),
                            newMethod,
                            reinterpret_cast<t_method>(freeObject),
                            sizeof(PatinaPd),
                            CLASS_DEFAULT,
                            A_NULL);
    addMethods(klass);
    return klass;
}

} // namespace

extern "C" void patina_tilde_setup(void)
{
    patinaClass = class_new(gensym("patina~"),
                            reinterpret_cast<t_newmethod>(newObject),
                            reinterpret_cast<t_method>(freeObject),
                            sizeof(PatinaPd),
                            CLASS_DEFAULT,
                            A_DEFSYM,
                            A_NULL);

    addMethods(patinaClass);

    patinaDriveClass = makeAliasClass("patina-drive~", reinterpret_cast<t_newmethod>(newDriveObject));
    patinaFilterClass = makeAliasClass("patina-filter~", reinterpret_cast<t_newmethod>(newFilterObject));
    patinaDelayClass = makeAliasClass("patina-delay~", reinterpret_cast<t_newmethod>(newDelayObject));
    patinaReverbClass = makeAliasClass("patina-reverb~", reinterpret_cast<t_newmethod>(newReverbObject));
    patinaCompressorClass = makeAliasClass("patina-compressor~", reinterpret_cast<t_newmethod>(newCompressorObject));
    patinaModulationClass = makeAliasClass("patina-modulation~", reinterpret_cast<t_newmethod>(newModulationObject));
    patinaTapeClass = makeAliasClass("patina-tape~", reinterpret_cast<t_newmethod>(newTapeObject));
    patinaEqClass = makeAliasClass("patina-eq~", reinterpret_cast<t_newmethod>(newEqObject));
    patinaLimiterClass = makeAliasClass("patina-limiter~", reinterpret_cast<t_newmethod>(newLimiterObject));
    patinaChannelStripClass = makeAliasClass("patina-channelstrip~", reinterpret_cast<t_newmethod>(newChannelStripObject));
    patinaEnvelopeClass = makeAliasClass("patina-envelope~", reinterpret_cast<t_newmethod>(newEnvelopeObject));
}
