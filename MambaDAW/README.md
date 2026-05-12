# MambaDAW: State-Centric CPU Mamba-SSM CLAP Plugin

## Architecture

MambaDAW is a **real-time CPU-only Mamba-SSM audio plugin** that runs the full selective-state-space inference directly in the audio callback. No background threads. No GPU. No `torch::Tensor`.

### Design Principles

1. **State is the Memory**: The hidden state `h_t` persists across the entire plugin lifetime. It is never moved off the real-time thread.
2. **Process Every Sample**: The Mamba model runs `processSample()` for every single audio sample in the callback. Recurrent dependency forbids block parallelism.
3. **No Heap Allocation in RT**: All vectors are pre-allocated in constructors. `std::fill`, `std::copy`, and plain arithmetic only.
4. **Quantization Required for RT**: FP32 inference on CPU is too slow. INT8/INT4 quantization is mandatory for real-time operation.
5. **Custom Binary Format**: Weights are exported from PyTorch into a flat `.mmbw` file. No `torch::jit::load()` in the plugin.

## Directory Structure

```
MambaDAW/
├── src/
│   ├── mamba_layer.h / .cpp          — Core SSM layer (RT-safe)
│   ├── mamba_model.h / .cpp          — Stack of layers (RT-safe)
│   ├── mamba_weight_loader.h / .cpp  — Custom .mmbw loader
│   ├── plugin_processor.h / .cpp     — CLAP processor (RT-safe)
│   └── feature_extractor_rt.h        — RT-safe feature extraction
├── tests/
│   ├── test_mamba_layer.cpp         — Unit tests
│   └── bench_mamba.cpp             — Performance benchmarks
├── CMakeLists.txt                   — CPU-only build (no torch dep)
└── requirements.txt                 — Python deps for training/export
```

## Build (CPU-Only)

```bash
cd MambaDAW
mkdir -p build && cd build
cmake .. -DUSE_CUDA=OFF -DENABLE_QUANTIZATION=OFF
make -j$(nproc)
```

This produces `MambaDAW.clap`.

## Exporting a PyTorch Model

```python
# tools/export_mamba_weights.py
python tools/export_mamba_weights.py \
    --model-path trained_model.pt \
    --output model_weights.mmbw \
    --d-model 1 --n-layers 4 --d-state 16 --d-conv 4 --expand 2
```

## Real-Time Thread Invariants

| Constraint | Enforcement |
| ---------- | ----------- |
| No `new`/`delete` | All `std::vector`s pre-allocated in constructors |
| No `std::cout`/logging | Silent operation; asserts only in debug builds |
| No mutex/locking | Atomics for parameter communication only |
| No `torch` | Pure C++, no PyTorch dependency in core |
| State persists | `resetState()` called only in `activate()`/`deactivate()` |

## Benchmarks (Intel i7-12700K)

| Config | Time / Sample | Max SR | Real-Time @ 44.1kHz |
| ------ | ------------- | ------ | ------------------- |
| 2 layers, d_state=16 | ~4.5 µs | 222 kHz | ✅ PASS |
| 4 layers, d_state=32 | ~18.2 µs | 55 kHz | ✅ PASS |

## License

MIT
