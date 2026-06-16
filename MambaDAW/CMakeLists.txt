cmake_minimum_required(VERSION 3.14)
project(MambaDAW LANGUAGES C CXX)

# NOT IMPLEMENTED (post-MVP): GPU/CUDA inference path (FR4.4/NFR2/NFR7)
# NOT IMPLEMENTED (post-MVP): Windows/macOS builds (NFR8) — Linux-only for MVP
# IMPLEMENTED (Phase 2): Headless VST3 wrapper — no GUI, links mamba_core
# NOT IMPLEMENTED (post-MVP): AU wrapper (NFR10) — CLAP+VST3+LV2 shipped

# ---------------------------------------------------------------------------
# Build options — each generates a separate plugin MODULE
# ---------------------------------------------------------------------------
option(MAMBA_ENABLE_CLAP "Build CLAP plugin"      ON)
option(MAMBA_ENABLE_VST3 "Build VST3 plugin"      OFF)  # Phase 2 (done)
option(MAMBA_ENABLE_LV2  "Build LV2 plugin"       OFF)  # Phase 3 (done)
option(MAMBA_ENABLE_GUI  "Build with JUCE GUI"    ON)

# ---------------------------------------------------------------------------
# ADR-004: Model configuration via generated config.h
# Override any default on the cmake command line, e.g.:
#   cmake -DMAMBA_N_MEL=128 -DMAMBA_N_LAYERS=8 ..
# ---------------------------------------------------------------------------
set(MAMBA_N_MEL            80  CACHE STRING "Number of mel frequency bands")
set(MAMBA_N_FFT            2048 CACHE STRING "FFT size (must be power of 2)")
set(MAMBA_HOP_SAMPLES      512  CACHE STRING "STFT hop size in samples")
set(MAMBA_N_MAMBA_OUT      80   CACHE STRING "Mamba head output dimension")
set(MAMBA_N_AUDIO_FEATURES 4    CACHE STRING "Compact audio feature count")
set(MAMBA_N_SESSION_FEATURES 4  CACHE STRING "Compact session feature count")
set(MAMBA_N_EFFECT_PARAMS  19   CACHE STRING "Number of effect parameters")
set(MAMBA_N_LAYERS         4    CACHE STRING "Number of Mamba SSM layers")
set(MAMBA_D_STATE          16   CACHE STRING "Mamba SSM state dimension")
set(MAMBA_D_CONV           4    CACHE STRING "Mamba SSM conv kernel size")
set(MAMBA_EXPAND           2    CACHE STRING "Mamba SSM expand factor")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mamba_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/mamba_config.h"
    @ONLY
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Shared generated include directory (used by all targets)
# ---------------------------------------------------------------------------
set(_GENERATED_INC_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# ===========================================================================
# mamba_core — format-agnostic static library containing all DSP, inference,
#              and processor code. No CLAP/VST3/LV2 dependencies.
#              Every plugin wrapper (CLAP, VST3, LV2) links this.
# ===========================================================================

# Core sources (always required)
set(_MAMBA_CORE_SOURCES
  src/plugin_processor.cpp
  src/mamba_layer.cpp
  src/mamba_model.cpp
  src/mamba_weight_loader.cpp
  src/parameter_gate.cpp
  src/state_serialization.cpp
)

# Optional sources — included only when source files exist (no stubs needed)
set(_MAMBA_CORE_OPTIONAL_SOURCES
  src/dsp/eq.cpp
  src/dsp/compressor.cpp
  src/dsp/saturator.cpp
  src/dsp/reverb.cpp
  src/dsp/dsp_chain.cpp
  src/dsp/spectral_feedback.cpp
  src/feature_extractor.cpp
  src/effect_decoder.cpp
  src/inference_thread.cpp
)
foreach(_src IN LISTS _MAMBA_CORE_OPTIONAL_SOURCES)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    list(APPEND _MAMBA_CORE_SOURCES "${_src}")
  endif()
endforeach()
unset(_MAMBA_CORE_OPTIONAL_SOURCES)

set(_MAMBA_CORE_HEADERS
  src/plugin_processor.h
  src/mamba_layer.h
  src/mamba_model.h
  src/mamba_weight_loader.h
  src/context_assembler.h
  src/parameter_gate.h
  src/param_smoother.h
  src/feature_extractor_rt.h
  src/state_serialization.h
  src/dsp/effect_params.h
)

add_library(mamba_core STATIC ${_MAMBA_CORE_SOURCES} ${_MAMBA_CORE_HEADERS})

target_include_directories(mamba_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
)

target_compile_definitions(mamba_core PRIVATE
    "-D_GLIBCXX_USE_CXX11_ABI=1"
)

target_compile_options(mamba_core PRIVATE -fPIC)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(mamba_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Threading — needed for inference_thread, state_serialization, etc.
find_package(Threads REQUIRED)
target_link_libraries(mamba_core PUBLIC Threads::Threads)

# Optional: Torch (for legacy/experimental PyTorch path)
find_package(Torch QUIET)
if(Torch_FOUND)
  target_include_directories(mamba_core PRIVATE ${TORCH_INCLUDE_DIRS})
  message(STATUS "Torch found: ${TORCH_INCLUDE_DIRS}")
endif()

unset(_MAMBA_CORE_SOURCES)
unset(_MAMBA_CORE_HEADERS)

# ===========================================================================
# CLAP plugin wrapper — mamba_clap
# ===========================================================================
if(MAMBA_ENABLE_CLAP)
  include(FetchContent)
  FetchContent_Declare(
    clap
    GIT_REPOSITORY https://github.com/free-audio/clap.git
    GIT_TAG 1.2.2
  )
  FetchContent_GetProperties(clap)
  if(NOT clap_POPULATED)
    FetchContent_MakeAvailable(clap)
  endif()

  set(_CLAP_SOURCES src/plugin_wrapper.cpp)
  set(_CLAP_HEADERS src/plugin_instance.h)

  if(MAMBA_ENABLE_GUI)
    set(JUCE_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/modules")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FREETYPE   REQUIRED freetype2)
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
    find_package(X11 REQUIRED)

    list(APPEND _CLAP_SOURCES
      ${JUCE_MODULES_DIR}/juce_core/juce_core.cpp
      ${JUCE_MODULES_DIR}/juce_events/juce_events.cpp
      ${JUCE_MODULES_DIR}/juce_data_structures/juce_data_structures.cpp
      ${JUCE_MODULES_DIR}/juce_graphics/juce_graphics.cpp
      ${JUCE_MODULES_DIR}/juce_gui_basics/juce_gui_basics.cpp
    )

    set(_GUI_SOURCES
      src/gui/MambaLookAndFeel.cpp
      src/gui/PluginEditor.cpp
      src/gui/HeaderBar.cpp
      src/gui/ContextMeter.cpp
      src/gui/ControlBar.cpp
      src/gui/SectionPanel.cpp
      src/gui/LevelMeter.cpp
      src/gui/MambaKnob.cpp
      src/gui/ManualControlsBar.cpp
      src/gui/ManualEQPanel.cpp
      src/gui/ManualCompPanel.cpp
      src/gui/ManualSatPanel.cpp
      src/gui/ManualRevPanel.cpp
      src/gui/gui_bridge.cpp
    )
    foreach(_src IN LISTS _GUI_SOURCES)
      if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        list(APPEND _CLAP_SOURCES "${_src}")
      endif()
    endforeach()
    unset(_GUI_SOURCES)
  endif()

  add_library(mamba_clap MODULE ${_CLAP_SOURCES} ${_CLAP_HEADERS})

  target_include_directories(mamba_clap PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(mamba_clap PRIVATE
    mamba_core
    clap
    dl
  )

  if(MAMBA_ENABLE_GUI)
    target_include_directories(mamba_clap PRIVATE
      ${JUCE_MODULES_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
      ${FREETYPE_INCLUDE_DIRS}
      ${FONTCONFIG_INCLUDE_DIRS}
      ${X11_INCLUDE_DIRS}
    )
    target_link_libraries(mamba_clap PRIVATE
      ${X11_LIBRARIES}
      ${FREETYPE_LIBRARIES}
      ${FONTCONFIG_LIBRARIES}
      rt
    )
    target_compile_definitions(mamba_clap PRIVATE
      MAMBA_ENABLE_GUI=1
      LINUX=1
      JUCE_STANDALONE_APPLICATION=0
      JUCE_MODAL_LOOPS_PERMITTED=1
      JUCE_USE_CURL=0
      JUCE_WEB_BROWSER=0
      JUCE_USE_XRANDR=1
      JUCE_USE_XCURSOR=1
      JUCE_USE_XINERAMA=1
      JUCE_USE_XRENDER=1
      JUCE_MODULE_AVAILABLE_juce_core=1
      JUCE_MODULE_AVAILABLE_juce_events=1
      JUCE_MODULE_AVAILABLE_juce_data_structures=1
      JUCE_MODULE_AVAILABLE_juce_graphics=1
      JUCE_MODULE_AVAILABLE_juce_gui_basics=1
      JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
      $<$<CONFIG:Debug>:DEBUG=1 _DEBUG=1>
      $<$<CONFIG:Release>:NDEBUG=1 _NDEBUG=1>
    )
  endif()

  set_target_properties(mamba_clap PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
    OUTPUT_NAME "MambaClapPlugin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Release"
  )

  target_compile_definitions(mamba_clap PRIVATE "-D_GLIBCXX_USE_CXX11_ABI=1")

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(mamba_clap PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  unset(_CLAP_SOURCES)
  unset(_CLAP_HEADERS)
endif()

# ===========================================================================
# VST3 plugin wrapper — mamba_vst3
# ===========================================================================
if(MAMBA_ENABLE_VST3)
  set(VST3_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK")

  if(NOT EXISTS "${VST3_SDK_DIR}/pluginterfaces/vst/ivstaudioprocessor.h")
    message(FATAL_ERROR "VST3 SDK not found at ${VST3_SDK_DIR}. "
      "Set MAMBA_ENABLE_VST3=OFF or ensure JUCE submodule is checked out.")
  endif()

  # Required VST3 SDK base source files
  set(_VST3_SDK_SOURCES
    "${VST3_SDK_DIR}/base/source/fobject.cpp"
    "${VST3_SDK_DIR}/base/source/fbuffer.cpp"
    "${VST3_SDK_DIR}/base/source/fstreamer.cpp"
    "${VST3_SDK_DIR}/base/source/fstring.cpp"
    "${VST3_SDK_DIR}/base/thread/source/flock.cpp"
    "${VST3_SDK_DIR}/pluginterfaces/base/funknown.cpp"
    "${VST3_SDK_DIR}/pluginterfaces/base/coreiids.cpp"
    "${VST3_SDK_DIR}/pluginterfaces/base/conststringtable.cpp"
    "${VST3_SDK_DIR}/pluginterfaces/base/ustring.cpp"
    "${VST3_SDK_DIR}/public.sdk/source/vst/vstcomponentbase.cpp"
    "${VST3_SDK_DIR}/public.sdk/source/vst/vstinitiids.cpp"
    "${VST3_SDK_DIR}/public.sdk/source/vst/vstbus.cpp"
    "${VST3_SDK_DIR}/public.sdk/source/common/memorystream.cpp"
    "${VST3_SDK_DIR}/public.sdk/source/common/commonstringconvert.cpp"
    "${VST3_SDK_DIR}/public.sdk/source/vst/utility/stringconvert.cpp"
  )

  set(_VST3_WRAPPER_SOURCES src/plugin_wrapper_vst3.cpp)

  add_library(mamba_vst3 MODULE
    ${_VST3_WRAPPER_SOURCES}
    ${_VST3_SDK_SOURCES}
  )

  target_include_directories(mamba_vst3 PRIVATE
    ${VST3_SDK_DIR}
  )

  target_link_libraries(mamba_vst3 PRIVATE
    mamba_core
    dl
  )

  target_compile_definitions(mamba_vst3 PRIVATE
    "-D_GLIBCXX_USE_CXX11_ABI=1"
    "-DNDEBUG"
  )

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(mamba_vst3 PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  set_target_properties(mamba_vst3 PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    OUTPUT_NAME "MambaDAW"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Release/VST3"
  )

  # Create VST3 bundle structure (required by most hosts)
  set(_VST3_BUNDLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/Release/MambaDAW.vst3")
  set(_VST3_BUNDLE_BIN "${_VST3_BUNDLE_DIR}/Contents/x86_64-linux")
  add_custom_command(TARGET mamba_vst3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_VST3_BUNDLE_BIN}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_BINARY_DIR}/Release/VST3/MambaDAW.so"
      "${_VST3_BUNDLE_BIN}/MambaDAW.so"
    COMMENT "Creating VST3 bundle: MambaDAW.vst3"
  )

  unset(_VST3_SDK_SOURCES)
  unset(_VST3_WRAPPER_SOURCES)
  unset(_VST3_BUNDLE_DIR)
  unset(_VST3_BUNDLE_BIN)
endif()

# ===========================================================================
# LV2 plugin wrapper — mamba_lv2  (Phase 3, done)
# ===========================================================================
if(MAMBA_ENABLE_LV2)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LV2 REQUIRED lv2)

  set(_LV2_WRAPPER_SOURCES src/plugin_wrapper_lv2.cpp)

  add_library(mamba_lv2 MODULE ${_LV2_WRAPPER_SOURCES})

  target_include_directories(mamba_lv2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
    ${LV2_INCLUDE_DIRS}
  )

  target_link_libraries(mamba_lv2 PRIVATE
    mamba_core
    ${LV2_LIBRARIES}
  )

  target_compile_definitions(mamba_lv2 PRIVATE
    "-D_GLIBCXX_USE_CXX11_ABI=1"
  )

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(mamba_lv2 PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  set_target_properties(mamba_lv2 PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    OUTPUT_NAME "MambaDAW"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Release/LV2"
  )

  # Create LV2 bundle structure
  set(_LV2_BUNDLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/Release/MambaDAW.lv2")
  set(_LV2_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources/MambaDAW.lv2")
  add_custom_command(TARGET mamba_lv2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_LV2_BUNDLE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_BINARY_DIR}/Release/LV2/MambaDAW.so"
      "${_LV2_BUNDLE_DIR}/MambaDAW.so"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${_LV2_RESOURCE_DIR}/manifest.ttl"
      "${_LV2_BUNDLE_DIR}/manifest.ttl"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${_LV2_RESOURCE_DIR}/MambaDAW.ttl"
      "${_LV2_BUNDLE_DIR}/MambaDAW.ttl"
    COMMENT "Creating LV2 bundle: MambaDAW.lv2"
  )

  unset(_LV2_WRAPPER_SOURCES)
  unset(_LV2_BUNDLE_DIR)
  unset(_LV2_RESOURCE_DIR)
endif()

# ===========================================================================
# Test targets — all link mamba_core instead of duplicating source lists
# ===========================================================================
enable_testing()

# --- helper: collect optional core sources for tests that need them ---
# (Some tests were compiled directly against .cpp files; now they link mamba_core
#  which includes everything the test needs.)

# --- test_dsp ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_dsp.cpp")
  add_executable(test_dsp tests/test_dsp.cpp)
  target_include_directories(test_dsp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
  )
  target_link_libraries(test_dsp PRIVATE mamba_core)
  target_compile_options(test_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- test_inference_thread ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_inference_thread.cpp")
  add_executable(test_inference_thread tests/test_inference_thread.cpp)
  target_include_directories(test_inference_thread PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
  )
  target_link_libraries(test_inference_thread PRIVATE mamba_core)
  target_compile_options(test_inference_thread PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- test_mamba_layer ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mamba_layer.cpp")
  add_executable(test_mamba_layer tests/test_mamba_layer.cpp)
  target_include_directories(test_mamba_layer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
  )
  target_link_libraries(test_mamba_layer PRIVATE mamba_core)
  target_compile_options(test_mamba_layer PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- param_gate_test ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/param_gate_test.cpp")
  add_executable(param_gate_test tests/param_gate_test.cpp)
  target_include_directories(param_gate_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
  )
  target_link_libraries(param_gate_test PRIVATE mamba_core)
  target_compile_options(param_gate_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- bench_mamba ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/bench_mamba.cpp")
  add_executable(bench_mamba tests/bench_mamba.cpp)
  target_include_directories(bench_mamba PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
  )
  target_link_libraries(bench_mamba PRIVATE mamba_core)
  target_compile_options(bench_mamba PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- test_state_serialization ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_state_serialization.cpp")
  add_executable(test_state_serialization tests/test_state_serialization.cpp)
  target_include_directories(test_state_serialization PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
  )
  target_link_libraries(test_state_serialization PRIVATE mamba_core)
  target_compile_options(test_state_serialization PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- test_alloc_audit ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_alloc_audit.cpp")
  add_executable(test_alloc_audit tests/test_alloc_audit.cpp)
  target_include_directories(test_alloc_audit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
  )
  target_link_libraries(test_alloc_audit PRIVATE mamba_core)
  target_compile_options(test_alloc_audit PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- test_tsan ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tsan.cpp")
  add_executable(test_tsan tests/test_tsan.cpp)
  target_include_directories(test_tsan PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
  )
  target_link_libraries(test_tsan PRIVATE mamba_core)
  target_compile_options(test_tsan PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- test_spectral_feedback ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_spectral_feedback.cpp")
  add_executable(test_spectral_feedback tests/test_spectral_feedback.cpp)
  target_include_directories(test_spectral_feedback PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
  )
  target_link_libraries(test_spectral_feedback PRIVATE mamba_core)
  target_compile_options(test_spectral_feedback PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- test_latency ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_latency.cpp")
  add_executable(test_latency tests/test_latency.cpp)
  target_include_directories(test_latency PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
  )
  target_link_libraries(test_latency PRIVATE mamba_core)
  target_compile_options(test_latency PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- test_noise_floor ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_noise_floor.cpp")
  add_executable(test_noise_floor tests/test_noise_floor.cpp)
  target_include_directories(test_noise_floor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
  )
  target_link_libraries(test_noise_floor PRIVATE mamba_core)
  target_compile_options(test_noise_floor PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- test_soak ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_soak.cpp")
  add_executable(test_soak tests/test_soak.cpp)
  target_include_directories(test_soak PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
  )
  target_link_libraries(test_soak PRIVATE mamba_core)
  target_compile_options(test_soak PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- test_cpu_budget ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cpu_budget.cpp"
    AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/cpu_budget_monitor.h")
  add_executable(test_cpu_budget tests/test_cpu_budget.cpp)
  target_include_directories(test_cpu_budget PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
  )
  target_link_libraries(test_cpu_budget PRIVATE mamba_core)
  target_compile_options(test_cpu_budget PRIVATE -Wall -Wextra -Wpedantic)
endif()

# For production builds, you may want to disable logging entirely:
# target_compile_definitions(mamba_core PRIVATE NDEBUG)
