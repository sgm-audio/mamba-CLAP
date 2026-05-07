#!/bin/bash

# Build script for MambaFX CLAP plugin
# Requires environment variables:
#   CLAP_PATH - path to CLAP installation (containing include/ and lib/ directories)
#   TORCH_INSTALL_PATH - path to LibTorch installation (containing include/ and lib/ directories)

set -e  # Exit on any error

echo "Building MambaFX CLAP plugin..."

# Check for required environment variables
if [ -z "$CLAP_PATH" ]; then
    echo "Error: CLAP_PATH environment variable not set"
    echo "Please set it to the CLAP installation directory (e.g., /usr/local)"
    exit 1
fi

if [ -z "$TORCH_INSTALL_PATH" ]; then
    echo "Error: TORCH_INSTALL_PATH environment variable not set"
    echo "Please set it to the LibTorch installation directory (e.g., /path/to/libtorch)"
    exit 1
fi

echo "Using CLAP_PATH: $CLAP_PATH"
echo "Using TORCH_INSTALL_PATH: $TORCH_INSTALL_PATH"

# Change to clap-plugin directory
cd clap-plugin

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. \
    -DCLAP_PATH="$CLAP_PATH" \
    -DTORCH_INSTALL_PATH="$TORCH_INSTALL_PATH"

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