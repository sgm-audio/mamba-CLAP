cmake_minimum_required(VERSION 3.14)
project(MambaClapPlugin LANGUAGES C CXX)

# NOT IMPLEMENTED (post-MVP): GPU/CUDA inference path (FR4.4/NFR2/NFR7)
# NOT IMPLEMENTED (post-MVP): Windows/macOS builds (NFR8) — Linux-only for MVP
# NOT IMPLEMENTED (post-MVP): VST3/AU wrapper (NFR10) — CLAP only for MVP

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

# Fetch and build CLAP from source
include(FetchContent)
FetchContent_Declare(
  clap
  GIT_REPOSITORY https://github.com/free-audio/clap.git
    GIT_TAG 1.2.2
)
FetchContent_GetProperties(clap)
if(NOT clap_POPULATED)
  FetchContent_MakeAvailable(clap)  # modern approach
endif()

# ---------------------------------------------------------------------------
# JUCE GUI (set -DMAMBA_ENABLE_GUI=OFF for headless/CI builds without JUCE)
# Bypasses JUCE's own CMake system: compile only the unity .cpp files directly.
# JUCE 8's add_subdirectory adds individual sub-files as separate TUs, which
# breaks JUCE's unity-build model where sub-files are only #include'd internally.
# ---------------------------------------------------------------------------
option(MAMBA_ENABLE_GUI "Build with JUCE GUI" ON)
if(MAMBA_ENABLE_GUI)
    set(JUCE_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/modules")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FREETYPE   REQUIRED freetype2)
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
    find_package(X11 REQUIRED)
    list(APPEND SOURCES
        ${JUCE_MODULES_DIR}/juce_core/juce_core.cpp
        ${JUCE_MODULES_DIR}/juce_events/juce_events.cpp
        ${JUCE_MODULES_DIR}/juce_data_structures/juce_data_structures.cpp
        ${JUCE_MODULES_DIR}/juce_graphics/juce_graphics.cpp
        ${JUCE_MODULES_DIR}/juce_gui_basics/juce_gui_basics.cpp
    )
endif()

# Find Torch (optional — CPU-only path doesn't need it)
find_package(Torch QUIET)
if(Torch_FOUND)
  message(STATUS "Torch found: ${TORCH_INCLUDE_DIRS}")
endif()

# MambaDAW is pure CPU Mamba SSM; does not require FFTW.
# (FFTW is only needed by the legacy clap-plugin's STFT path)

# Core plugin sources (always required)
set(SOURCES
  src/plugin_wrapper.cpp
  src/plugin_processor.cpp
  src/mamba_layer.cpp
  src/mamba_model.cpp
  src/mamba_weight_loader.cpp
  src/parameter_gate.cpp
  src/state_serialization.cpp
)

# Parallel-work / DSP / inference .cpp files — included only when present (no stub sources)
set(_MAMBA_OPTIONAL_SOURCES
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
foreach(_src IN LISTS _MAMBA_OPTIONAL_SOURCES)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
    list(APPEND SOURCES "${_src}")
  endif()
endforeach()
unset(_MAMBA_OPTIONAL_SOURCES)
unset(_src)

# GUI sources — appended to SOURCES only when MAMBA_ENABLE_GUI=ON and each file exists.
if(MAMBA_ENABLE_GUI)
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
            list(APPEND SOURCES "${_src}")
        endif()
    endforeach()
    unset(_GUI_SOURCES)
    unset(_src)
endif()

# Header files
set(HEADERS
  src/plugin_processor.h
  src/mamba_layer.h
  src/mamba_model.h
  src/mamba_weight_loader.h
  src/context_assembler.h
  src/parameter_gate.h
  src/param_smoother.h
  src/feature_extractor_rt.h
  src/state_serialization.h
)

# Create library
add_library(${PROJECT_NAME} MODULE ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

if(Torch_FOUND)
  target_include_directories(${PROJECT_NAME} PRIVATE ${TORCH_INCLUDE_DIRS})
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    clap  # The target from the CLAP subdirectory
    dl
    pthread
)

if(Torch_FOUND)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${TORCH_LIBRARIES})
endif()

if(MAMBA_ENABLE_GUI)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${JUCE_MODULES_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
        ${FREETYPE_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${X11_LIBRARIES}
        ${FREETYPE_LIBRARIES}
        ${FONTCONFIG_LIBRARIES}
        GL
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        MAMBA_ENABLE_GUI=1
        JUCE_STANDALONE_APPLICATION=0
        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_MODULE_AVAILABLE_juce_core=1
        JUCE_MODULE_AVAILABLE_juce_events=1
        JUCE_MODULE_AVAILABLE_juce_data_structures=1
        JUCE_MODULE_AVAILABLE_juce_graphics=1
        JUCE_MODULE_AVAILABLE_juce_gui_basics=1
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    )
endif()

# Set properties for plugin
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Release"
)

# Add compiler definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "-D_GLIBCXX_USE_CXX11_ABI=1"
)

