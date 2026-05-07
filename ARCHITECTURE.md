# System Architecture: PyTorch + Mamba CLAP Audio Effects Plugin

## High-Level Architecture

### Components and Responsibilities

#### 1. CLAP Plugin Interface (`MambaClapPlugin`)
- **Responsibility**: Implements the CLAP 1.0 plugin interface for host communication
- **Handles**: Host initialization, audio processing callbacks, parameter automation, lifecycle management
- **Interfaces**: CLAP host, audio processing threads, UI thread (via host)

#### 2. Audio Processing Pipeline
- **STFTProcessor**: Converts time-domain audio to/from mel-spectrogram domain
  - Forward: Waveform → STFT → Magnitude → Mel-filterbank → Log-Mel spectrogram
  - Inverse: Mel-spectrogram → Inverse Mel-filterbank → Magnitude → ISTFT → Waveform
- **MambaWrapper**: Encapsulates Mamba-3 model inference
  - Loads Mamba-3 checkpoint from mamba_ssm library
  - Processes mel-spectrogram sequences with state maintenance
  - Handles device placement (CPU/GPU)
- **EffectDecoders**: Converts Mamba output to effect-specific parameters
  - ReverbDecoder: Maps to reverb parameters (decay, pre-diffusion, etc.)
  - CompressionDecoder: Maps to compression parameters (ratio, attack, release)
  - SaturationDecoder: Maps to distortion/harmonics parameters
  - EQDecoder: Maps to frequency band gains

#### 3. Parameter and State Management
- **PluginParameters**: Manages host-exposed parameters (effect type, mix, context length, etc.)
- **ProcessingState**: Maintains Mamba hidden state across audio blocks for temporal context
- **EffectContext**: Tracks musical context (estimated section type, energy, etc.) for adaptation

#### 4. Model Management
- **ModelLoader**: Handles loading and initialization of Mamba-3 weights
  - Supports TorchScript and native PyTorch formats
  - Manages device transfer and memory allocation
  - Provides fallback to identity processing on failure

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

### Resource Requirements per Instance
- **CPU**: 1-2 cores (depending on block size and model complexity)
- **Memory**: 200-500MB (model weights + activations + audio buffers)
- **GPU**: Optional; 100-200MB VRAM if CUDA enabled
- **Disk**: 50-200MB for model files (depending on Mamba-3 variant)

## Key Design Choices and Tradeoffs

### 1. CLAP Format Selection
- **Choice**: Use CLAP 1.0 as primary plugin format
- **Why**: Modern, open standard; good DAW support (Reaper, Bitwig, Tracktion); avoids vendor lock-in
- **Tradeoff**: Less mature ecosystem than VST3/AU; requires CLAP wrapper for some hosts
- **Mitigation**: Provide CLAP wrapper for VST3/AU if needed in future

### 2. Mamba-3 Selection
- **Choice**: Use Mamba-3 SSM architecture for temporal modeling
- **Why**: O(N) complexity for long sequences; proven audio modeling capability; efficient inference
- **Tradeoff**: Newer architecture with fewer audio-specific pretrained models; requires adaptation
- **Mitigation**: Start with audio-effect-specific fine-tuning of base Mamba-3 models

### 3. Mel-Spectrogram Intermediate Representation
- **Choice**: Convert audio to mel-spectrogram for Mamba processing
- **Why**: Compact representation; aligns with human auditory perception; reduces sequence length
- **Tradeoff**: Phase information loss; STFT/ISTFT overhead
- **Mitigation**: Use sufficient overlap (50%) and window size to minimize artifacts; consider complex spectrograms if needed

### 4. Block-Based Processing Approach
- **Choice**: Process audio in overlapping blocks (2048 samples, 50% hop)
- **Why**: Enables real-time operation with bounded latency; matches STFT natural block size
- **Tradeoff**: Block boundary artifacts; state continuity challenges
- **Mitigation**: Overlap-add synthesis; maintain Mamba state across blocks; use appropriate windowing

### 5. CPU-First with GPU Acceleration
- **Choice**: Prioritize CPU implementation with optional GPU acceleration
- **Why**: Broader compatibility; simpler debugging; GPU not always available in audio PCs
- **Tradeoff**: Higher latency on CPU; more complex dual-path implementation
- **Mitigation**: Transparent fallback; asynchronous model loading to avoid audio glitches

### 6. Effect-Specific Decoding Heads
- **Choice**: Separate decoders for each effect type sharing Mamba backbone
- **Why**: Efficient multi-task learning; shared contextual understanding; easy to add new effects
- **Tradeoff**: Increased model size; potential interference between tasks
- **Mitigation**: Task-specific adapters; regularization during training; modular decoder design

## Reliability Plan

### Error Handling and Timeouts
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
- **Model Inference**:
  - Synchronous processing within audio callback
  - No queuing; if inference takes too long, audio glitches occur
  - Mitigation: Profiling to ensure real-time capability; reduce model complexity if needed

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
- **Model Files**: 
  - Location: User-configurable path (environment variable or plugin parameter)
  - Format: TorchScript (.pt) or native PyTorch checkpoint
  - Size: 100MB-1GB depending on Mamba-3 variant
