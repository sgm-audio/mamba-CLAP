# PRD: PyTorch + Mamba CLAP Audio Effects Plugin

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

## Goals
1. Enable audio effects that understand and adapt to long-term musical context (60+ seconds)
2. Provide professional-quality audio processing that enhances musical expression
3. Maintain compatibility with standard DAW workflows through CLAP format
4. Deliver noticeable improvement over traditional fixed-parameter effects
5. Support real-time processing in Reaper DAW with acceptable latency

## Non-Goals
1. **Ultra-low latency processing** (<5ms) - Prioritizing quality and context awareness over minimal latency
2. **Replacement for all traditional effects** - Intelligent complement to, not replacement of, established DSP techniques
3. **Video/post-production applications** - Focused exclusively on music production use cases
4. **Multi-instrument separation** - Processing mixed audio stems, not source separation
5. **Physical modeling emulation** - Not attempting to replicate specific hardware units

## Success Metrics
1. **User Engagement**: 70% of test users continue using plugin after initial trial period
2. **Effect Quality**: 80% of blind A/B tests prefer Mamba CLAP effects over traditional equivalents for context-adaptive material
3. **Technical Performance**: <50ms average latency in Reaper with CPU fallback, <20ms with GPU acceleration
4. **Stability**: <1 crash per 10 hours of continuous operation in Reaper
5. **Adoption**: 100+ downloads/month from audio plugin distribution channels after launch

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

## Acceptance Criteria (Given/When/Then Format)

### Context-Aware Processing
**AC1.1**: Given a song with distinct quiet verse and loud chorus sections, When audio is processed through the Mamba CLAP reverb plugin, Then the reverb decay time shall be measurably shorter during verses than during choruses (minimum 20% difference)
**AC1.2**: Given a drum loop with consistent transient pattern, When processed through the Mamba CLAP compressor plugin, Then the compression ratio shall adapt to detected musical density, showing higher compression during busier sections

### Technical Performance
**AC2.1**: Given a 44.1kHz stereo audio stream, When processed through the plugin on CPU (Intel i5-8400), Then the average latency shall be less than 50ms as measured by loopback test
**AC2.2**: Given a 48kHz stereo audio stream, When processed through the plugin on GPU (NVIDIA GTX 1660), Then the average latency shall be less than 20ms as measured by loopback test
**AC2.3**: Given 10 minutes of continuous audio processing, When monitored for stability, Then the plugin shall produce zero crashes, audio dropouts, or audible artifacts

### CLAP Compliance
**AC3.1**: Given a Reaper DAW v6.0+, When the plugin is scanned and loaded, Then it shall appear in the plugin browser with correct name, vendor, and category
**AC3.2**: Given an instantiated plugin in Reaper, When audio is routed through it, Then it shall process stereo audio without channel inversion or phase issues
**AC3.3**: Given a plugin with automated parameters, When automation curves are drawn in Reaper, Then the plugin shall respond to parameter changes with no zipper noise or stepping artifacts

### User Experience
**AC4.1**: Given a first-time user, When they insert the plugin and load a preset, Then they shall be able to hear an audible effect difference between wet and dry signals within 30 seconds
**AC4.2**: Given a user adjusting the context length parameter, When they increase the value from 20s to 60s, Then they shall perceive a change in how the effect adapts to longer musical sections
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