# Optional: enable more warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Test targets
# All test targets need the generated config header directory.
set(_GENERATED_INC_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(_DSP_TEST_SOURCES
  src/dsp/eq.cpp
  src/dsp/compressor.cpp
  src/dsp/saturator.cpp
  src/dsp/reverb.cpp
  src/dsp/dsp_chain.cpp
  src/dsp/spectral_feedback.cpp
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_dsp.cpp")
    set(_dsp_test_all_ok TRUE)
    foreach(_f IN LISTS _DSP_TEST_SOURCES)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
            set(_dsp_test_all_ok FALSE)
        endif()
    endforeach()
    if(_dsp_test_all_ok)
        add_executable(test_dsp tests/test_dsp.cpp ${_DSP_TEST_SOURCES})
    target_include_directories(test_dsp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${_GENERATED_INC_DIR}
    )
    target_compile_options(test_dsp PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    unset(_dsp_test_all_ok)
endif()
unset(_DSP_TEST_SOURCES)

set(_INF_TEST_SOURCES
    src/feature_extractor.cpp
    src/effect_decoder.cpp
    src/inference_thread.cpp
    src/mamba_layer.cpp
    src/mamba_model.cpp
    src/mamba_weight_loader.cpp
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_inference_thread.cpp")
    set(_inf_test_all_ok TRUE)
    foreach(_f IN LISTS _INF_TEST_SOURCES)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
            set(_inf_test_all_ok FALSE)
        endif()
    endforeach()
    if(_inf_test_all_ok)
        add_executable(test_inference_thread tests/test_inference_thread.cpp ${_INF_TEST_SOURCES})
target_include_directories(test_inference_thread PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${_GENERATED_INC_DIR}
    )
    target_link_libraries(test_inference_thread PRIVATE pthread)
        target_compile_options(test_inference_thread PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    unset(_inf_test_all_ok)
endif()
unset(_INF_TEST_SOURCES)

# --- test_mamba_layer ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mamba_layer.cpp"
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/mamba_layer.cpp")
add_executable(test_mamba_layer
tests/test_mamba_layer.cpp
src/mamba_layer.cpp
)
target_include_directories(test_mamba_layer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${_GENERATED_INC_DIR}
    )
target_compile_options(test_mamba_layer PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- param_gate_test ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/param_gate_test.cpp"
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/parameter_gate.cpp")
add_executable(param_gate_test
tests/param_gate_test.cpp
src/parameter_gate.cpp
)
target_include_directories(param_gate_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${_GENERATED_INC_DIR}
    )
target_compile_options(param_gate_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- bench_mamba ---
set(_BENCH_SOURCES
src/mamba_layer.cpp
src/mamba_model.cpp
src/mamba_weight_loader.cpp
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/bench_mamba.cpp")
set(_bench_all_ok TRUE)
foreach(_f IN LISTS _BENCH_SOURCES)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
set(_bench_all_ok FALSE)
endif()
endforeach()
if(_bench_all_ok)
add_executable(bench_mamba tests/bench_mamba.cpp ${_BENCH_SOURCES})
target_include_directories(bench_mamba PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${_GENERATED_INC_DIR}
    )
target_compile_options(bench_mamba PRIVATE -Wall -Wextra -Wpedantic)
endif()
unset(_bench_all_ok)
endif()
unset(_BENCH_SOURCES)

# --- test_state_serialization ---
set(_STATE_SER_SOURCES
  src/mamba_layer.cpp
  src/mamba_model.cpp
  src/state_serialization.cpp
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_state_serialization.cpp")
  set(_ss_test_all_ok TRUE)
  foreach(_f IN LISTS _STATE_SER_SOURCES)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
      set(_ss_test_all_ok FALSE)
    endif()
  endforeach()
  if(_ss_test_all_ok)
    add_executable(test_state_serialization tests/test_state_serialization.cpp ${_STATE_SER_SOURCES})
target_include_directories(test_state_serialization PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${_GENERATED_INC_DIR}
    )
    target_compile_options(test_state_serialization PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  unset(_ss_test_all_ok)
endif()
unset(_STATE_SER_SOURCES)

# --- test_alloc_audit ---
set(_ALLOC_AUDIT_SOURCES
  src/dsp/eq.cpp
  src/dsp/compressor.cpp
  src/dsp/saturator.cpp
  src/dsp/reverb.cpp
  src/dsp/dsp_chain.cpp
  src/dsp/spectral_feedback.cpp
  src/feature_extractor.cpp
  src/effect_decoder.cpp
  src/inference_thread.cpp
  src/mamba_layer.cpp
  src/mamba_model.cpp
  src/mamba_weight_loader.cpp
  src/parameter_gate.cpp
  src/state_serialization.cpp
  src/plugin_processor.cpp
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_alloc_audit.cpp")
	set(_aa_all_ok TRUE)
	foreach(_f IN LISTS _ALLOC_AUDIT_SOURCES)
		if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
			set(_aa_all_ok FALSE)
		endif()
	endforeach()
	if(_aa_all_ok)
		add_executable(test_alloc_audit tests/test_alloc_audit.cpp ${_ALLOC_AUDIT_SOURCES})
target_include_directories(test_alloc_audit PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${_GENERATED_INC_DIR}
    )
		target_link_libraries(test_alloc_audit PRIVATE pthread)
		target_compile_options(test_alloc_audit PRIVATE -Wall -Wextra -Wpedantic)
	endif()
	unset(_aa_all_ok)
endif()
unset(_ALLOC_AUDIT_SOURCES)

# --- test_tsan ---
set(_TSAN_SOURCES
	src/inference_thread.cpp
	src/effect_decoder.cpp
	src/mamba_layer.cpp
	src/mamba_model.cpp
	src/mamba_weight_loader.cpp
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tsan.cpp")
	set(_ts_all_ok TRUE)
	foreach(_f IN LISTS _TSAN_SOURCES)
		if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
			set(_ts_all_ok FALSE)
		endif()
	endforeach()
	if(_ts_all_ok)
		add_executable(test_tsan tests/test_tsan.cpp ${_TSAN_SOURCES})
target_include_directories(test_tsan PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${_GENERATED_INC_DIR}
    )
		target_link_libraries(test_tsan PRIVATE pthread)
		target_compile_options(test_tsan PRIVATE -Wall -Wextra -Wpedantic)
	endif()
	unset(_ts_all_ok)
endif()
unset(_TSAN_SOURCES)

# --- test_spectral_feedback ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_spectral_feedback.cpp"
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/spectral_feedback.cpp")
  add_executable(test_spectral_feedback
    tests/test_spectral_feedback.cpp
    src/dsp/spectral_feedback.cpp
  )
target_include_directories(test_spectral_feedback PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${_GENERATED_INC_DIR}
    )
  target_compile_options(test_spectral_feedback PRIVATE -Wall -Wextra -Wpedantic)
endif()

# For production builds, you may want to disable logging entirely:
# target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG)

# --- test_latency ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_latency.cpp")
set(_LAT_TEST_SOURCES
  src/dsp/eq.cpp
  src/dsp/compressor.cpp
  src/dsp/saturator.cpp
  src/dsp/reverb.cpp
  src/dsp/dsp_chain.cpp
  src/dsp/spectral_feedback.cpp
)
set(_lat_all_ok TRUE)
foreach(_f IN LISTS _LAT_TEST_SOURCES)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
    set(_lat_all_ok FALSE)
  endif()
endforeach()
if(_lat_all_ok)
  add_executable(test_latency tests/test_latency.cpp ${_LAT_TEST_SOURCES})
target_include_directories(test_latency PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${_GENERATED_INC_DIR}
    )
  target_compile_options(test_latency PRIVATE -Wall -Wextra -Wpedantic)
endif()
unset(_lat_all_ok)
unset(_LAT_TEST_SOURCES)
endif()

# --- test_noise_floor ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_noise_floor.cpp")
set(_NF_TEST_SOURCES
  src/dsp/eq.cpp
  src/dsp/compressor.cpp
  src/dsp/saturator.cpp
  src/dsp/reverb.cpp
  src/dsp/dsp_chain.cpp
  src/dsp/spectral_feedback.cpp
)
set(_nf_all_ok TRUE)
foreach(_f IN LISTS _NF_TEST_SOURCES)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
    set(_nf_all_ok FALSE)
  endif()
endforeach()
if(_nf_all_ok)
  add_executable(test_noise_floor tests/test_noise_floor.cpp ${_NF_TEST_SOURCES})
target_include_directories(test_noise_floor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${_GENERATED_INC_DIR}
    )
  target_compile_options(test_noise_floor PRIVATE -Wall -Wextra -Wpedantic)
endif()
unset(_nf_all_ok)
unset(_NF_TEST_SOURCES)
endif()

# --- test_soak ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_soak.cpp")
set(_SOAK_TEST_SOURCES
src/dsp/eq.cpp
src/dsp/compressor.cpp
src/dsp/saturator.cpp
src/dsp/reverb.cpp
src/dsp/dsp_chain.cpp
src/dsp/spectral_feedback.cpp
)
set(_soak_all_ok TRUE)
foreach(_f IN LISTS _SOAK_TEST_SOURCES)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
set(_soak_all_ok FALSE)
endif()
endforeach()
if(_soak_all_ok)
add_executable(test_soak tests/test_soak.cpp ${_SOAK_TEST_SOURCES})
target_include_directories(test_soak PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${_GENERATED_INC_DIR}
    )
target_compile_options(test_soak PRIVATE -Wall -Wextra -Wpedantic)
endif()
unset(_soak_all_ok)
unset(_SOAK_TEST_SOURCES)
endif()

# --- test_cpu_budget ---
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cpu_budget.cpp"
    AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/cpu_budget_monitor.h")
add_executable(test_cpu_budget tests/test_cpu_budget.cpp)
target_include_directories(test_cpu_budget PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_GENERATED_INC_DIR}
)
target_compile_options(test_cpu_budget PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(test_cpu_budget PRIVATE pthread)
endif()
