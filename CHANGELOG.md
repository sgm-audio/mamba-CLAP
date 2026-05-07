# Changelog

All notable changes to the **mamba-CLAP** project will be documented in this file.

## [1.1.0] - 2026-05-06

### Added
- **JUCE Frontend Architecture**: Comprehensive design blueprint for a high-fidelity "Neural HUD" UI.
- **MambaLookAndFeel**: Custom JUCE styling engine with neon-green aesthetics and HUD-style rotary sliders.
- **VisualizerComponent**: OpenGL-accelerated visualization skeleton for real-time spectrograms and SSM state monitoring.
- **Architectural Documentation**: Added `clap-plugin/juce_gui/DESIGN.md` detailing the visual philosophy and technical implementation.

## [1.0.0] - 2026-05-06

### Added
- **Initial Monorepo Structure**: Flattened and merged `mamba` and `mamba-compressor` into a unified ecosystem.
- **CLAP Plugin Core**: C++ foundation for the Clever Audio Plugin standard integration.
- **Mamba SSM Generations**: Inclusion of Mamba-1, Mamba-2 (SSD), and Mamba-3 architectures.
- **Context-Aware Compressor**: Specialized audio application using selective state space models.
- **Enhanced Audio Utilities**: PyTorch implementations of spectrogram predictors and multi-resolution STFT losses.
- **GEMINI.md**: Foundational project context and instruction set.

### Removed
- Unrelated "SGM Personal OS" web and branding components (`sgm_os_deploy.py`, `dist_sgm/`).
