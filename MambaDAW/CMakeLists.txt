cmake_minimum_required(VERSION 3.16)
project(MambaDAW LANGUAGES CXX)

# =============================================================================
# Build options
# =============================================================================
option(USE_CUDA "Enable CUDA/LibTorch backend (GPU path, off by default)" OFF)

# =============================================================================
# C++ standard
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# CLAP SDK (header-only) — auto-fetched if not found locally
# =============================================================================
include(FetchContent)

find_path(CLAP_INCLUDE_DIR clap/clap.h
    HINTS
        /usr/local/include
        /usr/include
        /opt/homebrew/include
        ${CMAKE_SOURCE_DIR}/../clap-sdk/include
        $ENV{CLAP_SDK_PATH}/include
)

if(NOT CLAP_INCLUDE_DIR)
    message(STATUS "CLAP SDK not found locally — fetching from GitHub...")
    FetchContent_Declare(
        clap
        GIT_REPOSITORY https://github.com/free-audio/clap.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(clap)
    set(CLAP_INCLUDE_DIR ${clap_SOURCE_DIR}/include)
    message(STATUS "CLAP SDK fetched to: ${CLAP_INCLUDE_DIR}")
endif()

# =============================================================================
# FFTW3 single-precision (optional: only needed for AudioFrontend STFT path)
# AudioFrontend is NOT in the RT build path after the state-centric refactor.
# =============================================================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW3F fftw3f)  # Not REQUIRED — warn, don't fail
if(NOT FFTW3F_FOUND)
    message(WARNING
        "libfftw3f not found — AudioFrontend STFT path will be disabled.\n"
        "Install if you need the offline STFT preprocessing path:\n"
        "  Ubuntu/Debian: sudo apt-get install libfftw3-dev\n"
        "  Fedora/RHEL:   sudo dnf install fftw-devel\n"
        "  macOS:         brew install fftw")
endif()

# =============================================================================
# Optional: LibTorch for GPU/TorchScript path
# =============================================================================
if(USE_CUDA)
    find_package(Torch REQUIRED)
    add_compile_definitions(USE_TORCH_BACKEND)
    # Older GCC/Clang need pre-CXX11 ABI to match LibTorch
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)
    endif()
endif()

# =============================================================================
# Source files — ALL compilation units included
# =============================================================================
set(SOURCES
    src/main.cpp
    src/plugin_wrapper.cpp
    src/plugin_processor.cpp
    src/mamba_model.cpp
    src/mamba_layer.cpp
    src/mamba_weight_loader.cpp
)

# =============================================================================
# CLAP plugin target
# =============================================================================
add_library(MambaDAW MODULE ${SOURCES})

set_target_properties(MambaDAW PROPERTIES
    PREFIX  ""
    SUFFIX  ".clap"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/release"
)

target_include_directories(MambaDAW PRIVATE
    ${CLAP_INCLUDE_DIR}
    ${FFTW3F_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Aggressive optimisation — mandatory for real-time SSM performance
target_compile_options(MambaDAW PRIVATE
    -O3
    -march=native       # Enable AVX2/AVX-512 on target CPU
    -ffast-math         # Allows SIMD FP reassociation — safe for audio DSP
    -fno-exceptions     # RT code must not throw
    -Wall
    -Wextra
    -Wno-unused-parameter
)

if(FFTW3F_FOUND)
    target_link_libraries(MambaDAW PRIVATE ${FFTW3F_LIBRARIES})
    target_compile_definitions(MambaDAW PRIVATE HAVE_FFTW3)
endif()

if(USE_CUDA)
    target_link_libraries(MambaDAW PRIVATE ${TORCH_LIBRARIES})
endif()

# =============================================================================
# Install
# =============================================================================
include(GNUInstallDirs)
install(TARGETS MambaDAW
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/clap")

# =============================================================================
# Build summary
# =============================================================================
message(STATUS "=== MambaDAW Build Configuration ===")
message(STATUS "  C++ standard : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type   : ${CMAKE_BUILD_TYPE}")
message(STATUS "  CUDA backend : ${USE_CUDA}")
message(STATUS "  CLAP headers : ${CLAP_INCLUDE_DIR}")
message(STATUS "  FFTW3f libs  : ${FFTW3F_LIBRARIES}")
message(STATUS "=====================================")