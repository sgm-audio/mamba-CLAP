cmake_minimum_required(VERSION 3.14)
project(MambaClapPlugin LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check for required environment variables
if(NOT DEFINED ENV{CLAP_PATH})
    message(FATAL_ERROR "Environment variable CLAP_PATH must be set to the CLAP installation directory")
endif()
if(NOT DEFINED ENV{TORCH_INSTALL_PATH})
    message(FATAL_ERROR "Environment variable TORCH_INSTALL_PATH must be set to the LibTorch installation directory")
endif()

# Set paths from environment variables
set(CLAP_PATH "$ENV{CLAP_PATH}")
set(TORCH_INSTALL_PATH "$ENV{TORCH_INSTALL_PATH}")

# Set CLAP include and library directories
set(CLAP_INCLUDE_DIRS "${CLAP_PATH}/include")
set(CLAP_LIBRARIES "${CLAP_PATH}/lib/libclap.so")

# Set Torch include and library directories
set(TORCH_INCLUDE_DIRS "${TORCH_INSTALL_PATH}/include"
                       "${TORCH_INSTALL_PATH}/include/torch/csrc/api/include")
set(TORCH_LIBRARIES "${TORCH_INSTALL_PATH}/lib/libtorch_cpu.so"
                    "${TORCH_INSTALL_PATH}/lib/libtorch.so")

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
    ${CLAP_INCLUDE_DIRS}
    ${TORCH_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CLAP_LIBRARIES}
    ${TORCH_LIBRARIES}
    dl
    pthread
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