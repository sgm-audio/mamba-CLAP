cmake_minimum_required(VERSION 3.14)
project(ARAIntelligencePlugin LANGUAGES CXX)

# Find JUCE
# We assume JUCE is installed and we can find it via the JUCE module path.
# The user must set the JUCE_MODULE_PATH environment variable or we can try to find it.
# For simplicity, we'll look for JUCE in common locations or require it to be set via environment.
set(JUCE_INSTALL_PATH "" CACHE PATH "Path to JUCE installation")

if(NOT JUCE_INSTALL_PATH)
    # Try to find JUCE via environment variable or common locations
    if(DEFINED ENV{JUCE_INSTALL_PATH})
        set(JUCE_INSTALL_PATH $ENV{JUCE_INSTALL_PATH})
    endif()
endif()

# If we still don't have a path, we'll need to fetch JUCE or require it to be set.
# We'll assume the user has set JUCE_INSTALL_PATH or that JUCE is installed in a standard location.
# We'll use find_package(JUCE) if available, but note that JUCE doesn't provide a standard config file.
# Instead, we'll include the JUCE modules directly.

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    Source/ARAIntelligenceDocumentController.h
    Source/ARAIntelligenceDocumentController.cpp
)

# Header files (for IDE)
set(HEADERS
    Source/ARAIntelligenceDocumentController.h
)

# Create library
add_library(${PROJECT_NAME} MODULE ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    # We assume the user has set JUCE_INSTALL_PATH and that the JUCE modules are in JUCE_INSTALL_PATH/modules
    # We'll add the JUCE module paths manually. In a real JUCE project, you would use the JUCE CMake helper.
    # For simplicity, we'll note that the user must set the JUCE module path appropriately.
    # We'll try to find the JUCE modules by looking for the JuceHeader.h in the JUCE_INSTALL_PATH.
    # This is a simplified approach. In practice, you would use the JUCE CMake helper provided by JUCE.
    # Since we don't have that, we'll rely on the user to set the include path via the environment or by modifying this CMakeLists.txt.
    # We'll add a note and let the user adjust.
)

# Link libraries
# We link against the JUCE modules. Since we don't have the JUCE CMake helper, we'll assume the user has set up the include paths
# and we'll link against the JUCE libraries that are built as part of the JUCE framework.
# However, note that JUCE is typically built as a static library and included via source.
# For simplicity, we'll note that the user must build JUCE as a static library and link against it, or use the JUCE modules as source.
# We'll leave a comment and let the user adjust.

# We'll try to find the JUCE library if it's installed, but note that the ARA plugin requires JUCE to be built with certain modules.
# We'll link against the JUCE core, gui, gui_basics, audio_basics, audio_devices, audio_processors, audio_utils, dsp, and ara_utils.
# We'll assume the user has built JUCE as a static library or we can find the components.

# For now, we'll link against the JUCE libraries that we expect to be available.
# We'll also link against the ARA SDK if it's available as a library.

# Note: The user must have the ARA SDK installed and set the path to its headers and libraries.
# We'll assume the user has set the ARA_SDK_PATH environment variable or we can try to find it.

set(ARA_SDK_PATH "" CACHE PATH "Path to ARA SDK installation")

if(NOT ARA_SDK_PATH)
    if(DEFINED ENV{ARA_SDK_PATH})
        set(ARA_SDK_PATH $ENV{ARA_SDK_PATH})
    endif()
endif()

# Include directories for ARA SDK
if(ARA_SDK_PATH AND EXISTS "${ARA_SDK_PATH}/Include")
    target_include_directories(${PROJECT_NAME} PRIVATE "${ARA_SDK_PATH}/Include")
endif()

# Link against the ARA SDK library if we can find it
if(ARA_SDK_PATH AND EXISTS "${ARA_SDK_PATH}/Lib/ara")
    target_link_directories(${PROJECT_NAME} PRIVATE "${ARA_SDK_PATH}/Lib")
    target_link_libraries(${PROJECT_NAME} PRIVATE ara)
