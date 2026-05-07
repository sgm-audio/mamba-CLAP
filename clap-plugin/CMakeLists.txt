cmake_minimum_required(VERSION 3.14)
project(MambaClapPlugin LANGUAGES CXX)

# Find JUCE - we'll assume JUCE is installed or we'll fetch it
# For simplicity, we'll look for JUCE in common locations or require it to be set via environment
set(JUCE_INSTALL_PATH "" CACHE PATH "Path to JUCE installation")

if(NOT JUCE_INSTALL_PATH)
    # Try to find JUCE via environment variable or common locations
    if(DEFINED ENV{JUCE_INSTALL_PATH})
        set(JUCE_INSTALL_PATH $ENV{JUCE_INSTALL_PATH})
    endif()
endif()

# If we still don't have a path, we'll need to fetch JUCE
if(NOT JUCE_INSTALL_PATH OR NOT EXISTS "${JUCE_INSTALL_PATH}")
    message(WARNING "JUCE not found at ${JUCE_INSTALL_PATH}. Please set JUCE_INSTALL_PATH environment variable or modify CMakeLists.txt.")
    # For now, we'll create a minimal CMakeLists.txt that assumes JUCE is available via find_package
    # In a real scenario, you'd want to use FetchContent to download JUCE or require it to be installed
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Torch
find_package(Torch REQUIRED)

# Source files
set(SOURCES
    src/clap-plugin.cpp
    juce_gui/JucePluginProcessor.cpp
    juce_gui/JucePluginEditor.cpp
)

# Header files
set(HEADERS
    include/clap-plugin.hpp
    juce_gui/JucePluginProcessor.h
    juce_gui/JucePluginEditor.h
)

# Create library
add_library(${PROJECT_NAME} MODULE ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/juce_gui
    ${TORCH_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
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

# Add a note about JUCE
message("NOTE: This CMakeLists.txt assumes you have JUCE properly set up in your build environment.")
message("For a complete JUCE integration, you would typically:")
message("1. Download JUCE from https://juce.com/get-juce")
message("2. Add the JUCE modules to your project")
message("3. Link against JUCE::JUCE in target_link_libraries")
message("4. Include the JUCE headers")
