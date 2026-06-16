# Mamba API Reference

## Core Modules

### Mamba-SSM (`mamba_ssm`)

The core Mamba State Space Model implementation.

#### Main Classes

- `Mamba` - Standard Mamba-1 block
- `Mamba2` - Mamba-2 (State Space Duality) block
- `Mamba3` - Mamba-3 (Inference-first) block

#### Usage Example

```python
import torch
from mamba_ssm import Mamba

# Create a Mamba-1 model
model = Mamba(
    d_model=512,      # Model dimension
    d_state=16,       # SSM state expansion factor
    d_conv=4,         # Local convolution width
    expand=2,         # Block expansion factor
)

# Forward pass
x = torch.randn(2, 100, 512)  # (batch, sequence, dim)
output = model(x)
```

For detailed API references, see the source code documentation in the respective modules:
- [`mamba_ssm/modules/mamba_simple.py`](mamba_ssm/modules/mamba_simple.py)
- [`mamba_ssm/modules/mamba2.py`](mamba_ssm/modules/mamba2.py)
- [`mamba_ssm/modules/mamba3.py`](mamba_ssm/modules/mamba3.py)
