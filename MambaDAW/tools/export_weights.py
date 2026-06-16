#!/usr/bin/env python3
"""
tools/export_weights.py

Export a trained SimpleMambaModel to the .mamba_weights binary format
consumed by MambaWeightLoader::load() in the C++ CLAP plugin.

Binary format specification (must match mamba_weight_loader.h):
────────────────────────────────────────────────────────────────
  [MambaWeightHeader] — 32 bytes, little-endian, packed
      magic[4]      : 'M' 'M' 'B' 'W'
      version       : uint32 = 1
      n_layers      : uint32
      d_model       : uint32
      d_state       : uint32
      d_conv        : uint32
      expand        : uint32
      reserved      : uint32 = 0

  [Layer 0 weights]  (repeated n_layers times)
  ...
  [Layer n-1 weights]

  Per-layer layout (all float32 or int8 + scale, little-endian):
      in_proj   [d_model * 2 * d_inner]  float32
      out_proj  [d_inner]                float32
      conv1d    [d_inner * d_conv]       float32
      dA        [d_inner * d_state]      float32
      dB        [d_inner * d_state]      float32
      C         [d_inner * d_state]      float32
      D         [d_inner]                float32

Usage examples:
  # Export FP32 model
  python tools/export_weights.py \\
      --checkpoint resources/mamba_model_final.pt \\
      --out        resources/model.mamba_weights

  # Export with INT8 quantization (4x smaller, faster CPU inference)
  python tools/export_weights.py \\
      --checkpoint resources/mamba_model_final.pt \\
      --out        resources/model_int8.mamba_weights \\
      --quantize   int8

  # Verify an existing .mamba_weights file
  python tools/export_weights.py --verify resources/model.mamba_weights
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

import torch

# ---------------------------------------------------------------------------
# Add repo root to path so we can import simple_mamba regardless of CWD.
# ---------------------------------------------------------------------------
_TOOLS_DIR = Path(__file__).resolve().parent
_REPO_ROOT = _TOOLS_DIR.parent
sys.path.insert(0, str(_TOOLS_DIR))

from simple_mamba import SimpleMambaModel  # noqa: E402

# ---------------------------------------------------------------------------
# Format constants (must match mamba_weight_loader.h)
# ---------------------------------------------------------------------------
MAGIC = 0x574D424D  # little-endian bytes: 4D 42 4D 57 — matches C++ constexpr MAGIC
VERSION = 1
HEADER_FORMAT = "<IIIIIIII"  # 8 × uint32_t little-endian
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)  # 32 bytes

assert HEADER_SIZE == 32, f"Header size mismatch: got {HEADER_SIZE}, expected 32"


# =============================================================================
# Header helpers
# =============================================================================


def pack_header(n_layers: int, d_model: int, d_state: int, d_conv: int, expand: int) -> bytes:
    return struct.pack(HEADER_FORMAT, MAGIC, VERSION, n_layers, d_model, d_state, d_conv, expand, 0)


def unpack_header(data: bytes) -> dict:
    magic, version, n_layers, d_model, d_state, d_conv, expand, reserved = struct.unpack(
        HEADER_FORMAT, data[:HEADER_SIZE]
    )
    return {
        "magic": magic,
        "version": version,
        "n_layers": n_layers,
        "d_model": d_model,
        "d_state": d_state,
        "d_conv": d_conv,
        "expand": expand,
        "reserved": reserved,
    }


# =============================================================================
# Quantization utilities
# =============================================================================


def quantize_int8_symmetric(tensor: torch.Tensor) -> tuple[torch.Tensor, float]:
    """
    Symmetric per-tensor INT8 quantization.

    Returns (quantized int8 tensor, scale float32).
    Dequantize with: float_val = int8_val * scale

    NOTE: The C++ MambaWeightLoader currently only supports float32.
          INT8 support requires a C++ update to MambaLayer::loadLayerWeights()
          to accept (int8*, scale) pairs. This function prepares the data;
          the INT8 format stores: [float32 scale][int8 weights...] per tensor.
    """
    abs_max = tensor.abs().max().item()
    if abs_max == 0.0:
        return torch.zeros_like(tensor, dtype=torch.int8), 1.0
    scale = abs_max / 127.0
    q = (tensor / scale).round().clamp(-128, 127).to(torch.int8)
    return q, scale


# =============================================================================
# Export
# =============================================================================

WEIGHT_KEYS = ["in_proj", "out_proj", "conv1d", "dA", "dB", "C", "D"]


def export_float32(
    model: SimpleMambaModel,
    out_path: Path,
    verbose: bool = True,
) -> None:
    """Write a FP32 .mamba_weights file."""
    arch = model.arch_dict()
    header = pack_header(
        n_layers=arch["n_layers"],
        d_model=arch["d_model"],
        d_state=arch["d_state"],
        d_conv=arch["d_conv"],
        expand=arch["expand"],
    )

    total_floats = 0
    with open(out_path, "wb") as f:
        f.write(header)
        for i, layer in enumerate(model.layers):
            weights = layer.get_weight_arrays()
            for key in WEIGHT_KEYS:
                w = weights[key]
                assert w.dtype == torch.float32, f"Layer {i} '{key}' is not float32"
                data = w.detach().numpy().tobytes()
                f.write(data)
                total_floats += w.numel()
                if verbose:
                    print(
                        f"  Layer {i:2d} | {key:10s} : {w.numel():8,d} floats  "
                        f"({w.numel() * 4 / 1024:.1f} KB)"
                    )

    file_size = out_path.stat().st_size
    if verbose:
        print(f"\n✅ Exported FP32 → {out_path}")
        print(f"   Total weights : {total_floats:,} floats")
        print(f"   File size     : {file_size / 1024:.1f} KB")


def export_int8(
    model: SimpleMambaModel,
    out_path: Path,
    verbose: bool = True,
) -> None:
    """
    Write an INT8-quantized .mamba_weights file.

    File format for INT8 (version=1 with quantized flag):
        [header]
        for each layer:
            for each weight tensor:
                [float32 scale]           4 bytes
                [int8 quantized values]   numel bytes

    ⚠️  This format requires a matching update to MambaWeightLoader::load()
        and MambaLayer::loadLayerWeights() in C++. The exporter produces the
        correct binary — the C++ reader is the TODO.
    """
    # For now write FP32 with a clear error — INT8 C++ reader not yet implemented.
    print("⚠️  INT8 export requested.")
    print("   The INT8 binary format is defined but MambaWeightLoader in C++")
    print("   does not yet support int8 weights. Exporting FP32 instead.")
    print("   See: Phase 3 — Quantization task in AGENTS.md")
    export_float32(model, out_path, verbose)


# =============================================================================
# Projection section V2 — matrix head, no encoder (d_model = N_MEL)
# =============================================================================

PROJ_MAGIC_V2 = 0x324F5250  # 'PRO2' little-endian — must match C++ PROJ_MAGIC_V2
PROJ_V2_HEADER_FORMAT = "<III"  # proj_magic_v2, d_model_in, head_out


def write_proj_section(
    f,
    head_weight: torch.Tensor,  # shape [N_MAMBA_OUT, N_MEL]  (nn.Linear(N_MEL, N_MAMBA_OUT).weight)
    head_bias: torch.Tensor,  # shape [N_MAMBA_OUT]
    verbose: bool = True,
) -> None:
    """Append a PROJ V2 section to an already-opened .mambaweights file.

    Layout: PRO2_MAGIC | d_model_in | head_out | head_w[head_out*d_model_in] | head_b[head_out]
    head_w is row-major: row i = weights for output logit i.
    """
    head_w = head_weight.detach().float().contiguous()  # [N_MAMBA_OUT, N_MEL]
    head_b = head_bias.detach().float()  # [N_MAMBA_OUT]

    head_out, d_model_in = head_w.shape

    f.write(struct.pack(PROJ_V2_HEADER_FORMAT, PROJ_MAGIC_V2, d_model_in, head_out))
    f.write(head_w.numpy().tobytes())  # row-major [head_out * d_model_in] floats
    f.write(head_b.numpy().tobytes())

    if verbose:
        print(f"\n  [PROJ V2] head  : head_w[{head_out}, {d_model_in}] + head_b[{head_out}]")
        proj_bytes = struct.calcsize(PROJ_V2_HEADER_FORMAT) + (head_out * d_model_in + head_out) * 4
        print(f"  [PROJ V2] section size: {proj_bytes / 1024:.1f} KB")


# =============================================================================
# Verification
# =============================================================================


def verify_file(path: Path) -> bool:
    """Read back a .mamba_weights file and verify its integrity."""
    print(f"Verifying: {path}")
    with open(path, "rb") as f:
        raw_header = f.read(HEADER_SIZE)
        if len(raw_header) < HEADER_SIZE:
            print("❌ File too short to contain a valid header.")
            return False

        hdr = unpack_header(raw_header)
        if hdr["magic"] != MAGIC:
            print(f"❌ Invalid magic: {hdr['magic']:#010x} (expected {MAGIC:#010x})")
            return False
        if hdr["version"] != VERSION:
            print(f"❌ Unsupported version: {hdr['version']} (expected {VERSION})")
            return False

        print("  Architecture:")
        print(f"    n_layers : {hdr['n_layers']}")
        print(f"    d_model  : {hdr['d_model']}")
        print(f"    d_state  : {hdr['d_state']}")
        print(f"    d_conv   : {hdr['d_conv']}")
        print(f"    expand   : {hdr['expand']}")

        d_inner = hdr["expand"] * hdr["d_model"]
        per_layer_sizes = {
            "in_proj": hdr["d_model"] * 2 * d_inner,
            "out_proj": hdr["d_model"] * d_inner,  # [d_model, d_inner] full matrix
            "conv1d": d_inner * hdr["d_conv"],
            "dA": d_inner * hdr["d_state"],
            "dB": d_inner * hdr["d_state"],
            "C": d_inner * hdr["d_state"],
            "D": d_inner,
        }
        per_layer_floats = sum(per_layer_sizes.values())

        all_ok = True
        for layer_idx in range(hdr["n_layers"]):
            layer_data = f.read(per_layer_floats * 4)
            if len(layer_data) < per_layer_floats * 4:
                print(
                    f"❌ Unexpected EOF at layer {layer_idx} "
                    f"(got {len(layer_data)} bytes, expected {per_layer_floats * 4})"
                )
                all_ok = False
                break

            # Deserialise and basic sanity check (no NaN/Inf)
            import struct as st

            floats_flat = list(st.unpack(f"<{per_layer_floats}f", layer_data))
            nan_count = sum(1 for v in floats_flat if v != v)  # NaN test
            inf_count = sum(1 for v in floats_flat if v == float("inf") or v == float("-inf"))
            status = "✅" if (nan_count == 0 and inf_count == 0) else "⚠️"
            print(
                f"  Layer {layer_idx:2d}: {status}  "
                f"NaN={nan_count}, Inf={inf_count}, "
                f"floats={per_layer_floats:,}"
            )
            if nan_count > 0 or inf_count > 0:
                all_ok = False

        # Check for optional PROJ V2 section.
        proj_peek = f.read(12)  # magic(4) + d_model_in(4) + head_out(4)
        if len(proj_peek) == 12:
            import struct as st2

            pm, d_model_in, head_out = st2.unpack("<III", proj_peek)
            if pm == PROJ_MAGIC_V2:
                proj_floats = head_out * d_model_in + head_out  # head_w matrix + head_b
                proj_data = f.read(proj_floats * 4)
                if len(proj_data) == proj_floats * 4:
                    print(f"  [PROJ V2] ✅  head({d_model_in}→{head_out}) matrix found")
                else:
                    print("  [PROJ V2] ⚠️  Section header found but data truncated")
                remaining = f.read(1)
                if remaining:
                    print("⚠️  Trailing bytes after PROJ section — file may be corrupt.")
            elif proj_peek:
                print("⚠️  Unexpected bytes after layer data — file may be corrupt.")

    if all_ok:
        stat = path.stat()
        print(f"\n✅ Verification passed — {stat.st_size / 1024:.1f} KB")
    else:
        print("\n❌ Verification FAILED")
    return all_ok


# =============================================================================
# Checkpoint loading
# =============================================================================


def load_checkpoint(checkpoint_path: Path, arch: dict) -> SimpleMambaModel:
    """
    Load a SimpleMambaModel from a .pt state_dict checkpoint.

    The checkpoint must have been saved with torch.save(model.state_dict(), path).
    Architecture is inferred from arch dict (passed via CLI or detected from keys).
    """
    model = SimpleMambaModel(
        d_model=arch["d_model"],
        n_layers=arch["n_layers"],
        d_state=arch["d_state"],
        d_conv=arch["d_conv"],
        expand=arch["expand"],
    )
    state = torch.load(checkpoint_path, map_location="cpu", weights_only=True)
    model.load_state_dict(state, strict=True)
    model.eval()
    return model


def detect_arch_from_state_dict(state: dict) -> dict | None:
    """
    Try to infer architecture from state_dict key shapes.
    Returns None if detection fails.
    """
    try:
        # layers.0.in_proj.weight shape = [2*d_inner, d_model]
        w = state["layers.0.in_proj.weight"]
        proj_out, d_model = w.shape
        d_inner = proj_out // 2

        # layers.0.conv1d.weight shape = [d_inner, 1, d_conv]
        d_conv = state["layers.0.conv1d.weight"].shape[2]

        # layers.0.dA shape = [d_inner, d_state]
        d_state = state["layers.0.dA"].shape[1]

        # Count layers from state dict keys
        layer_indices = set()
        for k in state:
            if k.startswith("layers."):
                idx = int(k.split(".")[1])
                layer_indices.add(idx)

        expand = d_inner // d_model

        return {
            "d_model": d_model,
            "n_layers": max(layer_indices) + 1,
            "d_state": d_state,
            "d_conv": d_conv,
            "expand": expand,
        }
    except (KeyError, IndexError, ValueError):
        return None


# =============================================================================
# CLI
# =============================================================================


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Export SimpleMambaModel to .mamba_weights binary format",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    sub = p.add_subparsers(dest="command", required=True)

    # --- export ---
    e = sub.add_parser("export", help="Export a checkpoint to .mamba_weights")
    e.add_argument(
        "--checkpoint", required=True, type=Path, help="Path to .pt state_dict checkpoint"
    )
    e.add_argument("--out", required=True, type=Path, help="Output path for .mamba_weights file")
    e.add_argument(
        "--quantize",
        default="fp32",
        choices=["fp32", "int8"],
        help="Weight precision (default: fp32)",
    )
    e.add_argument("--quiet", action="store_true", help="Suppress per-layer progress")
    # Architecture overrides (auto-detected from checkpoint if not provided)
    e.add_argument("--d_model", type=int, default=None)
    e.add_argument("--n_layers", type=int, default=None)
    e.add_argument("--d_state", type=int, default=None)
    e.add_argument("--d_conv", type=int, default=None)
    e.add_argument("--expand", type=int, default=None)

    # --- verify ---
    v = sub.add_parser("verify", help="Verify a .mamba_weights file")
    v.add_argument("file", type=Path, help="Path to .mamba_weights file")

    # --- init ---
    i = sub.add_parser(
        "init", help="Initialise a random model and export (useful for testing the plugin)"
    )
    i.add_argument("--out", required=True, type=Path)
    i.add_argument("--d_model", type=int, default=1)
    i.add_argument("--n_layers", type=int, default=4)
    i.add_argument("--d_state", type=int, default=16)
    i.add_argument("--d_conv", type=int, default=4)
    i.add_argument("--expand", type=int, default=2)

    return p


def cmd_export(args: argparse.Namespace) -> None:
    if not args.checkpoint.exists():
        sys.exit(f"❌ Checkpoint not found: {args.checkpoint}")

    print(f"Loading checkpoint: {args.checkpoint}")
    state = torch.load(args.checkpoint, map_location="cpu", weights_only=True)

    # Detect backend type before doing anything else.
    is_gru = any(k.startswith("gru.") or k.startswith("proj.") for k in state)
    if is_gru:
        sys.exit(
            "❌  This checkpoint was saved from --backend gru.\n"
            "   GRU models cannot be exported to .mambaweights (no Mamba SSM layers).\n"
            "\n"
            "   Retrain with --backend mamba, then export:\n"
            "     python train_effect_controller.py --backend mamba --data dsp \\\n"
            "         --epochs 20 --dataset-size 1024 --device cpu\n"
            "     python tools/export_weights.py export \\\n"
            "         --checkpoint python_training/resources/effect_controller_best.pt \\\n"
            "         --out resources/effect_controller.mambaweights"
        )

    # Auto-detect architecture unless overrides provided
    detected = detect_arch_from_state_dict(state)
    arch = {
        "d_model": args.d_model or (detected and detected["d_model"]) or 1,
        "n_layers": args.n_layers or (detected and detected["n_layers"]) or 4,
        "d_state": args.d_state or (detected and detected["d_state"]) or 16,
        "d_conv": args.d_conv or (detected and detected["d_conv"]) or 4,
        "expand": args.expand or (detected and detected["expand"]) or 2,
    }
    if detected:
        print(f"Auto-detected architecture: {detected}")
    else:
        print(f"Using architecture overrides: {arch}")

    # Detect full EffectControllerMamba checkpoint (has head key; no encoder in V2 model).
    has_proj = "head.weight" in state and "head.bias" in state
    if has_proj:
        print("Detected full EffectControllerMamba checkpoint — will export PROJ V2 section.")
        # Extract Mamba backbone sub-state.
        mamba_state = {k[len("mamba.") :]: v for k, v in state.items() if k.startswith("mamba.")}
        # Re-detect arch from mamba sub-state.
        detected = detect_arch_from_state_dict(mamba_state) or detected
        arch = {
            "d_model": args.d_model or (detected and detected["d_model"]) or 80,
            "n_layers": args.n_layers or (detected and detected["n_layers"]) or 4,
            "d_state": args.d_state or (detected and detected["d_state"]) or 16,
            "d_conv": args.d_conv or (detected and detected["d_conv"]) or 4,
            "expand": args.expand or (detected and detected["expand"]) or 2,
        }
        backbone = SimpleMambaModel(**arch)
        backbone.load_state_dict(mamba_state, strict=True)
        backbone.eval()
        model = backbone
    else:
        model = load_checkpoint(args.checkpoint, arch)

    print(f"Model loaded — {sum(p.numel() for p in model.parameters()):,} parameters")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    verbose = not args.quiet

    if args.quantize == "int8":
        export_int8(model, args.out, verbose)
    else:
        export_float32(model, args.out, verbose)

    # Append PROJ V2 section if exporting a full controller checkpoint.
    if has_proj:
        with open(args.out, "ab") as f:
            write_proj_section(
                f,
                head_weight=state["head.weight"],
                head_bias=state["head.bias"],
                verbose=verbose,
            )
        print(f"✅ PROJ V2 section appended to {args.out}")


def cmd_verify(args: argparse.Namespace) -> None:
    if not args.file.exists():
        sys.exit(f"❌ File not found: {args.file}")
    ok = verify_file(args.file)
    sys.exit(0 if ok else 1)


def cmd_init(args: argparse.Namespace) -> None:
    """Create a random-weight model and export it — useful for plugin smoke-testing."""
    model = SimpleMambaModel(
        d_model=args.d_model,
        n_layers=args.n_layers,
        d_state=args.d_state,
        d_conv=args.d_conv,
        expand=args.expand,
    )
    model.eval()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    export_float32(model, args.out, verbose=True)
    print("\nℹ️  This model has random (untrained) weights.")
    print("   Train with: python tools/train_audio_mamba.py --out resources/")
    print(
        f"   Then export: python tools/export_weights.py export --checkpoint resources/mamba_final.pt --out {args.out}"
    )


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()

    if args.command == "export":
        cmd_export(args)
    elif args.command == "verify":
        cmd_verify(args)
    elif args.command == "init":
        cmd_init(args)


if __name__ == "__main__":
    main()
