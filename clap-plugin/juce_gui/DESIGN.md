# Mamba Based: JUCE Frontend Architecture Design

This document outlines the architecture for a "visually pleasing," high-performance JUCE frontend for the Mamba audio processor. The design prioritizes a "High-Tech HUD" aesthetic, real-time feedback, and a seamless user experience.

## 1. Design Philosophy: "The Silicon Serpent"
The UI should feel like a high-performance terminal into a neural state space. It uses high-contrast colors, monospaced typography, and hardware-accelerated visualizations to convey the intelligence of the underlying Mamba SSM.

### Visual Palette
- **Background**: `Void Black` (#050505)
- **Primary Accent**: `Neon Green` (#00FF41) - Used for nominal states and active audio signals.
- **Secondary Accent**: `Alert Amber` (#FFB600) - Used for gain reduction and parameter highlights.
- **Typography**: monospaced fonts (JetBrains Mono) for numerical data; clean sans-serif (Inter) for labels.

## 2. Core Architectural Components

### A. `MambaLookAndFeel` (The "Skin")
A centralized class inheriting from `juce::LookAndFeel_V4` to define the global aesthetic.
- **Custom Sliders**: "Arc" style rotary sliders with glowing trails and minimalist value displays.
- **Custom Buttons**: HUD-style toggles with "active" glow states.
- **Global Font**: Integration of "JetBrains Mono" as the default typeface.

### B. Modular UI Components
The main editor is divided into logical, reusable modules:

1.  **Header Component (HUD)**:
    *   System status indicator (pulsing "NOMINAL" tag).
    *   Real-time latency and "Confidence" readout from the Mamba model.
    *   Animated "Mamba" logo.

2.  **Visualization Component (The "Neural View")**:
    *   **Spectrogram (OpenGL)**: A 60fps real-time STFT display showing input vs. enhanced spectrogram.
    *   **Temporal Gain Plot**: A scrolling "ribbon" showing the history of gain reduction, highlighting the model's lookahead and reasoning.
    *   **SSM State Visualizer**: A pulsing, abstract representation of the Mamba hidden state (e.g., a "latent space" particle system).

3.  **Control Panel Component**:
    *   **Primary Controls**: Large rotary sliders for Threshold, Ratio, and Makeup.
    *   **Ballistics Section**: High-precision sliders for Attack/Release.
    *   **Context Section**: Controls for Mamba-specific parameters (e.g., "History Window," "Neural Saturation").

### C. Technical Implementation

#### 1. Hardware Acceleration (`OpenGLContext`)
To ensure the UI remains responsive during complex visualizations, the `Neural View` component will use an `juce::OpenGLContext`. This allows the GPU to handle the pixel-heavy spectrogram rendering, freeing the CPU for audio processing and the main message thread for UI interaction.

#### 2. Layout Engine (`FlexBox` & `Grid`)
Instead of manual coordinate math, the editor will use `juce::FlexBox` for the top-level arrangement (Header, Visuals, Controls) and `juce::Grid` for the control knobs. This ensures a balanced, professional layout that scales gracefully.

#### 3. Thread-Safe Parameter Bridge
Leveraging `juce::AudioProcessorValueTreeState` (APVTS) to sync GUI components with the audio processor's parameters. Custom `SliderAttachment` and `ButtonAttachment` classes will be used to ensure thread-safety and DAW automation support.

## 3. Interaction & Animation
- **Hover States**: Components slightly "glow" or expand when hovered.
- **Signal Flow Animation**: Subtle "data stream" lines moving from the Mel Encoder visualization to the DSP output.
- **Parameter Modulation**: Visual indicators on sliders showing real-time modulation if the model is dynamically adjusting parameters.

## 4. File Structure (Proposed)
```
clap-plugin/juce_gui/
├── MambaLookAndFeel.h/cpp      # Global styling
├── Components/
│   ├── HeaderComponent.h/cpp   # HUD & Logo
│   ├── VisualizerComponent.h/cpp # OpenGL Spectrogram & SSM State
│   └── ControlPanel.h/cpp      # Knobs & Buttons
├── JucePluginEditor.h/cpp       # Main Editor (Orchestrator)
└── JucePluginProcessor.h/cpp    # Audio Engine (Existing)
```
