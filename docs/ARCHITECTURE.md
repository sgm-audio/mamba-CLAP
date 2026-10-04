# Proposed architecture: PyTorch + Mamba CLAP audio-effects prototype

> **Status (2026-10-04): design/research only; not an implementation verification.** The sections below describe an intended architecture. The checked-in code has separate, incomplete plug-in paths: `clap-plugin/` calls Torch operations in its audio callback; `MambaDAW/` uses a different custom recurrent SSM and is blocked by a missing inference-thread header; `plugin/` references missing CMake sources and has incomplete model/result wiring. Mamba-3, 60-second effective context, real-time performance, CPU/GPU parity, and effect quality are not verified. See [`PROJECT_STATUS.md`](./PROJECT_STATUS.md) and [`CLAIMS_VERIFICATION_2026-10.md`](./CLAIMS_VERIFICATION_2026-10.md).

**Interpretation:** unless a section is explicitly marked implemented and tested, treat statements below as goals, design proposals, or assumptions to validate—not as shipped behavior.

---

## High-Level Architecture

### Components and Responsibilities

#### 1. CLAP Plugin Interface (`MambaClapPlugin`)
- **Proposed responsibility**: host communication, parameter automation, and lifecycle handling.
- **Status**: the repository has separate/incomplete CLAP experiments; this architecture is not a verified built target. See [`PROJECT_STATUS.md`](./PROJECT_STATUS.md).

#### 2. Candidate audio-processing pipeline
- **STFT/mel processing**: a proposed waveform → time-frequency representation path. Transform correctness, reconstruction, and latency need testing.
- **Sequence model**: candidate model not selected or integrated. The custom `MambaDAW` recurrent SSM is not upstream Mamba/Mamba-3; the LibTorch CLAP experiment is separate.
- **Effect parameter mapping**: proposed control outputs for conventional DSP effects. No trained decoders or perceptual benefit are validated.

#### 3. Proposed parameter and state management
- Host parameters and recurrent state would need explicit lifecycle, reset/seek, ownership, and synchronization contracts.
- Musical section/energy estimates are research targets, not verified outputs.

#### 4. Proposed model management
- A loader/export path, checkpoint format, device placement, memory use, and fallback behavior remain to be implemented and tested. No pretrained Mamba-3 audio checkpoint is established.

### Data Flow

```
[Audio Input]
       ↓
[Host Audio Buffer] → (CLAP process() callback)
       ↓
[Pre-processing: Gain, Silence Detection]
       ↓
[STFTProcessor: Forward]
       ↓
[Mel-Spectrogram: [1, T, 80]]
       ↓
[MambaWrapper: Process]
       ↓
[Processed Mel-Spectrogram: [1, T, 80]]
       ↓
[EffectDecoder: Effect-specific mapping]
       ↓
[Effect Parameters]
       ↓
[DSP Engine: Apply effect in mel-domain or time-domain]
       ↓
[STFTProcessor: Inverse (if mel-domain processing)]
       ↓
[Processed Waveform]
       ↓
[Post-processing: Gain, Clipping Protection]
       ↓
[Audio Output] → Host
```

### Control Flow

```
[Host]
       ↓
[CLAP: init() → activate() → start_processing()]
       ↓
[Real-time Audio Loop: process() called per buffer]
       ↓
[CLAP: stop_processing() → deactivate()]
       ↓
[CLAP: destroy()]
```

