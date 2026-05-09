#!/usr/bin/env python3
"""
tools/train_audio_mamba.py

End-to-end waveform training for the MambaDAW scalar audio SSM.

Task: Context-Aware Reverb (learns a reverb tail from dry/wet audio pairs).
If no real dataset is provided, it generates a synthetic decaying RIR dataset.

Quick start:
    python tools/train_audio_mamba.py

Full pipeline:
    python tools/train_audio_mamba.py \
        --epochs 50 --batch_size 32 --seq_len 8192 \
        --out_dir resources/ --device cpu \
        --rt60 1.5

Then export:
    python tools/export_weights.py export \
        --checkpoint resources/mamba_best.pt \
        --out resources/mamba.mamba_weights
"""

from __future__ import annotations

import argparse
import math
import os
import sys
import time
from pathlib import Path

import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np

_TOOLS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(_TOOLS_DIR))

from simple_mamba import SimpleMambaModel  # noqa: E402

# =============================================================================
# Reverb dataset
# =============================================================================

def _make_signal(length: int, sr: int = 44100, rng=None) -> np.ndarray:
    """Generate a clean signal: short staccato bursts (good for reverb training)."""
    if rng is None:
        rng = np.random.default_rng()
    t = np.linspace(0, length / sr, length, dtype=np.float32)
    sig = np.zeros(length, dtype=np.float32)
    n_bursts = rng.integers(2, 6)
    
    for _ in range(n_bursts):
        freq = rng.uniform(200, 2000)
        amp  = rng.uniform(0.1, 0.9)
        # Create a short envelope burst
        burst_len = rng.integers(sr // 20, sr // 5)
        start_idx = rng.integers(0, max(1, length - burst_len))
        
        burst_t = t[:burst_len]
        env = np.exp(-15.0 * burst_t)  # sharp decay
        sig[start_idx:start_idx+burst_len] += amp * env * np.sin(2 * math.pi * freq * burst_t)
        
    peak = np.max(np.abs(sig)) + 1e-8
    return sig / peak

class ReverbDataset(torch.utils.data.Dataset):
    """
    Dataset of (dry_audio, wet_audio) waveform pairs.
    Generates synthetic RIRs using exponential noise decay if no audio dir is provided.
    """

    def __init__(
        self,
        n_sequences: int = 500,
        seq_len: int = 8192,
        sample_rate: int = 44100,
        rt60: float = 1.5,
        seed: int = 42,
    ) -> None:
        super().__init__()
        rng = np.random.default_rng(seed)
        self.seq_len = seq_len

        print(f"Generating synthetic reverb dataset (rt60={rt60}s)...")
        
        # Generate an RIR using PyTorch (faster FFT convolution)
        rir_len = int(sample_rate * rt60)
        t = torch.linspace(0, rt60, rir_len, dtype=torch.float32)
        decay = torch.exp(-6.91 * t / rt60)
        
        # Seed torch for reproducibility
        torch.manual_seed(seed)
        rir = torch.randn(rir_len, dtype=torch.float32) * decay
        rir = rir / rir.abs().max()
        
        dry_list, wet_list = [], []
        for i in range(n_sequences):
            clean_np = _make_signal(seq_len, sample_rate, rng)
            clean = torch.from_numpy(clean_np)
            
            # Fast FFT Convolution
            N = seq_len + rir_len - 1
            N_pow2 = 2 ** math.ceil(math.log2(N))
            
            clean_fft = torch.fft.rfft(clean, n=N_pow2)
            rir_fft = torch.fft.rfft(rir, n=N_pow2)
            wet = torch.fft.irfft(clean_fft * rir_fft, n=N_pow2)[:seq_len]
            
            # Mix dry and wet (50% mix)
            wet_mixed = (clean * 0.5) + (wet * 0.5)
            
            # Normalise
            dry_list.append(torch.tanh(clean))
            wet_list.append(torch.tanh(wet_mixed))

        self.dry = torch.stack(dry_list).unsqueeze(-1)
        self.wet = torch.stack(wet_list).unsqueeze(-1)

    def __len__(self) -> int:
        return len(self.dry)

    def __getitem__(self, idx: int) -> tuple[torch.Tensor, torch.Tensor]:
        return self.dry[idx], self.wet[idx]

# =============================================================================
# Loss
# =============================================================================

class SpectralLoss(nn.Module):
    def __init__(self, fft_sizes: list[int] | None = None, alpha: float = 0.7) -> None:
        super().__init__()
        self.fft_sizes = fft_sizes or [64, 128, 256, 512, 1024, 2048]
        self.alpha = alpha

    def forward(self, pred: torch.Tensor, target: torch.Tensor) -> torch.Tensor:
        p = pred.squeeze(-1)
        t = target.squeeze(-1)
        td_loss = torch.nn.functional.mse_loss(p, t)

        spec_loss = torch.zeros(1, device=pred.device)
        for n_fft in self.fft_sizes:
            if p.shape[-1] < n_fft:
                continue
            P = torch.fft.rfft(p,  n=n_fft)
            T = torch.fft.rfft(t, n=n_fft)
            spec_loss = spec_loss + torch.nn.functional.mse_loss(
                P.abs() / math.sqrt(n_fft),
                T.abs() / math.sqrt(n_fft),
            )
        spec_loss = spec_loss / len(self.fft_sizes)
        return (1.0 - self.alpha) * td_loss + self.alpha * spec_loss

# =============================================================================
# Training loop
# =============================================================================

def train(args: argparse.Namespace) -> None:
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    device = torch.device(args.device)
    print(f"Device         : {device}")

    model = SimpleMambaModel(
        d_model=1,
        n_layers=args.n_layers,
        d_state=args.d_state,
        d_conv=args.d_conv,
        expand=args.expand,
    ).to(device)
    total_params = sum(p.numel() for p in model.parameters())
    print(f"Model params   : {total_params:,}")

    print("\nBuilding dataset...")
    train_ds = ReverbDataset(
        n_sequences=args.n_train,
        seq_len=args.seq_len,
        sample_rate=args.sample_rate,
        rt60=args.rt60,
        seed=0,
    )
    val_ds = ReverbDataset(
        n_sequences=max(32, args.n_train // 10),
        seq_len=args.seq_len,
        sample_rate=args.sample_rate,
        rt60=args.rt60,
        seed=99,
    )

    train_loader = torch.utils.data.DataLoader(
        train_ds, batch_size=args.batch_size, shuffle=True,
        num_workers=0, pin_memory=(device.type == "cuda"),
    )
    val_loader = torch.utils.data.DataLoader(
        val_ds, batch_size=args.batch_size, shuffle=False, num_workers=0,
    )

    criterion = SpectralLoss(alpha=args.spectral_alpha)
    optimiser = optim.AdamW(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimiser, T_max=args.epochs, eta_min=args.lr * 0.01)

    best_val_loss = float("inf")
    best_ckpt = out_dir / "mamba_best.pt"
    final_ckpt = out_dir / "mamba_final.pt"

    print(f"\n{'Epoch':>6}  {'Train Loss':>12}  {'Val Loss':>12}  {'LR':>10}  {'Time':>8}")
    print("-" * 60)

    for epoch in range(1, args.epochs + 1):
        t0 = time.time()
        model.train()
        train_loss = 0.0

        for dry, wet in train_loader:
            dry, wet = dry.to(device), wet.to(device)
            optimiser.zero_grad(set_to_none=True)
            pred, _ = model(dry)
            loss = criterion(pred, wet)
            loss.backward()
            nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
            optimiser.step()
            train_loss += loss.item()

        train_loss /= len(train_loader)
        scheduler.step()

        model.eval()
        val_loss = 0.0
        with torch.no_grad():
            for dry, wet in val_loader:
                dry, wet = dry.to(device), wet.to(device)
                pred, _ = model(dry)
                val_loss += criterion(pred, wet).item()
        val_loss /= len(val_loader)

        elapsed = time.time() - t0
        lr_now = optimiser.param_groups[0]["lr"]

        marker = " ✅" if val_loss < best_val_loss else ""
        print(f"{epoch:6d}  {train_loss:12.6f}  {val_loss:12.6f}  {lr_now:10.2e}  {elapsed:7.1f}s{marker}")

        if val_loss < best_val_loss:
            best_val_loss = val_loss
            torch.save(model.state_dict(), best_ckpt)

    torch.save(model.state_dict(), final_ckpt)
    print(f"\nFinal checkpoint : {final_ckpt}")

    print("\nAuto-exporting best checkpoint to .mamba_weights...")
    _EXPORT_SCRIPT = _TOOLS_DIR / "export_weights.py"
    if _EXPORT_SCRIPT.exists():
        out_weights = out_dir / "mamba.mamba_weights"
        cmd = f'"{sys.executable}" "{_EXPORT_SCRIPT}" export --checkpoint "{best_ckpt}" --out "{out_weights}"'
        if os.system(cmd) == 0:
            print(f"  Exported → {out_weights}")
        else:
            print("  ⚠️  Export failed. Run manually.")

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Train Mamba-SSM for Context-Aware Reverb")
    p.add_argument("--n_layers",   type=int,   default=4)
    p.add_argument("--d_state",    type=int,   default=16)
    p.add_argument("--d_conv",     type=int,   default=4)
    p.add_argument("--expand",     type=int,   default=2)
    p.add_argument("--n_train",    type=int,   default=500)
    p.add_argument("--seq_len",    type=int,   default=4096)
    p.add_argument("--sample_rate",type=int,   default=44100)
    p.add_argument("--rt60",       type=float, default=1.5, help="Reverb RT60 decay time (seconds)")
    p.add_argument("--epochs",     type=int,   default=30)
    p.add_argument("--batch_size", type=int,   default=16)
    p.add_argument("--lr",         type=float, default=3e-3)
    p.add_argument("--weight_decay",type=float,default=1e-4)
    p.add_argument("--spectral_alpha", type=float, default=0.7, help="Weight of spectral loss")
    p.add_argument("--out_dir",    type=str,   default="resources/")
    p.add_argument("--device",     type=str,   default="cpu")
    return p.parse_args()

if __name__ == "__main__":
    train(parse_args())
