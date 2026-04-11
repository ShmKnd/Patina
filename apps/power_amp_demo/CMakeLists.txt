cmake_minimum_required(VERSION 3.22)
project(PatinaExamples VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# JUCE
# ============================================================================

set(JUCE_DIR "/Users/shoma/Documents/JUCE" CACHE PATH
    "Path to JUCE source root (override with -DJUCE_DIR=...)")
add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

# ============================================================================
# Patina (2 levels up from apps/power_amp_demo/)
# ============================================================================

set(PATINA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../.."
                 "${CMAKE_BINARY_DIR}/Patina")

# ============================================================================
# GUI App  –  Tube Power Amp + LC Filter demo
# ============================================================================

juce_add_gui_app(PatinaExamples
    PRODUCT_NAME "Patina Examples"
    BUNDLE_ID    "com.patina.examples"
)

target_sources(PatinaExamples PRIVATE Main.cpp)

juce_generate_juce_header(PatinaExamples)

target_link_libraries(PatinaExamples
    PRIVATE
        Patina_static
        juce::juce_core
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_utils
)
