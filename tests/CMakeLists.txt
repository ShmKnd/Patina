cmake_minimum_required(VERSION 3.16)

# ============================================================================
# Catch2 (FetchContent)
# ============================================================================
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.4
)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)

# ============================================================================
# Test executable
# ============================================================================
add_executable(Patina_tests
    test_audio_compat.cpp
    test_fastmath.cpp
    test_mixer.cpp
    test_stereo_image.cpp
    test_compander.cpp
    test_bbd_stage_emulator.cpp
    test_bbd_sampler.cpp
    test_delay_core.cpp
    test_tone_filter.cpp
    test_drive_module.cpp
    test_tl072_input_buffer.cpp
    test_tl072_lfo.cpp
    test_input_lpf.cpp
    test_output_module.cpp
    test_feedback_module.cpp
    test_time_stage_controller.cpp
    test_modding_config.cpp
    test_process_spec.cpp
    test_analog_presets.cpp
    test_bbd_delay_engine.cpp
    test_state_variable_filter.cpp
    test_ladder_filter.cpp
    test_phaser_stage.cpp
    test_tube_preamp.cpp
    test_transformer_model.cpp
    test_tape_saturation.cpp
    test_envelope_follower.cpp
    test_tremolo_vca.cpp
    test_noise_gate.cpp
    test_spring_reverb.cpp
    test_plate_reverb.cpp
    test_power_supply_sag.cpp
    test_battery_sag.cpp
    test_adapter_sag.cpp
    test_capacitor_aging.cpp
    test_ota_sk_filter.cpp
    test_diode_ladder_filter.cpp
    test_analog_allpass.cpp
    test_photo_compressor.cpp
    test_fet_compressor.cpp
    test_variable_mu_compressor.cpp
    test_parts_primitives.cpp
    test_drive_engine.cpp
    test_reverb_engine.cpp
    test_compressor_engine.cpp
    test_modulation_engine.cpp
    test_tape_machine_engine.cpp
    test_channel_strip_engine.cpp
    test_analog_vco.cpp
    test_audio_transformer.cpp
    test_envelope_generator.cpp
    test_limiter_engine.cpp
    test_ring_modulator.cpp
    test_wave_folder.cpp
    test_opamp_primitive.cpp
)

target_include_directories(Patina_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(Patina_tests PRIVATE cxx_std_17)
target_link_libraries(Patina_tests PRIVATE Catch2::Catch2WithMain)

# Register tests with CTest
catch_discover_tests(Patina_tests)