elseif(ARA_SDK_PATH AND EXISTS "${ARA_SDK_PATH}/lib/ara")
    target_link_directories(${PROJECT_NAME} PRIVATE "${ARA_SDK_PATH}/lib")
    target_link_libraries(${PROJECT_NAME} PRIVATE ara)
endif()

# We also need to link against the Rust engine. We'll assume the user has built the Rust engine and set the path to the shared library.
set(RUST_ENGINE_PATH "" CACHE PATH "Path to the directory containing the shared library for the Rust engine (ara_intelligence_engine)")

if(NOT RUST_ENGINE_PATH)
    if(DEFINED ENV{RUST_ENGINE_PATH})
        set(RUST_ENGINE_PATH $ENV{RUST_ENGINE_PATH})
    endif()
endif()

# We don't link against the Rust engine at compile time because it's loaded dynamically via FFI.
# Instead, we'll note that the user must ensure the shared library is available at runtime.
# We'll add a copy command to put the shared library in the same directory as the plugin if the user provides the path.
if(RUST_ENGINE_PATH AND EXISTS "${RUST_ENGINE_PATH}")
    # Determine the shared library extension based on the system
    if(UNIX AND NOT APPLE)
        set(RUST_ENGINE_LIB "libara_intelligence_engine.so")
    elseif(APPLE)
        set(RUST_ENGINE_LIB "libara_intelligence_engine.dylib")
    elseif(WIN32)
        set(RUST_ENGINE_LIB "ara_intelligence_engine.dll")
    endif()

    if(EXISTS "${RUST_ENGINE_PATH}/${RUST_ENGINE_LIB}")
        # Add a custom command to copy the shared library to the output directory
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${RUST_ENGINE_PATH}/${RUST_ENGINE_LIB}"
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    else()
        message(WARNING "Rust engine library not found at ${RUST_ENGINE_PATH}/${RUST_ENGINE_LIB}. The plugin will fail to load the engine at runtime.")
    endif()
else()
    message(WARNING "Rust engine path not set. The plugin will fail to load the engine at runtime. Set RUST_ENGINE_PATH to the directory containing the shared library.")
endif()

# Set properties for plugin
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Release"
)

# Add a note about JUCE and ARA SDK
message("NOTE: This CMakeLists.txt assumes you have JUCE and the ARA SDK properly set up in your build environment.")
message("For a complete JUCE integration, you would typically:")
message("1. Download JUCE from https://juce.com/get-juce")
message("2. Add the JUCE modules to your project (or build JUCE as a static library and link against it)")
message("3. Link against the required JUCE modules (core, gui, gui_basics, audio_basics, audio_devices, audio_processors, audio_utils, dsp, ara_utils)")
message("")
message("For the ARA SDK, you must have it installed and set the ARA_SDK_PATH environment variable or adjust the CMakeLists.txt.")
message("")
message("For the Rust engine, you must build the Rust crate 'ara_intelligence_engine' to produce a shared library and set the RUST_ENGINE_PATH environment variable to the directory containing that library.")
message("")
message("Example setup for JUCE (using the JUCE CMake helper):")
message("  # In your main CMakeLists.txt, you would do:")
message("  find_package(JUCE REQUIRED)")
message("  # Then link against JUCE::JUCE")
message("")
message("Example for building the Rust engine:")
message("  cd ara-intelligence-engine")
message("  cargo build --release")
message("  # The shared library will be in target/release/")
message("")
message("Then set the environment variables:")
message("  export JUCE_INSTALL_PATH=/path/to/juce")
message("  export ARA_SDK_PATH=/path/to/ara/sdk")
message("  export RUST_ENGINE_PATH=/path/to/ara-intelligence-engine/target/release")
message("")
message("Then run cmake and make as usual.")
