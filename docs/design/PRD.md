# Aspirational PRD: PyTorch + Mamba CLAP audio-effects research

> **Status (2026-10-04): proposed requirements, not delivered features.** The 60-second context, Mamba-3 integration, latency, host, quality, and adoption values in this PRD are goals/acceptance targets only. They have not been demonstrated by the current repository. See [`docs/PROJECT_STATUS.md`](../PROJECT_STATUS.md) and [`docs/CLAIMS_VERIFICATION_2026-10.md`](../CLAIMS_VERIFICATION_2026-10.md).

## Problem Statement
Audio engineers and music producers currently rely on traditional DSP-based effects (reverb, compression, etc.) that operate on short-term signal analysis (typically 20-100ms windows). These effects lack the ability to understand long-term musical context (60+ seconds), resulting in processing that doesn't adapt to the evolving structure, style, or emotional content of a music piece. For example, a reverb effect might apply the same room characteristics throughout a song regardless of whether it's a verse, chorus, or bridge, missing opportunities to enhance the musical narrative through context-aware processing.

## Target Users
1. **Music Producers** - Creating electronic, pop, hip-hop, or experimental music who want effects that evolve with their arrangements
2. **Audio Engineers** - Mixing engineers seeking intelligent processors that adapt to different song sections
3. **Sound Designers** - Professionals creating immersive audio experiences needing context-sensitive effects
4. **Electronic Music Artists** - Live performers using DAWs who want effects that respond to their performance dynamics

## User Journeys
1. **Music Production Workflow**:
   - User arranges a song with distinct sections (intro, verse, chorus, bridge, outro)
   - User inserts Mamba CLAP effect on vocal track
   - Effect analyzes incoming audio and builds contextual understanding over time
   - During verse: applies subtle, intimate reverb
   - During chorus: automatically increases reverb depth and width for emotional impact
   - During bridge: shifts to a different reverb character for contrast
   - User adjusts effect parameters globally, but behavior adapts locally to song sections

2. **Mixing Session**:
   - Engineer inserts Mamba CLAP compressor on drum bus
   - Effect analyzes full mix context to inform compression decisions
   - During sparse sections: applies gentle compression to preserve dynamics
   - During dense sections: increases compression to maintain consistent energy
   - Effect adapts to genre-specific expectations (e.g., more compression for EDM, less for jazz)

## Goals (proposed)
1. Test whether audio effects can benefit from longer musical context; effective duration is not selected or measured.
2. Evaluate audio quality against conventional and learned baselines using blind listening and objective measures.
3. Assess compatibility with selected DAWs and formats only after a buildable plug-in path exists.
4. Measure latency, stability, and resource use under a defined host/block/hardware protocol.

## Non-Goals (current proposal)
1. No <5ms processing target has been validated. Select a latency target only after algorithmic, host-buffer, and worker behavior are defined and measured; previous <5ms, <50ms CPU, and <20ms GPU values conflict and are withdrawn as commitments.
2. **Replacement for all traditional effects** — any learned control would complement, not replace, established DSP until evidence supports otherwise.
3. **Video/post-production applications** — the research concept is focused on music production.
4. **Multi-instrument separation** — source separation is out of scope for this proposal.
5. **Physical modeling emulation** — no hardware-emulation claim is intended.

## Proposed validation metrics (targets only; not results)
1. **User engagement**: define a study before recruiting users; the former 70% continuation figure is not validated.
2. **Effect quality**: specify sample set, baseline, and blind protocol before setting any preference threshold; the former 80% figure is not a result.
3. **Technical performance**: define a host, block size, sample rate, hardware, and latency metric, then publish worst-case and average measurements. No <5ms, <50ms CPU, or <20ms GPU result is established.
4. **Stability**: measure crashes, dropouts, and error recovery in actual hosts; the former crash-rate figure is a proposed target only.
5. **Adoption**: downloads or revenue are not validated success forecasts. Establish buyer interest and retention with observed data before setting numeric targets.

## Functional Requirements

### 1. Context-Aware Audio Processing
**FR1.1**: The plugin shall analyze incoming audio to build a contextual understanding of musical content over time windows of at least 60 seconds
**FR1.2**: The plugin shall adapt effect parameters based on detected musical characteristics (section type, energy level, spectral content)
**FR1.3**: The plugin shall maintain a processing state that evolves continuously during audio playback

