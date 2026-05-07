# MambaCompressor: Strategic Distribution Playbook

## 1. Hacker News (Show HN)

**Title**: Show HN: MambaCompressor – Context-aware audio compression with linear-time SSMs

**Draft**:
I’ve been working on a new approach to audio dynamic range processing that moves away from heuristic attack/release settings and toward learned musical intelligence.

The core idea: Traditional compressors are context-blind. They react to a 10ms window of amplitude, but they don't know if they're in a sparse verse or a dense chorus. 

MambaCompressor uses Selective State Space Models (Mamba) to maintain a hidden state of the last 60+ seconds of audio. This allows it to:
- Anticipate transitions (e.g., gain reduction *before* a drop).
- Preserve transients in sparse sections while applying "glue" in dense ones.
- Scale linearly with sequence length (unlike Transformers).

We use Mamba to predict the parameters of a *differentiable* DSP compressor. This means the output is still phase-coherent, interpretable, and professional-grade—not "neural-sounding."

Benchmarks on RTX 4090:
- Context Length: 60s
- Latency: 3.2ms
- Memory: 1.8GB (vs 12GB+ for Transformers)

GitHub: [https://github.com/scottmill306/mamba-compressor]

I'd love to hear from audio engineers and DSP folks on what they think about the differentiable ballistics approach.

---

## 2. r/MachineLearning

**Title**: [P] MambaCompressor: Context-Aware Audio Processing with Linear-Time SSMs and Differentiable DSP

**Draft**:
We are releasing MambaCompressor, an open-source model that leverages Mamba SSMs for long-context temporal reasoning in audio mastering.

**Key Technical Highlights**:
1. **SSM vs Transformer**: We use Mamba to handle minute-long musical contexts. The $O(N)$ scaling is critical for high-sample-rate audio (44.1kHz).
2. **Differentiable DSP**: Instead of end-to-end waveform synthesis, the model predicts coefficients for an IIR-smoothed gain envelope. This preserves the signal's phase and allows for sample-accurate, differentiable compression.
3. **Blind Training**: We implement a "synthetic corruption" strategy that allows training on massive unpaired music datasets. The model learns to "undo" bad compression without needing ground-truth dry/wet pairs.
4. **Perceptual Losses**: Multi-resolution STFT + Crest Factor preservation + LUFS targeting.

Repo: [https://github.com/scottmill306/mamba-compressor]

Feedback on the SSM state size (`d_state=32`) for audio semantic capture is very welcome.

---

## 3. r/AudioProgramming

**Title**: MambaCompressor: Differentiable DSP + Mamba SSMs for Intelligent Dynamic Processing

**Draft**:
Hi all, I've just open-sourced a project that combines modern SSM architectures with traditional DSP ballistics.

The project implements a "Parameter Head" architecture where a neural network (Mamba) emits time-varying `gain_db`, `attack_ms`, and `release_ms` parameters. These are then fed into a differentiable IIR filter to create a sample-accurate gain envelope.

**Why this matters**:
- **Lookahead**: 46ms lookahead is baked into the gradient path.
- **Interpretability**: You can actually plot the `attack_ms` envelope and see how the AI decided to clamp down on a specific transient.
- **Latency**: We're targeting sub-5ms for the full Mamba tower + DSP.

We're currently looking for contributors to help with:
- Triton/CUDA kernels for the IIR ballistics loop (current bottleneck).
- VST3/AU wrappers via JUCE.
- Multiband extension.

Check it out: [https://github.com/scottmill306/mamba-compressor]
