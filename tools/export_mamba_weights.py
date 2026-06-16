#!/usr/bin/env python3
"""
export_mamba_weights.py

Exports a trained PyTorch Mamba model into the custom binary format
consumed by the C++ MambaWeightLoader.

Usage:
    python tools/export_mamba_weights.py \
        --model-path model.pt \
        --output weights.mmbw \
        --d-model 1 \
        --n-layers 2 \
        --d-state 16 \
        --d-conv 4 \
        --expand 2

The script attempts to auto-infer architecture from the checkpoint keys,
but you can override any parameter with the CLI flags.
"""

import argparse
import struct
import sys
from pathlib import Path

try:
    import torch
except ImportError:
    print("ERROR: PyTorch is required. Install with: pip install torch")
    sys.exit(1)


MAGIC = 0x574D424D  # "MMBW"
VERSION = 1


def _write_header(f, n_layers, d_model, d_state, d_conv, expand, flags=0):
    """Write 28-byte little-endian header."""
    f.write(
        struct.pack("<IIIIIIII", MAGIC, VERSION, n_layers, d_model, d_state, d_conv, expand, flags)
    )


def _write_floats(f, tensor):
    """Write a PyTorch tensor as raw little-endian float32s."""
    flat = tensor.detach().cpu().flatten().numpy()
    f.write(flat.astype("<f4").tobytes())


def export_from_checkpoint(model, output_path, d_model, n_layers, d_state, d_conv, expand):
    """
    model : torch.nn.Module (your Mamba model instance)
    """
    d_inner = expand * d_model  # same convention as C++ code

    with open(output_path, "wb") as f:
        _write_header(f, n_layers, d_model, d_state, d_conv, expand)

        # --- Save per-layer weights ---
        for layer_idx in range(n_layers):
            # These keys are illustrative; adapt to your actual model naming.
            prefix = f"layers.{layer_idx}."

            # 1. Input projection:  [d_model, 2*d_inner]
            in_proj = model.get_parameter(f"{prefix}in_proj.weight")
            # Expected shape: (2*d_inner, d_model). Transpose to match C++ layout
            assert in_proj.shape == (
                2 * d_inner,
                d_model,
            ), f"in_proj shape mismatch: {in_proj.shape} vs ({2*d_inner}, {d_model})"
            _write_floats(f, in_proj.T)  # -> [d_model, 2*d_inner]

            # 2. Output projection: [d_inner, d_model]
            out_proj = model.get_parameter(f"{prefix}out_proj.weight")
            assert out_proj.shape == (
                d_model,
                d_inner,
            ), f"out_proj shape mismatch: {out_proj.shape} vs ({d_model}, {d_inner})"
            _write_floats(f, out_proj.T)  # -> [d_inner, d_model]

            # 3. Conv1d: [d_inner, d_conv]
            conv = model.get_parameter(f"{prefix}conv1d.weight")
            assert conv.shape == (
                d_inner,
                d_conv,
            ), f"conv1d shape mismatch: {conv.shape} vs ({d_inner}, {d_conv})"
            _write_floats(f, conv)

            # 4. dA: [d_inner, d_state]
            dA = model.get_parameter(f"{prefix}dA")
            assert dA.shape == (
                d_inner,
                d_state,
            ), f"dA shape mismatch: {dA.shape} vs ({d_inner}, {d_state})"
            _write_floats(f, dA)

            # 5. dB: [d_inner, d_state]
            dB = model.get_parameter(f"{prefix}dB")
            assert dB.shape == (
                d_inner,
                d_state,
            ), f"dB shape mismatch: {dB.shape} vs ({d_inner}, {d_state})"
            _write_floats(f, dB)

            # 6. C: [d_inner, d_state]
            C = model.get_parameter(f"{prefix}C")
            assert C.shape == (
                d_inner,
                d_state,
            ), f"C shape mismatch: {C.shape} vs ({d_inner}, {d_state})"
            _write_floats(f, C)

            # 7. D: [d_inner]
            D = model.get_parameter(f"{prefix}D")
            assert D.shape == (d_inner,), f"D shape mismatch: {D.shape} vs ({d_inner},)"
            _write_floats(f, D)

    print(f"[export] Wrote {output_path}  ({Path(output_path).stat().st_size} bytes)")


def validate_export(path, n_layers, d_model, d_state, d_conv, expand):
    """Read back and sanity-check the exported file."""
    d_inner = expand * d_model

    # ... or just parse raw bytes for a quick sanity check

    with open(path, "rb") as f:
        magic, version, n_layers_f, d_model_f, d_state_f, d_conv_f, expand_f, flags = struct.unpack(
            "<IIIIIIII", f.read(28)
        )

    assert magic == MAGIC, "Magic mismatch"
    assert version == VERSION, "Version mismatch"
    assert n_layers_f == n_layers
    assert d_model_f == d_model
    assert d_state_f == d_state
    assert d_conv_f == d_conv
    assert expand_f == expand

    print("[validate] Header OK")

    # Quick size sanity check
    expected_per_layer = (
        d_model * 2 * d_inner
        + d_model * d_inner
        + d_inner * d_conv
        + d_inner * d_state * 3
        + d_inner
    ) * 4
    actual_size = Path(path).stat().st_size
    expected_size = 28 + n_layers * expected_per_layer
    assert actual_size == expected_size, f"Size mismatch: {actual_size} vs {expected_size}"
    print(f"[validate] File size OK  ({actual_size} bytes)")


def main():
    parser = argparse.ArgumentParser(description="Export PyTorch Mamba model to binary format")
    parser.add_argument("--model-path", required=True, help="Path to trained PyTorch model (.pt)")
    parser.add_argument("--output", required=True, help="Output binary file path (.mmbw)")
    parser.add_argument("--d-model", type=int, default=1, help="Model dimension")
    parser.add_argument("--n-layers", type=int, default=2, help="Number of Mamba layers")
    parser.add_argument("--d-state", type=int, default=16, help="State space dimension")
    parser.add_argument("--d-conv", type=int, default=4, help="Causal conv kernel size")
    parser.add_argument("--expand", type=int, default=2, help="Expansion factor")
    parser.add_argument("--validate", action="store_true", help="Validate after export")
    args = parser.parse_args()

    print("Loading model...")
    model = torch.load(args.model_path, map_location="cpu")
    model.eval()

    print("Exporting...")
    export_from_checkpoint(
        model,
        args.output,
        d_model=args.d_model,
        n_layers=args.n_layers,
        d_state=args.d_state,
        d_conv=args.d_conv,
        expand=args.expand,
    )

    if args.validate:
        validate_export(
            args.output, args.n_layers, args.d_model, args.d_state, args.d_conv, args.expand
        )


if __name__ == "__main__":
    main()
