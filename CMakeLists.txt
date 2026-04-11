cmake_minimum_required(VERSION 3.16)
project(Patina 
    VERSION 1.0.0 
    LANGUAGES CXX
    DESCRIPTION "Standalone C++ Analog Modeling DSP Library"
)

# ============================================================================
# Configuration
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ============================================================================
# Build Options
# ============================================================================

option(PATINA_BUILD_TESTS "Build unit tests" OFF)
option(PATINA_BUILD_STATIC_LIB "Build single static library archive (libPatina.a)" ON)
option(PATINA_BUILD_C_BINDINGS "Build C API shared/static library" OFF)

# ============================================================================
# Main Library Target
# ============================================================================

set(LIB_NAME Patina)

# Header-only library (no .cpp files required for most modules)
add_library(${LIB_NAME} INTERFACE)
add_library(Patina::Patina ALIAS ${LIB_NAME})

# Include directories
target_include_directories(${LIB_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Compile features
target_compile_features(${LIB_NAME} INTERFACE cxx_std_17)

# ============================================================================
# Optional: Add implementations that require compilation
# ============================================================================

# Check for .cpp files that may need compilation
file(GLOB_RECURSE DSP_IMPLEMENTATIONS "${CMAKE_CURRENT_SOURCE_DIR}/dsp/**/*.cpp")

if(DSP_IMPLEMENTATIONS)
    # Create object library for implementations
    add_library(${LIB_NAME}_impl OBJECT ${DSP_IMPLEMENTATIONS})
    
    target_include_directories(${LIB_NAME}_impl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(${LIB_NAME}_impl PRIVATE cxx_std_17)
    
    # Link implementations to main library
    target_link_libraries(${LIB_NAME} INTERFACE $<TARGET_OBJECTS:${LIB_NAME}_impl>)
    
    # Optionally create a single static library archive from the object files
    if(PATINA_BUILD_STATIC_LIB)
        add_library(${LIB_NAME}_static STATIC $<TARGET_OBJECTS:${LIB_NAME}_impl>)
        set_target_properties(${LIB_NAME}_static PROPERTIES OUTPUT_NAME "${LIB_NAME}")
        target_include_directories(${LIB_NAME}_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
        if(NOT ANDROID)
            set_target_properties(${LIB_NAME}_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
        endif()
    endif()
endif()

message(STATUS "Building standalone Patina (no JUCE required)")

# ============================================================================
# C Bindings (Optional)
# ============================================================================

if(PATINA_BUILD_C_BINDINGS)
    set(PATINA_C_SRC ${CMAKE_CURRENT_SOURCE_DIR}/bindings/c/patina_c.cpp)

    # Static C binding library
    add_library(patina_c_static STATIC ${PATINA_C_SRC})
    set_target_properties(patina_c_static PROPERTIES OUTPUT_NAME "patina_c")
    target_include_directories(patina_c_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bindings/c>
        $<INSTALL_INTERFACE:include>
    )
    target_include_directories(patina_c_static PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(patina_c_static PRIVATE cxx_std_17)
    if(NOT ANDROID)
        set_target_properties(patina_c_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()

    # Shared C binding library
    add_library(patina_c_shared SHARED ${PATINA_C_SRC})
    set_target_properties(patina_c_shared PROPERTIES OUTPUT_NAME "patina_c")
    target_include_directories(patina_c_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/bindings/c>
        $<INSTALL_INTERFACE:include>
    )
    target_include_directories(patina_c_shared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(patina_c_shared PRIVATE cxx_std_17)
    target_compile_definitions(patina_c_shared PRIVATE PATINA_EXPORT PATINA_SHARED)

    install(TARGETS patina_c_static patina_c_shared
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
    )
    install(FILES bindings/c/patina_c.h DESTINATION include)

    message(STATUS "  Build C Bindings: ON")
endif()

# ============================================================================
# Tests (Optional)
# ============================================================================

if(PATINA_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Installation
# ============================================================================

install(TARGETS ${LIB_NAME}
    EXPORT ${LIB_NAME}Targets
)

if(TARGET ${LIB_NAME}_static)
    install(TARGETS ${LIB_NAME}_static
        ARCHIVE DESTINATION lib
    )
endif()

# Install aggregate header
install(FILES include/patina.h
    DESTINATION include
)

# Install DSP headers preserving directory structure
# After install: headers are at <prefix>/include/dsp/**
# Usage: #include "dsp/core/AudioCompat.h"
install(DIRECTORY dsp/
    DESTINATION include/dsp
    FILES_MATCHING 
        PATTERN "*.h"
        PATTERN "*.hpp"
)

# Install CMake config
install(EXPORT ${LIB_NAME}Targets
    FILE ${LIB_NAME}Targets.cmake
    NAMESPACE Patina::
    DESTINATION lib/cmake/${LIB_NAME}
)

# ============================================================================
# Integration: Find Package Support
# ============================================================================

# Allow find_package(Patina)
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${LIB_NAME}Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${LIB_NAME}Config.cmake"
    INSTALL_DESTINATION lib/cmake/${LIB_NAME}
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${LIB_NAME}Config.cmake"
    DESTINATION lib/cmake/${LIB_NAME}
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "Patina Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Static Lib: ${PATINA_BUILD_STATIC_LIB}")
message(STATUS "  Build Tests: ${PATINA_BUILD_TESTS}")
message(STATUS "  Build C Bindings: ${PATINA_BUILD_C_BINDINGS}")
message(STATUS "")
message(STATUS "Usage in CMake projects:")
message(STATUS "  find_package(Patina REQUIRED)")
message(STATUS "  target_link_libraries(your_target PRIVATE Patina::Patina)")
message(STATUS "")
