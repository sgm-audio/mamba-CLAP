cmake_minimum_required(VERSION 3.14)
project(MambaClapPlugin LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch and build CLAP from source
include(FetchContent)
FetchContent_Declare(
  clap
  GIT_REPOSITORY https://github.com/free-audio/clap.git
  GIT_TAG main
)
FetchContent_GetProperties(clap)
if(NOT clap_POPULATED)
  FetchContent_MakeAvailable(clap)  # modern approach
endif()

# Find Torch (optional — CPU-only path doesn't need it)
find_package(Torch QUIET)
if(Torch_FOUND)
  message(STATUS "Torch found: ${TORCH_INCLUDE_DIRS}")
endif()

# MambaDAW is pure CPU Mamba SSM; does not require FFTW.
# (FFTW is only needed by the legacy clap-plugin's STFT path)

# Source files
set(SOURCES
    src/plugin_wrapper.cpp
    src/plugin_processor.cpp
    src/mamba_layer.cpp
    src/mamba_model.cpp
    src/mamba_weight_loader.cpp
)

# Header files
set(HEADERS
    src/plugin_processor.h
    src/mamba_layer.h
    src/mamba_model.h
    src/mamba_weight_loader.h
    src/context_assembler.h
    src/parameter_gate.h
    src/feature_extractor_rt.h
)

# Create library
add_library(${PROJECT_NAME} MODULE ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
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

# For production builds, you may want to disable logging entirely:
# target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG)
