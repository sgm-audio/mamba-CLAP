#!/usr/bin/env python3
"""
Basic usage example for the Mamba repository.
This script demonstrates how to use the core Mamba-SSM components.
"""

import torch
from mamba_ssm import Mamba, Mamba2, Mamba3

def example_mamba1():
    """Example using Mamba-1"""
    print("=== Mamba-1 Example ===")
    
    # Create model
    model = Mamba(
        d_model=256,      # Model dimension
        d_state=16,       # SSM state expansion factor
        d_conv=4,         # Local convolution width
        expand=2,         # Block expansion factor
    )
    
    # Create sample input
    batch_size, seq_len, dim = 2, 100, 256
    x = torch.randn(batch_size, seq_len, dim)
    
    # Forward pass
    with torch.no_grad():
        output = model(x)
    
    print(f"Input shape: {x.shape}")
    print(f"Output shape: {output.shape}")
    print(f"Output mean: {output.mean().item():.4f}")
    print()

def example_mamba2():
    """Example using Mamba-2"""
    print("=== Mamba-2 Example ===")
    
    # Create model
    model = Mamba2(
        d_model=256,      # Model dimension
        d_state=64,       # SSM state expansion factor (typically 64 or 128 for Mamba-2)
        d_conv=4,         # Local convolution width
        expand=2,         # Block expansion factor
    )
    
    # Create sample input
    batch_size, seq_len, dim = 2, 100, 256
    x = torch.randn(batch_size, seq_len, dim)
    
    # Forward pass
    with torch.no_grad():
        output = model(x)
    
    print(f"Input shape: {x.shape}")
    print(f"Output shape: {output.shape}")
    print(f"Output mean: {output.mean().item():.4f}")
    print()

def example_mamba3():
    """Example using Mamba-3"""
    print("=== Mamba-3 Example ===")
    
    # Create model (using bfloat16 for efficiency as in the paper)
    model = Mamba3(
        d_model=256,      # Model dimension
        d_state=128,      # SSM state size
        headdim=64,       # SSM headdim
        is_mimo=True,     # Use MIMO mode
        mimo_rank=4,      # MIMO rank when is_mimo=True
        chunk_size=16,    # 64/mimo_rank if x is in bf16, else 32/mimo_rank
        is_outproj_norm=False,  # Additional post SSM norm
        dtype=torch.bfloat16,
    )
    
    # Create sample input (using bfloat16)
    batch_size, seq_len, dim = 2, 100, 256
    x = torch.randn(batch_size, seq_len, dim, dtype=torch.bfloat16)
    
    # Forward pass
    with torch.no_grad():
        output = model(x)
    
    print(f"Input shape: {x.shape}")
    print(f"Output shape: {output.shape}")
    print(f"Output mean: {output.mean().item():.4f}")
    print()

if __name__ == "__main__":
    print("Mamba Repository Basic Usage Examples\n")
    
    try:
        example_mamba1()
    except Exception as e:
        print(f"Mamba-1 example failed: {e}\n")
    
    try:
        example_mamba2()
    except Exception as e:
        print(f"Mamba-2 example failed: {e}\n")
        
    try:
        example_mamba3()
    except Exception as e:
        print(f"Mamba-3 example failed: {e}\n")
    
    print("Examples completed!")