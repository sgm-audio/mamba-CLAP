# CMake outline for the Mamba-SSM CLAP/ARA plugin suite
# This is a conceptual outline showing the target structure.

cmake_minimum_required(VERSION 3.18)
project(MambaSSM_PLUGIN_SUITE VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific settings
if(APPLE)
    # macOS Universal build
    set(ARCHITECTURES arm64 x86_64)
    set(CMAKE_OSX_ARCHITECTURES "${ARCHITECTURES}")
    # CLAP and ARA SDK paths
    set(CLAP_SDK_PATH "/path/to/clap" CACHE PATH "Path to CLAP SDK")
    set(ARA_SDK_PATH "/path/to/ara2" CACHE PATH "Path to ARA 2.0 SDK")
elseif(WIN32)
    # Windows build
    # Set runtime library to multi-threaded DLL
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    set(CLAP_SDK_PATH "C:/path/to/clap" CACHE PATH "Path to CLAP SDK")
    set(ARA_SDK_PATH "C:/path/to/ara2" CACHE PATH "Path to ARA 2.0 SDK")
elseif(UNIX)
    # Linux build
    set(CLAP_SDK_PATH "/usr/local/include/clap" CACHE PATH "Path to CLAP SDK")
    set(ARA_SDK_PATH "/usr/local/include/ara2" CACHE PATH "Path to ARA 2.0 SDK")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CLAP_SDK_DIR}/include
    ${ARA_SDK_DIR}/include
)

# Library for SSM inference core (static library, used by all plugins)
add_library(ssm_core STATIC
    src/ssm/ssm_inference_core.cpp
    src/ssm/ssm_block.cpp
    src/ssm/weight_loader.cpp
)

target_include_directories(ssm_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ssm
)

# Plugin implementations (each as a module that can be compiled to CLAP, VST3, etc.)
# For simplicity, we show CLAP targets here.

# Mamba Synth Plugin
add_library(mamba_synth MODULE
    src/plugins/synth/synth_processor.cpp
    src/plugins/synth/synth_factory.cpp
)

target_link_libraries(mamba_synth PRIVATE
    ssm_core
    ${CLAP_LIBRARY}  # CLAP library
)

# Set plugin properties
set_target_properties(mamba_synth PROPERTIES
    PREFIX ""  # No lib prefix for plugins
    SUFFIX ".clap"  # CLAP extension
)

# Mamba FX Plugin
add_library(mamba_fx MODULE
    src/plugins/fx/fx_processor.cpp
    src/plugins/fx/fx_factory.cpp
)

target_link_libraries(mamba_fx PRIVATE
    ssm_core
    ${CLAP_LIBRARY}
)

set_target_properties(mamba_fx PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
)

# ARA Generative Companion Plugin
add_library(ara_companion MODULE
    src/plugins/ara/companion_processor.cpp
    src/plugins/ara/companion_factory.cpp
)

target_link_libraries(ara_companion PRIVATE
    ssm_core
    ${CLAP_LIBRARY}
    ${ARA_LIBRARY}  // ARA library
)

set_target_properties(ara_companion PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
)

# Optional: LV2 wrapper for Linux
if(UNIX AND NOT APPLE)
    # Find LV2 SDK
    find_package(PkgConfig)
    pkg_check_modules(LV2 REQUIRED lv2)
    
    # Create LV2 bundles that wrap the CLAP plugins
    # This would involve creating .ttl.ttl files and a small wrapper
    # For brevity, we omit the detailed LV2 wrapping logic here.
    message(STATUS "LV2 support enabled for Linux")
endif()

# Installation
install(TARGETS mamba_synth mamba_fx ara_companion
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/clap
)

# Testing
enable_testing()

# Unit tests for SSM core (can run in real-time simulation)
add_executable(test_ssm_core
    tests/ssm/test_ssm_inference_core.cpp
    tests/ssm/test_ssm_block.cpp
)

target_link_libraries(test_ssm_core PRIVATE
    ssm_core
    # Add testing libraries like Catch2 or Google Test
)

add_test(NAME SSM_Core_Test COMMAND test_ssm_core)

# Note: This is an outline. A real CMake setup would need to:
# - Find and link against actual CLAP and ARA SDKs
# - Handle plugin-specific resource files (e.g., .ttl for LV2, .json for CLAP)
# - Support different build types (Debug, Release, etc.)
# - Include proper compiler flags for real-time safety (e.g., -fno-exceptions, -fno-rtti)
# - Handle code signing for macOS (if distributing)