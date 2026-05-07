cmake_minimum_required(VERSION 3.20)
project(MambaFX VERSION 1.0.0 LANGUAGES CXX)

# ─── Language standard ────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Options ─────────────────────────────────────────────────────────────────
option(MAMBA_ENABLE_CUDA    "Enable CUDA inference backend"      OFF)
option(MAMBA_ENABLE_ARA     "Build with ARA 2 support"           ON)
option(MAMBA_ENABLE_UI      "Build plugin editor UI"             ON)
option(MAMBA_BUILD_TESTS    "Build unit tests"                   OFF)
option(MAMBA_SANITIZE       "Enable ASAN/TSAN in debug builds"   OFF)

# ─── External dependencies ───────────────────────────────────────────────────
# CLAP SDK ≥ 1.2  (header-only)
find_path(CLAP_INCLUDE_DIR clap/clap.h
    HINTS $ENV{CLAP_PATH}/include
    REQUIRED)

# LibTorch (C++ frontend for PyTorch)
find_package(Torch REQUIRED HINTS $ENV{TORCH_INSTALL_PATH})

if(MAMBA_ENABLE_ARA)
    # ARA 2 SDK ≥ 2.2  (header-only core + optional JUCE helper)
    find_path(ARA_INCLUDE_DIR ARA_API/ARAAPI.h
        HINTS $ENV{ARA_SDK_PATH}/ARA_API
              $ENV{ARA_SDK_PATH}
        REQUIRED)
    # clap-ara bridge (header-only)
    find_path(CLAP_ARA_INCLUDE_DIR clap-ara/clap_ara_bridge.h
        HINTS $ENV{CLAP_ARA_PATH}/include
        REQUIRED)
endif()

if(MAMBA_ENABLE_UI)
    # JUCE 7+ for cross-platform GUI
    find_package(JUCE REQUIRED HINTS $ENV{JUCE_PATH})
endif()

# ─── Source lists ─────────────────────────────────────────────────────────────
set(CORE_SOURCES
    core/PluginCore.cpp
    core/ParameterBank.cpp
    core/TransportState.cpp
)
set(CLAP_SOURCES
    clap/ClapPlugin.cpp
    clap/ClapExtensions.cpp
    clap/ClapFactory.cpp
)
set(MODEL_SOURCES
    model/MambaInferenceEngine.cpp
    model/MambaSession.cpp
    model/ModelLoader.cpp
    model/InferenceQueue.cpp
)
set(DSP_SOURCES
    dsp/STFTEngine.cpp
    dsp/OverlapAddSynthesizer.cpp
    dsp/EffectApplicator.cpp
)
set(STATE_SOURCES
    state/StateSerializer.cpp
    state/PresetBank.cpp
)
set(ARA_SOURCES
    ara/ARADocumentController.cpp
    ara/ARAPlaybackRenderer.cpp
    ara/ARAEditorRenderer.cpp
    ara/ARAEditorView.cpp
    ara/ARARegionAnalyzer.cpp
)
set(UI_SOURCES
    ui/EditorComponent.cpp
    ui/SpectrumDisplay.cpp
    ui/ParameterAttachment.cpp
)

set(ALL_SOURCES
    ${CORE_SOURCES}
    ${CLAP_SOURCES}
    ${MODEL_SOURCES}
    ${DSP_SOURCES}
    ${STATE_SOURCES}
)
if(MAMBA_ENABLE_ARA)
    list(APPEND ALL_SOURCES ${ARA_SOURCES})
endif()
if(MAMBA_ENABLE_UI)
    list(APPEND ALL_SOURCES ${UI_SOURCES})
endif()

# ─── Plugin target ────────────────────────────────────────────────────────────
add_library(MambaFX MODULE ${ALL_SOURCES})

set_target_properties(MambaFX PROPERTIES
    PREFIX   ""
    SUFFIX   ".clap"
    OUTPUT_NAME "MambaFX"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/MambaFX.clap"
    # Harden: no undefined symbols at link time
    LINK_FLAGS "-Wl,--no-undefined"
)

# ─── Include dirs ─────────────────────────────────────────────────────────────
target_include_directories(MambaFX PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CLAP_INCLUDE_DIR}
    ${TORCH_INCLUDE_DIRS}
)
if(MAMBA_ENABLE_ARA)
    target_include_directories(MambaFX PRIVATE
        ${ARA_INCLUDE_DIR}
        ${CLAP_ARA_INCLUDE_DIR}
    )
endif()

# ─── Compile definitions ─────────────────────────────────────────────────────
target_compile_definitions(MambaFX PRIVATE
    MAMBA_FX_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    MAMBA_FX_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    MAMBA_FX_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    _GLIBCXX_USE_CXX11_ABI=1
    TORCH_API_INCLUDE_EXTENSION_H
)
if(MAMBA_ENABLE_ARA)
    target_compile_definitions(MambaFX PRIVATE MAMBA_ENABLE_ARA=1)
endif()
if(MAMBA_ENABLE_CUDA)
    target_compile_definitions(MambaFX PRIVATE MAMBA_ENABLE_CUDA=1)
endif()

# ─── Compile options ─────────────────────────────────────────────────────────
target_compile_options(MambaFX PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wno-unused-parameter>
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Release>>:-O3 -march=native -ffast-math>
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Debug>>:-g3 -fno-omit-frame-pointer>
)
if(MAMBA_SANITIZE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(MambaFX PRIVATE -fsanitize=address,undefined)
    target_link_options(MambaFX PRIVATE -fsanitize=address,undefined)
endif()

# ─── Link libraries ──────────────────────────────────────────────────────────
target_link_libraries(MambaFX PRIVATE
    ${TORCH_LIBRARIES}
    dl pthread
)
if(MAMBA_ENABLE_UI)
    target_link_libraries(MambaFX PRIVATE
        juce::juce_gui_basics
        juce::juce_audio_processors
        juce::juce_dsp
    )
endif()

# ─── Tests ────────────────────────────────────────────────────────────────────
if(MAMBA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
