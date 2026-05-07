# Contributing to MambaCompressor

Thank you for your interest in contributing! We welcome contributions from the audio engineering and ML research communities.

## How to Contribute

### 1. Report Bugs
- Use GitHub Issues with the "bug" label
- Include: Python version, PyTorch version, GPU model, error traceback
- Provide minimal reproducible example

### 2. Suggest Features
- Open a GitHub Issue with "enhancement" label
- Describe the use case and expected behavior
- Discuss implementation approach before coding

### 3. Submit Code
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass
6. Submit a Pull Request

## Development Setup

```bash
# Clone your fork
git clone https://github.com/YOUR_USERNAME/mamba-compressor.git
cd mamba-compressor

# Install in editable mode with dev dependencies
pip install -e ".[dev]"

# Run tests
pytest tests/

# Format code
black src/ scripts/
isort src/ scripts/
```

## Code Style Guidelines

- **Type Hints**: Required for all function signatures
- **Docstrings**: Google style for all public classes/functions
- **Formatting**: Black (line length 100), isort for imports
- **Naming**: 
  - Classes: PascalCase
  - Functions/variables: snake_case
  - Constants: UPPER_SNAKE_CASE

## Testing Requirements

All PRs must include tests for:
- New functionality
- Bug fixes (regression tests)
- Edge cases (extreme inputs, boundary conditions)

Run tests locally before submitting:
```bash
pytest tests/ -v --cov=src
```

## Areas We Need Help

### High Priority
- [ ] CUDA kernel for IIR ballistic filtering (Triton or CUDA C++)
- [ ] Streaming inference pipeline with ring buffers
- [ ] Multiband extension (crossover filters + independent gain heads)
- [ ] VST3/AU plugin wrapper (JUCE + libtorch)

### Medium Priority
- [ ] NeMo ModelPT integration for distributed training
- [ ] Perceptual self-consistency loss implementation
- [ ] Gradio web demo for interactive experimentation
- [ ] A/B comparison framework vs. commercial compressors

### Nice to Have
- [ ] Additional DSP modules (EQ, limiter, saturation)
- [ ] Genre-specific pretrained checkpoints
- [ ] ONNX export support
- [ ] Documentation translations

## Questions?

Join our [Discussions](https://github.com/yourusername/mamba-compressor/discussions) or email audio.ai@example.com.

---

By contributing, you agree that your contributions will be licensed under the MIT License.
