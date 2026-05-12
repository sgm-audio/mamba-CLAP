# ADR-004: Memory Architecture and Model Configuration Contract

## Status
Accepted

## Context
Neural inference runs in `process()`, so all runtime memory and model dimensions must be prepared before RT entry. Model swaps from 1M to 50M parameters must not require code rewrites.

## Decision
### 1. RT Working Memory
- All inference working buffers are pre-allocated in `activate()`.
- Maximum host block size is supplied to allocation planner during `activate()`.
- No heap allocation is allowed in `process()`.

### 2. Weight Storage
- Model weights are loaded from memory-mapped read-only files.
- Mapped regions are cache-aligned and immutable at runtime.
- Weight file path is owned by main thread and published via prepared runtime context swap.

### 3. Compile-Time Model Dimensions
- A generated `config.h` from ML export tool defines fixed maxima and active dimensions.
- Build must fail if config constants violate compile-time guards.
- Runtime asserts verify weight metadata matches `config.h`.

### 4. Dependency Law for RT Path
- No BLAS/CUDA/heavy external compute runtimes may execute on the audio thread.
- RT kernel uses project-local C++ implementation and pre-allocated buffers only.

## Consequences
### Positive
- Deterministic RT memory behavior.
- Model capacity scaling handled through generated config and weights.
- Predictable binary footprint and CPU characteristics.

### Negative
- Config regeneration and rebuild required for dimension changes.
- Export tooling must stay synchronized with runtime parser.

## Verification Requirements
- Allocation audit (instrumented new/delete) reports zero allocations in `process()` over sustained run.
- Weight metadata validation test fails on mismatched dimensions.
- Cache alignment checks run in debug startup validation.

## Acceptance Criteria
- Swapping 1M to 50M model is achieved by changing generated `config.h` and weight file only.
- No inference-path memory allocation occurs after `activate()` completes.