### 2. Effect Types Implementation
**FR2.1**: The plugin shall implement a context-aware reverb effect that adapts decay time, pre-delay, and diffusion based on musical context
**FR2.2**: The plugin shall implement an intelligent compressor that adapts ratio, attack, and release times based on detected transients and musical density
**FR2.3**: The plugin shall implement adaptive saturation that adds harmonics complementary to the existing musical content
**FR2.4**: The plugin shall implement dynamic EQ that adjusts frequency bands based on spectral balance of the mix

### 3. CLAP Plugin Compliance
**FR3.1**: The plugin shall implement the CLAP 1.0 specification for audio effects
**FR3.2**: The plugin shall expose standard audio effect features (CLAP_PLUGIN_FEATURE_AUDIO_EFFECT)
**FR3.3**: The plugin shall support sample rates of 44.1kHz and 48kHz
**FR3.4**: The plugin shall process stereo audio signals
**FR3.5**: The plugin shall provide standard parameter automation support

### 4. Mamba-3 Integration
**FR4.1**: The plugin shall utilize the Mamba-3 State Space Model from the mamba_ssm library
**FR4.2**: The plugin shall process audio through a mel-spectrogram representation for Mamba input
**FR4.3**: The plugin shall implement effect-specific decoding from Mamba's output representation
**FR4.4**: The plugin shall support both CPU and GPU (CUDA) execution paths

### 5. User Interface and Controls
**FR5.1**: The plugin shall provide a parameter to select effect type (reverb, compression, saturation, EQ)
**FR5.2**: The plugin shall provide a context length adjustment control (10-120 seconds)
**FR5.3**: The plugin shall provide wet/dry mix control for each effect type
**FR5.4**: The plugin shall provide input/output gain controls
**FR5.5**: The plugin shall provide a bypass switch

## Non-Functional Requirements

### Performance
**NFR1**: The plugin shall process audio with less than 50ms latency on CPU (Intel i7-9700K or equivalent)
**NFR2**: The plugin shall process audio with less than 20ms latency on GPU (NVIDIA RTX 3060 or equivalent)
**NFR3**: The plugin shall use less than 30% CPU core on mid-range desktop processors during typical operation
**NFR4**: The plugin shall use less than 500MB RAM

### Reliability
**NFR5**: The plugin shall not crash or produce audio dropouts during normal operation in Reaper
**NFR6**: The plugin shall handle silent input gracefully without producing noise or artifacts
**NFR7**: The plugin shall recover gracefully from temporary GPU unavailability (fallback to CPU)

### Compatibility
**NFR8**: The plugin shall be compatible with Reaper v6.0+ on Windows 10/11 and macOS 10.15+
**NFR9**: The plugin shall follow CLAP 1.0 specification for plugin discovery and instantiation
**NFR10**: The plugin shall support standard plugin formats (VST3, AU) through CLAP wrapper if needed

### Privacy and Compliance
**NFR11**: The plugin shall not transmit audio data or user information externally
**NFR12**: The plugin shall not require internet connection for core functionality
**NFR13**: The plugin shall comply with GDPR by not storing personal data

## Edge Cases and Failure Behaviors

### Edge Cases
1. **Silent Input**: When input signal is below -60dBFS for >2 seconds, effect shall reduce processing to minimum to avoid noise amplification
2. **Clipping Detection**: When input exceeds 0dBFS, plugin shall apply soft clipping protection and indicate overload in UI
3. **Sample Rate Changes**: Plugin shall handle host sample rate changes without audio artifacts or crashes
4. **Buffer Size Variations**: Plugin shall maintain stable operation with host buffer sizes from 32 to 4096 samples
5. **Host Transport Changes**: Plugin shall maintain contextual state during host play/stop/seeks, resetting context only on explicit user action

### Failure Behaviors
1. **GPU Failure**: If GPU acceleration fails, plugin shall automatically fall back to CPU processing with visual indication in UI
2. **Model Loading Failure**: If Mamba model fails to load, plugin shall bypass processing and show error message in UI
3. **Memory Allocation Failure**: Plugin shall handle allocation failures gracefully by reducing context length or bypassing
4. **Numerical Instability**: Plugin shall detect NaN/inf values in processing and reset internal state to prevent audio corruption
5. **Host Incompatibility**: Plugin shall validate host capabilities during initialization and fail gracefully with clear error message if CLAP version unsupported

