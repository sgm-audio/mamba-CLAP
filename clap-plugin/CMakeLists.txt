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
  GIT_TAG v1.2.7
)
FetchContent_GetProperties(clap)
if(NOT clap_POPULATED)
  FetchContent_Populate(clap)
  add_subdirectory(${clap_SOURCE_DIR} ${clap_BINARY_DIR})
endif()

# Find Torch
find_package(Torch REQUIRED)
find_package(FFTW3 REQUIRED COMPONENTS fftw3f)

# Source files
set(SOURCES
    src/clap-plugin.cpp
)

# Header files
set(HEADERS
    include/clap-plugin.hpp
)

# Create library
add_library(${PROJECT_NAME} MODULE ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${TORCH_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${TORCH_LIBRARIES}
    clap  # The target from the CLAP subdirectory
    dl
    pthread
    FFTW3::fftw3f
)

# Set properties for plugin
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Release"
)

# Add compiler definitions for torch
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "-DTORCH_API_INCLUDE_EXTENSION_H"
    "-DTORCH_EXTENSION_NAME=MambaClapPlugin"
    "-D_GLIBCXX_USE_CXX11_ABI=1"
)