- **Plugin State**:
  - Location: Host-managed via CLAP state mechanism
  - Content: Last-used parameters (effect type, mix values, etc.)
  - Size: <1KB
- **Cache/Temporary**: 
  - Location: In-memory only
  - Content: Audio processing buffers, intermediate tensors
  - Size: <50MB per instance

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
- **Status**: Accepted
- **Context**: Need to support multiple DAWs with a single codebase
- **Decision**: Implement as CLAP 1.0 plugin with potential wrappers for VST3/AU
- **Consequences**: 
  - ✅ Modern, open standard
  - ✅ Good Reaper support (primary target)
  - ✅ Avoids vendor-specific complexities
  - ❌ Requires explanation to users unfamiliar with CLAP
  - ❌ May need wrapper for broader DAW compatibility
- **Implementation**: 
  - Direct CLAP implementation
  - Evaluate JUCE CLAP wrapper or standalone implementation
  - Provide build options for wrappers if needed

### ADR-002: Adopt Mamba-3 for Temporal Modeling
- **Status**: Accepted
- **Context**: Requirement for long-term context (60+ seconds) in audio effects
- **Decision**: Use Mamba-3 SSM architecture from mamba_ssm library
- **Consequences**: 
  - ✅ O(N) complexity enables long context
  - ✅ State-of-the-art sequence modeling
  - ✅ Efficient inference suitable for real-time
  - ✅ Access to pretrained models via Hugging Face
  - ❌ Newer architecture with limited audio-specific pretraining
  - ❌ Requires adaptation from language modeling to audio domain
- **Implementation**: 
  - Integrate mamba_ssm as submodule or dependency
  - Load Mamba-3 checkpoints via torch::jit::load or native loading
  - Create audio-specific processing wrapper around Mamba-3 block

### ADR-003: Mel-Spectrogram as Intermediate Representation
- **Status**: Accepted
- **Context**: Need to represent audio for sequential processing by Mamba
- **Decision**: Convert audio to log-mel spectrogram via STFT
- **Consequences**: 
  - �| Compact representation reduces sequence length
  - ✓ Perceptually relevant frequency scale
  - ✓ Enables use of established audio ML techniques
  - ✗ Phase information discarded
  - ✗ STFT/ISTFT computational overhead
  - ✗ Windowing artifacts possible
- **Implementation**: 
  - STFT with Hann window, 50% overlap
  - 80 mel bins covering 20Hz-20kHz range
  - Log compression with epsilon for numerical stability
  - Optional: Experiment with complex spectrograms or alternative representations

### ADR-004: Block-Based Processing with State Maintenance
- **Status**: Accepted
- **Context**: Real-time audio processing requires bounded latency
- **Decision**: Process in fixed-size blocks with recurrent state maintenance
- **Consequences**: 
  - ✅ Predictable latency (block duration + processing time)
  - ✅ Matches STFT natural processing grain
  - ✅ Enables true temporal context across blocks
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
- **Status**: Accepted
- **Context**: Variable hardware availability in audio production environments
- **Decision**: Prioritize robust CPU implementation with CUDA acceleration option
- **Consequences**: 
  - ✅ Broad compatibility (works on any modern PC)
  - ✅ Simpler debugging and profiling
  - ✅ Avoids GPU dependency issues
  - ✅ Power efficiency for laptop users
  - ❌ Higher latency than pure GPU solution
  - ❌ Increased code complexity (dual-code paths)
  - ❌ Suboptimal performance on high-end GPU systems
- **Implementation**: 
  - Core audio processing in CPU-native C++/libtorch
  - CUDA kernels for performance-critical operations (if beneficial)
  - Runtime device selection based on availability and benchmarking
  - Transparent fallback with performance indicators in UI

### ADR-006: Shared Backbone with Effect-Specific Decoders
- **Status**: Accepted
- **Context**: Multiple effect types sharing contextual understanding
- **Decision**: Single Mamba-3 backbone with multiple lightweight decoder heads
- **Consequences**: 
  - ✅ Efficient parameter sharing
  - ✅ Consistent contextual understanding across effects
  - ✅ Easy to add new effect types
  - ✅ Reduced memory footprint vs. separate models
  - ❌ Potential task interference during training
  - ❌ Requires careful decoder design to avoid bottlenecks
  - ❌ Training complexity for multi-task learning
- **Implementation**: 
  - Frozen or fine-tuned Mamba-3 backbone
  - Lightweight MLPs or transformers as effect heads
  - Shared preprocessing (normalization) and postprocessing (denormalization)
  - Independent activation per effect type via plugin parameter

## Risks and Prototyping Requirements

### High-Priority Risks Requiring Prototyping

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

This architecture provides a solid foundation for implementing the PyTorch + Mamba CLAP audio effects plugin while addressing the key requirements from the PRD. The design balances innovation with practicality, prioritizing reliability and usability in real-world audio production environments.