## MVP Scope + Phased Roadmap

### MVP (Minimum Viable Product)
- Single effect type: Context-Aware Reverb
- Mamba-3 integration with basic mel-spectrogram processing
- CPU-only implementation (with CUDA preparation)
- Basic UI with effect type selector, mix control, and context length
- CLAP plugin wrapper for Reaper testing
- Manual model loading via file path

### Phase 1: Core Functionality (Months 1-2)
- Implement context-aware reverb with Mamba-3
- Basic parameter controls (mix, context length, decay)
- CPU performance optimization
- Stability testing in Reaper
- User feedback collection

### Phase 2: Multi-Effect and GPU (Months 3-4)
- Add intelligent compressor effect
- Add adaptive saturation effect
- Implement GPU acceleration path
- CUDA kernel optimization for Mamba inference
- Cross-platform testing (Windows/macOS)

### Phase 3: Polish and Advanced Features (Months 5-6)
- Implement dynamic EQ effect
- Advanced UI with visual context display
- Preset management system
- A/B testing functionality
- Performance optimization for various hardware

## Proposed acceptance criteria (not passed; study protocol pending)

The thresholds below are draft research targets, not measured outcomes or release commitments. Musical-effect targets (including the former 20% difference) require a defined dataset and listening protocol. DAW/host criteria require a buildable plug-in first.

### Context-adaptive behavior (hypotheses)
**AC1.1**: Test whether a chosen model produces a reproducible, musically useful change across clearly labeled sections; define effect direction/magnitude from a baseline study rather than presuming a 20% result.
**AC1.2**: Test whether compressor parameters correlate with annotated musical density and are preferred to fixed-parameter baselines in blind evaluation.

### Technical performance (protocol before thresholds)
**AC2.1**: Record reported and loopback latency on a named CPU/host/sample-rate/block-size configuration. Set an acceptance threshold only after identifying algorithmic, host-buffer, and worker delays.
**AC2.2**: Treat GPU acceleration as a separate experiment. Do not assume it improves end-to-end latency or is suitable for an audio callback; document device, transfer, and scheduling costs.
**AC2.3**: After a reproducible build, test crashes, dropouts, and audible artifacts under sustained host load; publish duration, material, host, hardware, and observed result.

### CLAP Compliance
**AC3.1**: Given a Reaper DAW v6.0+, When the plugin is scanned and loaded, Then it shall appear in the plugin browser with correct name, vendor, and category
**AC3.2**: Given an instantiated plugin in Reaper, When audio is routed through it, Then it shall process stereo audio without channel inversion or phase issues
**AC3.3**: Given a plugin with automated parameters, When automation curves are drawn in Reaper, Then the plugin shall respond to parameter changes with no zipper noise or stepping artifacts

### User Experience
**AC4.1**: Given a first-time user, When they insert the plugin and load a preset, Then they shall be able to hear an audible effect difference between wet and dry signals within 30 seconds
**AC4.2**: After a validated model and host path exist, compare configured 20s and 60s context targets under a blind protocol; do not infer effective musical memory from the UI setting alone.
**AC4.3**: Given a user switching between effect types, When they change from reverb to compression, Then the plugin shall transition smoothly without audible clicks or processing glitches

### Edge Case Handling
**AC5.1**: Given silent input (-80dBFS) for 5 seconds, When processed through the plugin, Then the output noise floor shall not increase by more than 3dB
**AC5.2**: Given input signal peaking at +3dBFS, When processed through the plugin, Then the output shall not exhibit digital clipping distortion
**AC5.3**: Given host sample rate change from 44.1kHz to 48kHz, When the change occurs, Then the plugin shall continue processing without audible artifacts or interruption

## Out-of-Scope Items
1. Standalone application version (plugin-only for DAW integration)
2. Mobile/iOS versions (desktop focus only)
3. Physical hardware acceleration (relying on host CPU/GPU only)
4. Multi-channel surround sound processing (stereo only)
5. Machine learning model training capabilities (inference only)
6. Video synchronization or frame-accurate processing
7. Preset sharing/cloud synchronization features
8. Machine learning model training or fine-tuning within plugin
9. Support for legacy plugin formats (VST2, RTAS) - CLAP only
10. Advanced metering or spectrum analysis displays in UI