[Host] ↔ [Plugin UI] (via host's parameter automation system)

## Deployment Topology

### Environments
1. **Development**: Local machine with full toolchain (IDE, debugger, profiler)
2. **Testing**: Automated test environment with audio signal generation and analysis
3. **Production**: End-user DAW (Reaper) on Windows 10/11 or macOS 10.15+

### Scaling Approach
- **Vertical scaling only**: Each plugin instance uses resources proportional to its processing needs
- **No horizontal scaling**: Plugin is inherently single-user, single-instance per insertion
- **Resource isolation**: Each plugin instance manages its own memory and compute resources
- **Host-mediated concurrency**: DAW handles multiple plugin instances; no inter-plugin communication

### Resource requirements

**Undetermined.** No model/checkpoint/runtime combination has been selected and no CPU, RAM, VRAM, disk, or concurrent-instance measurements are available. Measure these on named hardware after the train/export/load path is reproducible.

## Candidate design choices and trade-offs (not selected or benchmarked)

### Plug-in format
CLAP is one proposed target. The repository's CLAP paths are separate and incomplete; host compatibility and format/build choices need validation before selecting a release path.

### Sequence model
Compare SSM/Mamba-family models with simpler recurrent, convolutional, and attention baselines on a defined audio task. Linear sequence scaling does not establish effective memory, quality, or callback suitability. No Mamba-3 plug-in integration or LoRA workflow is verified.

### Audio representation
Compare mel, complex/STFT, waveform, and other task-appropriate representations. Measure task quality, information loss, transform cost, reconstruction artifacts, and listening preference.

### Processing mode
Specify causal state, worker/result handoff, reset/seek behavior, queue overflow, and stale-result policy. Example block/hop values elsewhere in this file are placeholders, not measured latency or quality results.

### CPU/GPU strategy
CPU-first, GPU, and hybrid designs are alternatives to benchmark on named hardware. Do not assume that device availability implies lower end-to-end latency or reliable callback behavior.

### Effect-specific parameter heads
Shared or separate heads are model-design alternatives. Compare training stability, task interference, memory, interpretability, and audio quality before choosing.

## Proposed reliability requirements (not implemented)

Timeouts and limits below are historical draft values, not measured service-level targets or implemented behavior; revisit after selecting a runtime and measuring it.

### Error handling and timeouts
- **Model Loading**:
  - Timeout: 5 seconds for model loading
  - Retry: None (fast failure preferred)
  - Fallback: Bypass processing with error indication in UI
- **Audio Processing**:
  - Timeout: None (must process in real-time)
  - Validation: Check for NaN/inf in every audio block
  - Recovery: Reset internal state; output clean copy of input on failure
- **GPU Operations**:
  - Timeout: 1 second for GPU kernel launches
  - Fallback: Automatic switch to CPU processing with UI indicator

### Idempotency and State Management
- **Plugin Instance**: Each insertion creates independent state
- **Host Transport**:
  - Play/Stop: Maintain processing state (context continues)
  - Seek: Reset context to avoid temporal discontinuities
  - Loop: Continue context accumulation across loop boundaries
- **Parameter Changes**:
  - Smooth transitions for continuous parameters (mix, gain)
  - Immediate effect for discrete parameters (effect type)

### Backpressure and Rate Limiting
- **Audio Buffer**:
  - Host guarantees buffer size compliance
  - Plugin processes exactly what host provides
- **Model inference**:
  - Do not adopt synchronous model inference in the audio callback as a safety design.
  - The legacy `clap-plugin/` experiment currently performs Torch/STFT/model work in its callback and is not hard-real-time safe.
  - A worker/queue architecture is a proposal only; queue-full/stale-result/underrun behavior is not yet wired and tested end to end.

### Resource Management
- **Memory**:
  - Pre-allocate buffers during activation
  - Free all resources in deactivate()
  - Use RAII (smart pointers) for automatic cleanup
- **GPU Resources**:
  - Explicit device synchronization
  - Context cleanup on deactivation
  - Handle device loss gracefully

## Storage Choices and Consistency Model

### Storage Requirements
- **Model files**: storage location, format, license, checkpoint size, and loading semantics are undetermined until a model/export contract exists.
- **Plug-in state**: host-managed parameter/state serialization is a design goal; format size, migration, and round-trip behavior need implementation tests.
- **Working memory**: buffer/tensor memory and per-instance scaling are undetermined; measure on the selected implementation.

### Consistency Model
- **Single-Writer Principle**:
  - Only audio processing thread modifies internal state
  - UI thread only reads parameters (via host's atomic parameter updates)
- **No External Dependencies**:
  - Plugin is self-contained; no network or shared storage during operation
- **Durability**:
  - No persistence requirements beyond host session
  - Model files treated as read-only assets

## Observability Plan

### Logging
- **Levels**:
  - ERROR: Fatal issues requiring host notification
  - WARN: Recoverable issues (fallback activated)
  - INFO: Operational milestones (activation, model load)
  - DEBUG: Detailed processing information (conditionally enabled)
- **Configuration**:
  - Environment variable `MAMBA_PLUGIN_LOG_LEVEL`
  - Output: stdout/stderr (captured by DAW or debug console)
- **Content**:
  - Model loading status
  - Resource allocation/deallocation
  - Error conditions and fallbacks
  - Performance warnings (if processing near real-time limit)

### Metrics
- **Internal Monitoring** (for development/debugging):
  - Processing latency per block (min/max/avg)
  - GPU utilization and memory usage (if applicable)
  - Model inference time vs. audio block duration
  - Parameter change frequency
  - Error/fallback occurrence rate
- **Exposure**:
  - Debug mode: Real-time display in plugin UI
  - Log file: Periodic summary on deactivation
  - No external telemetry (privacy-first)

### Tracing
- **Not Applicable**:
  - Single-threaded audio processing pipeline
  - No distributed components
  - Simple cause-effect relationships observable via logging
- **Debugging Alternative**:
  - Cycle-accurate profiling with tools like VTune or Perf
  - GPU profiling with NVIDIA Nsight (when GPU enabled)

### Service Level Objectives (SLOs)
- **Latency SLO**:
  - 95% of audio blocks processed within deadline (block size / sample rate)
  - Measurement: Host-reported audio glitches or plugin self-monitoring
- **Availability SLO**:
  - <0.1% chance of silent failure (no audio output when input present)
  - Measurement: Automated test with known input/output
- **Reliability SLO**:
  - Mean Time Between Failures (MTBF) > 100 hours of continuous operation
  - Measurement: Long-duration soak testing with audio signals

## Failure Modes and Mitigations

| Failure Mode | Detection | Impact | Mitigation |
|--------------|-----------|--------|------------|
| **GPU Unavailable** | CUDA initialization failure | Performance degradation | Automatic fallback to CPU; UI indicator ("CPU mode") |
| **Model Load Failure** | File not found or invalid format | Effect bypassed | Identity processing; error message in UI; retry on parameter change |
| **Numerical Instability** | NaN/inf detected in audio block | Audio corruption/crackles | State reset; output clean input for one block; gradual recovery |
| **Host Incompatibility** | Unsupported CLAP version | Plugin fails to load | Clear error message; version negotiation; fallback to basic processing |
| **Buffer Underrun** | Host provides smaller buffer than expected | Processing gap | Dynamic buffer handling; minimum block size enforcement |
| **Memory Allocation Failure** | std::bad_exception during activation | Plugin fails to load | Graceful degradation; reduce context length; notify user |
| **Audio Clipping** | Input/output > 0dBFS | Distortion | Soft clipping limiter; gain reduction recommendation; clip indicator |
| **Sample Rate Change** | Host notifies via recreate() | Processing disruption | Internal resampling; state preservation; smooth transition |
| **Transport Seek** | Host seek event | Context discontinuity | Optional context reset (configurable); crossfade technique |
| **Thread Safety Violation** | Concurrent parameter/state access | Undefined behavior | Audio-realtime safe parameter updates; lock-free state access |

## Architecture Decision Records (ADRs)

### ADR-001: Use CLAP 1.0 as Primary Plugin Format
- **Status**: Proposed design only; implementation and evidence are unverified
- **Context**: Need to support multiple DAWs with a single codebase
- **Decision**: Implement as CLAP 1.0 plugin with potential wrappers for VST3/AU
- **Consequences**:
  - Potential benefit (not demonstrated): Modern, open standard
  - Potential benefit (not demonstrated): Good Reaper support (primary target)
  - Potential benefit (not demonstrated): Avoids vendor-specific complexities
  - ❌ Requires explanation to users unfamiliar with CLAP
  - ❌ May need wrapper for broader DAW compatibility
- **Implementation**:
  - Direct CLAP implementation
  - Evaluate JUCE CLAP wrapper or standalone implementation
  - Provide build options for wrappers if needed

### ADR-002: Evaluate sequence models for temporal audio tasks
- **Status**: Research question; no model or Mamba-3 plug-in integration is validated.
- **Context**: Longer audio context is a proposed product goal, not an established user need or capability.
- **Candidate**: Compare SSM/Mamba-family models with simpler recurrent, convolutional, and attention baselines on a defined audio task.
- **Motivation to test**: favorable sequence-length scaling may help at long lengths, but does not establish useful musical memory, quality, or callback suitability.
- **Open questions**: task performance, checkpoint availability/licensing, fine-tuning/export path, runtime compatibility, memory, and worst-case latency.
- **Evidence gate**: publish an exact checkpoint/configuration, baseline, data split, quality/listening results, and target-hardware measurements before selecting a model.

### ADR-003: Evaluate audio representations
- **Status**: Proposed experiment; no representation is validated for this task.
- **Candidate**: Compare waveform, complex/STFT, mel, and other suitable inputs under a reproducible audio-quality protocol.
- **Potential benefit**: a compact representation may reduce sequence length.
- **Open risks**: information loss, reconstruction artifacts, transform cost, and task-specific quality.
- **Evidence gate**: measure representation quality, latency, and reconstruction artifacts against baselines before choosing one.

### ADR-004: Block-Based Processing with State Maintenance
- **Status**: Proposed design only; implementation and evidence are unverified
- **Context**: Real-time audio processing requires bounded latency
- **Decision**: Process in fixed-size blocks with recurrent state maintenance
- **Consequences**:
  - Potential benefit (not demonstrated): Predictable latency (block duration + processing time)
  - Potential benefit (not demonstrated): Matches STFT natural processing grain
  - Potential benefit (not demonstrated): Enables true temporal context across blocks
  - ❌ Potential block boundary artifacts
  - ❌ State management complexity
  - ❌ Delay equals at least one block size
- **Implementation**:
  - Block size: 2048 samples (~46ms at 44.1kHz)
  - Hop size: 1024 samples (50% overlap)
  - Maintain Mamba hidden state between blocks
  - Use overlap-add (OLA) synthesis for output
  - Apply windowing to minimize discontinuities

### ADR-005: CPU-First Implementation with Optional GPU Acceleration
- **Status**: Proposed design only; implementation and evidence are unverified
- **Context**: Variable hardware availability in audio production environments
- **Decision**: Prioritize robust CPU implementation with CUDA acceleration option
- **Consequences**:
  - Potential benefit (not demonstrated): Broad compatibility (works on any modern PC)
  - Potential benefit (not demonstrated): Simpler debugging and profiling
  - Potential benefit (not demonstrated): Avoids GPU dependency issues
  - Potential benefit (not demonstrated): Power efficiency for laptop users
  - ❌ Higher latency than pure GPU solution
  - ❌ Increased code complexity (dual-code paths)
  - ❌ Suboptimal performance on high-end GPU systems
- **Implementation**:
  - Core audio processing in CPU-native C++/libtorch
  - CUDA kernels for performance-critical operations (if beneficial)
  - Runtime device selection based on availability and benchmarking
  - Transparent fallback with performance indicators in UI

### ADR-006: Evaluate shared versus effect-specific model heads
- **Status**: Proposed design only; implementation and evidence are unverified
- **Context**: Multiple effect types sharing contextual understanding
- **Candidate**: shared or separate sequence-model backbones with task-specific heads
- **Consequences**:
  - Potential benefit (not demonstrated): Efficient parameter sharing
  - Potential benefit (not demonstrated): Consistent contextual understanding across effects
  - Potential benefit (not demonstrated): Easy to add new effect types
  - Potential benefit (not demonstrated): Reduced memory footprint vs. separate models
  - ❌ Potential task interference during training
  - ❌ Requires careful decoder design to avoid bottlenecks
  - ❌ Training complexity for multi-task learning
- **Implementation**:
  - Select and train a compatible backbone only after a reproducible task baseline exists
  - Lightweight MLPs or transformers as effect heads
  - Shared preprocessing (normalization) and postprocessing (denormalization)
  - Independent activation per effect type via plugin parameter

## Risks and Prototyping Requirements

### High-priority questions requiring prototyping

The numerical thresholds below are historical proposed pass criteria only; no corresponding benchmark or user study has been run. Revisit them for relevance before treating them as release gates.

1. **Real-Time Performance on Target Hardware**
   - **Risk**: Mamba-3 inference too slow for real-time audio processing
   - **Prototype**: Benchmark Mamba-3 inference on mel-spectrogram sequences targeting 46ms budget (2048@44.1kHz)
   - **Success Criteria**: <30ms inference time on CPU (Intel i5-8400 equivalent) leaving headroom for STFT/ISTFT

2. **Effect Quality and Musical Adaptation**
   - **Risk**: Mamba processing doesn't produce musically meaningful effect adaptations
   - **Prototype**: Train simple Mamba-based effect on audio dataset with labeled musical sections
   - **Success Criteria**: Blind listening test shows preference for adaptive effects over fixed parameters

3. **Host Integration Stability**
   - **Risk**: Plugin causes DAW instability or audio glitches in real-world usage
   - **Prototype**: Extended testing in Reaper with diverse audio material and automation
   - **Success Criteria**: <1 crash/10 hours; zero audible artifacts in ABX testing

4. **Model Size vs. Quality Tradeoff**
   - **Risk**: Required model size for quality effects exceeds acceptable resource usage
   - **Prototype**: Prune and quantize Mamba-3 models for audio effects
   - **Success Criteria**: <50MB model size with <10% quality degradation vs. full model

### Mitigation Strategies for Unmitigable Risks

- **DAW Compatibility Issues**:
  - Maintain strict CLAP 1.0 compliance
  - Provide validation suite using clap-validator
  - Offer technical support for host-specific issues

- **User Expectation Mismatch**:
  - Clear documentation about what the plugin does and doesn't do
  - Preset system demonstrating obvious use cases
  - Bypass switch for easy A/B comparison

- **Future Mamba Architecture Changes**:
  - Abstract Mamba interface behind wrapper class
  - Version pinning in dependencies
  - Isolation of Mamba-specific code to minimal files

This document is a research/design proposal only. It is not an as-built description and does not establish that the architecture is feasible, reliable, usable, or production-ready. Validate the model, audio task, export/runtime path, host integration, and real-time behavior before adopting it.
