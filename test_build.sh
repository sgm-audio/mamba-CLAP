#!/bin/bash

# Simple build test script for the MambaFX CLAP plugin

set -e  # Exit on any error

echo "Building MambaFX CLAP plugin..."

# Change to clap-plugin directory
cd clap-plugin

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake ..

# Build the plugin
echo "Building plugin..."
make -j$(nproc)

echo "Build completed successfully!"
echo "Plugin built at: $(pwd)/Release/libMambaClapPlugin.clap"

# Optional: Run a simple validation
if [ -f "Release/libMambaClapPlugin.clap" ]; then
    echo "Plugin file created successfully."
else
    echo "ERROR: Plugin file not found!"
    exit 1
fi