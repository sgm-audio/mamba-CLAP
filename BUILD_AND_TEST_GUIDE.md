# Build and Test Guide for MambaFX CLAP Plugin

This guide provides instructions for building and testing the MambaFX CLAP plugin.

## Prerequisites

1. **CLAP Library** (https://github.com/free-audio/clap)
   - Build from source or install via package manager
   - Must provide `clap.h` and `libclap.so` (or `.a` for static)

2. **LibTorch** (PyTorch C++ frontend)
   - Download from https://pytorch.org/get-started/locally/
   - Select your platform and the "C++" option
   - Must provide `torch/torch.h` and `libtorch.so` (or `.a`)

3. **Build Tools**
   - CMake (version 3.14 or higher)
   - Make or Ninja
   - C++17 compatible compiler (GCC, Clang, or MSVC)

4. **Python** (for test model generation)
   - Python 3.6 or higher
   - `torch` Python package (pip install torch)

## Environment Variables

Set these environment variables before building:

- `CLAP_PATH`: Path to CLAP installation directory (must contain `include/` and `lib/` subdirectories)
  Example: `/usr/local` or `$HOME/clap_install`

- `TORCH_INSTALL_PATH`: Path to LibTorch installation directory (must contain `include/` and `lib/` subdirectories)
  Example: `$HOME/libtorch` or `/opt/libtorch`

- `MAMBA_MODEL_PATH`: (Optional) Path to TorchScript Mamba-3 model file
  Default: `./models/mamba3_audio_effects.pt` relative to plugin directory
  Example: `$HOME/models/mamba3_audio_effects.pt`

## Building the Plugin

### From the Project Root

```bash
# Navigate to the clap-plugin directory
cd clap-plugin

# Create and enter build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build the plugin
make -j$(nproc)

# The plugin will be generated at:
#   ./Release/libMambaClapPlugin.clap
```

### Alternative: Specify Paths Directly

```bash
cd clap-plugin/build
cmake .. -DCLAP_PATH="$CLAP_PATH" -DTORCH_INSTALL_PATH="$TORCH_INSTALL_PATH"
make -j$(nproc)
```

## Running Tests

### STFT Processor Unit Test

```bash
# From the project root
g++ -std=c++17 test_stft.cpp -Iclap-plugin/include -I"$TORCH_INSTALL_PATH/include" -I"$TORCH_INSTALL_PATH/include/torch/csrc/api/include" -L"$TORCH_INSTALL_PATH/lib" -ltorch -lc10 -o test_stft
./test_stft
```

### Plugin Integration Test

```bash
# From the project root
g++ -std=c++17 test_plugin_integration.cpp -Iclap-plugin/include -I"$TORCH_INSTALL_PATH/include" -I"$TORCH_INSTALL_PATH/include/torch/csrc/api/include" -L"$TORCH_INSTALL_PATH/lib" -ltorch -lc10 -o test_plugin_integration
./test_plugin_integration
```

Note: The integration test will generate a temporary TorchScript model at `/tmp/test_model.pt` for testing.

## Deploying the Plugin

### For Reaper DAW (Recommended for Testing)

1. Copy the built plugin to your CLAP plugin directory:
   ```bash
   cp Release/libMambaClapPlugin.clap ~/.clap/
   ```

   On Windows, the CLAP directory is typically:
   ```
   %USERPROFILE%\Documents\VST3Plugins\CLAP\
   ```
   or
   ```
   C:\Program Files\Common Files\CLAP\
   ```

2. In Reaper:
   - Go to Options → Preferences → Plug-ins → VST/AU
   - Click "Re-scan" or "Scan for new VST/AU plug-ins"
   - Look for "MambaFX" in the plugin list
   - Insert it on an audio track to test

### For Other DAWs

Consult your DAW's documentation for installing CLAP plugins. Most modern DAWs (Bitwig Studio, Tracktion Waveform, etc.) support CLAP 1.0.

## Troubleshooting

### Common Build Issues

1. **Missing CLAP headers/libraries**
   - Verify `CLAP_PATH` points to a directory containing `include/clap/clap.h` and `lib/libclap.so`
   - Run: `ls $CLAP_PATH/include/clap/clap.h` and `ls $CLAP_PATH/lib/libclap.so`

2. **Missing LibTorch headers/libraries**
   - Verify `TORCH_INSTALL_PATH` points to a directory containing `include/torch/csrc/api/include` and `lib/libtorch.so`
   - Run: `ls $TORCH_INSTALL_PATH/include/torch/csrc/api/include/ATen/ATen.h` and `ls $TORCH_INSTALL_PATH/lib/libtorch.so`

3. **Python.h not found (when building LibTorch from source)**
   - Install Python development headers: `sudo apt-get install python3-dev` (Ubuntu/Debian) or `sudo yum install python3-devel` (RHEL/CentOS)
   - Or use the pre-built LibTorch from pytorch.org which doesn't require Python.h for C++ usage

4. **Version mismatches**
   - Ensure compatibility between your compiler, CUDA (if applicable), and LibTorch version
   - Check the LibTorch download page for compatibility information

### Testing Issues

1. **Model loading fails**
   - Verify `MAMBA_MODEL_PATH` points to a valid TorchScript model file
   - The integration test generates its own model, so it should work without setting this variable
   - For custom models, use the `convert_model.py` script as a starting point

2. **Audio processing errors**
   - Check stderr output for `[ERROR]` messages
   - The plugin will fall back to dry signal on processing errors
   - Verify sample rate is one of: 22050, 44100, 48000, 96000 Hz

3. **No audible effect**
   - Check that the mix parameter is not set to 0.0 (fully dry)
   - Verify that the effect type is set correctly (0=reverb, 1=compression, 2=saturation, 3=EQ)
   - Try with a louder input signal
   - Remember that with the default identity model, the effect will be subtle

## Performance Expectations

- **Latency**: Approximately one block size (2048 samples) + processing time
  - At 44.1kHz: ~46ms block duration + ~10-30ms processing = ~56-76ms total
  - This is acceptable for most non-realtime-critical applications in a DAW
- **CPU Usage**: Moderate - primarily depends on the Mamba-3 model size
- **Memory Usage**: ~200-500MB (model weights + activations + audio buffers)

## Next Steps for Production Use

1. **Train or obtain a proper TorchScript Mamba-3 model** for audio effects
   - The current implementation uses simple linear layers as effect decoders
   - For best results, train a model end-to-end for your specific effect type
   - Or replace the effect decoders with more sophisticated networks

2. **Consider GPU acceleration** for reduced latency on compatible hardware
   - The implementation is device-agnostic and will use CUDA if a CUDA-enabled model is provided and `device_` is set to CUDA

3. **Enhance the user interface** with visual feedback or more precise parameter mapping
   - The current implementation uses simple linear mappings from Mamba output to effect parameters
   - For production, consider more sophisticated mapping functions or learned parameter prediction

4. **Add preset management** for quick switching between effect configurations
   - Could be implemented using the plugin's state mechanism or host-preset system

## Contact and Support

For questions or issues, please refer to the repository documentation or open an issue in the project's issue tracker.

Happy music making! 🎵