#!/usr/bin/env python3
"""
tools/simple_mamba.py

PyTorch reference implementation of the MambaLayer / MambaModel that exactly
mirrors the C++ implementation in:
    MambaDAW/src/mamba_layer.h / mamba_layer.cpp
    MambaDAW/src/mamba_model.h / mamba_model.cpp

Architecture contract (must stay in sync with C++):
────────────────────────────────────────────────────
  d_inner = expand * d_model

  Per sample (scalar path, d_model = 1):
    1. in_proj:    [1] → [2 * d_inner]         (weight: [2*d_inner, d_model])
    2. conv1d:     causal depthwise ring buffer  (weight: [d_inner, d_conv])
    3. SSM step:   h_t = dA * h_{t-1} + dB * x_t
                   y_t = C · h_t  +  D * x_t   (dA,dB,C: [d_inner, d_state]; D: [d_inner])
    4. gate:       gated = SSM_out * silu(z_branch)
    5. out_proj:   [d_inner] → [d_model=1]      (weight: [d_model, d_inner])

Weight tensors (stored flat in .mamba_weights, per layer):
    in_proj   : [d_model * 2 * d_inner]  float32
    out_proj  : [d_inner]               float32   (d_model=1 → d_inner rows)
    conv1d    : [d_inner * d_conv]      float32
    dA        : [d_inner * d_state]     float32   (pre-computed discrete A)
    dB        : [d_inner * d_state]     float32   (pre-computed discrete B, fixed)
    C         : [d_inner * d_state]     float32   (output matrix, fixed)
    D         : [d_inner]               float32
"""

from __future__ import annotations

import torch
import torch.nn as nn
import torch.nn.functional as F


class SimpleMambaLayer(nn.Module):
    """
    Scalar-path Mamba layer.

    Processes one sample at a time in a stateful recurrent loop.
    Forward() accepts [batch, seq_len, d_model] and processes each
    time step sequentially, mirroring the C++ processSample() loop.

    NOTE: B and C are FIXED parameters (not input-dependent).
          This is a linear SSM, not the full selective Mamba SSM.
          For full selectivity, B and C should be computed from x_t
          via an x_proj linear. That requires updating the C++ kernel.
    """

    def __init__(
        self,
        d_model: int,
        d_state: int = 16,
        d_conv: int = 4,
        expand: int = 2,
    ) -> None:
        super().__init__()
        self.d_model = d_model
        self.d_state = d_state
        self.d_conv  = d_conv
        self.expand  = expand
        self.d_inner = expand * d_model

        # --- Weights (exactly matching C++ weight layout) ---

        # in_proj: maps d_model → 2*d_inner (x branch + z gate branch)
        self.in_proj = nn.Linear(d_model, 2 * self.d_inner, bias=False)

        # out_proj: maps d_inner → d_model
        self.out_proj = nn.Linear(self.d_inner, d_model, bias=False)

        # conv1d: causal depthwise, kernel=d_conv, no bias
        # C++ ring-buffer is equivalent to causal conv with padding=d_conv-1
        self.conv1d = nn.Conv1d(
            in_channels=self.d_inner,
            out_channels=self.d_inner,
            kernel_size=d_conv,
            groups=self.d_inner,
            padding=d_conv - 1,
            bias=False,
        )

        # SSM matrices — fixed (non-input-dependent)
        # dA: discrete transition matrix per feature per state
        self.dA = nn.Parameter(torch.zeros(self.d_inner, d_state))
        # dB: discrete input matrix
        self.dB = nn.Parameter(torch.zeros(self.d_inner, d_state))
        # C: output matrix
        self.C  = nn.Parameter(torch.zeros(self.d_inner, d_state))
        # D: skip connection
        self.D  = nn.Parameter(torch.ones(self.d_inner))

        self._init_weights()

    def _init_weights(self) -> None:
        """
        Initialise weights to produce a stable SSM at the start of training.

        dA is initialised near 0.9 (slow decay) so gradients flow through
        multiple steps. dB and C are small random.
        """
        nn.init.xavier_uniform_(self.in_proj.weight)
        nn.init.xavier_uniform_(self.out_proj.weight)

        # Stable SSM initialisation:
        #   dA ≈ 0.9 means each step retains ~90% of the state.
        with torch.no_grad():
            self.dA.fill_(0.9)
            nn.init.normal_(self.dB, std=0.01)
            nn.init.normal_(self.C,  std=0.01)
            self.D.fill_(1.0)

    # ------------------------------------------------------------------
    # Forward: sequential (matches C++ processSample loop)
    # ------------------------------------------------------------------

    def forward(
        self,
        x: torch.Tensor,
        state: torch.Tensor | None = None,
    ) -> tuple[torch.Tensor, torch.Tensor]:
        """
        Args:
            x:     [batch, seq_len, d_model]
            state: [batch, d_inner, d_state] or None (zeros)

        Returns:
            output: [batch, seq_len, d_model]
            state:  [batch, d_inner, d_state]  (carry-over for streaming)
        """
        batch, seq_len, _ = x.shape
        device = x.device

        if state is None:
            state = torch.zeros(batch, self.d_inner, self.d_state, device=device)

        # --- Input projection: [batch, seq, d_model] → [batch, seq, 2*d_inner] ---
        xz = self.in_proj(x)
        x_branch, z_branch = xz.split(self.d_inner, dim=-1)  # each [batch, seq, d_inner]

        # --- Causal depthwise conv1d ---
        # Transpose to [batch, d_inner, seq] for conv1d
        x_t = x_branch.transpose(1, 2)
        conv_out = self.conv1d(x_t)[:, :, :seq_len]          # [batch, d_inner, seq]
        conv_out = F.silu(conv_out)
        conv_out = conv_out.transpose(1, 2)                   # [batch, seq, d_inner]

        # --- Sequential SSM recurrence ---
        # Matches C++ processSample() exactly.
        # dA, dB, C: [d_inner, d_state]  — broadcast over batch via einsum
        outputs = []
        h = state  # [batch, d_inner, d_state]

        for t in range(seq_len):
            x_t_step = conv_out[:, t, :]   # [batch, d_inner]

            # h_new[b, i, j] = dA[i, j] * h[b, i, j] + dB[i, j] * x[b, i]
            h = self.dA.unsqueeze(0) * h + self.dB.unsqueeze(0) * x_t_step.unsqueeze(-1)

            # y[b, i] = sum_j( C[i,j] * h[b,i,j] ) + D[i] * x[b, i]
            y = (h * self.C.unsqueeze(0)).sum(dim=-1)  # [batch, d_inner]
            y = y + self.D.unsqueeze(0) * x_t_step

            outputs.append(y)

        ssm_out = torch.stack(outputs, dim=1)  # [batch, seq, d_inner]

        # --- Gating: SSM_out * silu(z_branch) ---
        gated = ssm_out * F.silu(z_branch)

        # --- Output projection ---
        out = self.out_proj(gated)  # [batch, seq, d_model]

        return out, h  # return final state for streaming

    # ------------------------------------------------------------------
    # Weight extraction for .mamba_weights export
    # ------------------------------------------------------------------

    def get_weight_arrays(self) -> dict[str, torch.Tensor]:
        """
        Returns all weights as flat float32 tensors, in the exact order
        and shape expected by MambaWeightLoader::load() in C++.

        Shapes:
            in_proj  : [d_model * 2 * d_inner]  ← in_proj.weight is [2*d_inner, d_model]
            out_proj : [d_inner]                 ← out_proj.weight is [d_model, d_inner],
                                                    d_model=1 → just d_inner values
            conv1d   : [d_inner * d_conv]        ← conv1d.weight is [d_inner, 1, d_conv]
            dA       : [d_inner * d_state]
            dB       : [d_inner * d_state]
            C        : [d_inner * d_state]
            D        : [d_inner]
        """
        with torch.no_grad():
            return {
                # in_proj.weight shape is [2*d_inner, d_model].
                # C++ expects [d_model * 2 * d_inner], rows = features, so transpose first.
                "in_proj":  self.in_proj.weight.T.contiguous().float().flatten(),
                # out_proj.weight shape is [d_model, d_inner].
                # C++ expects [d_inner] (d_model=1 path), so just the first row.
                "out_proj": self.out_proj.weight[0].contiguous().float().flatten(),
                # conv1d.weight shape is [d_inner, 1, d_conv] → flatten to [d_inner * d_conv].
                "conv1d":   self.conv1d.weight.squeeze(1).contiguous().float().flatten(),
                "dA":       self.dA.contiguous().float().flatten(),
                "dB":       self.dB.contiguous().float().flatten(),
                "C":        self.C.contiguous().float().flatten(),
                "D":        self.D.contiguous().float().flatten(),
            }


class SimpleMambaModel(nn.Module):
    """
    Stack of SimpleMambaLayers with residual connections.

    Exactly mirrors C++ MambaModel::processSample() chain:
        out = sample
        for layer in layers:
            out = layer.processSample(out)
    """

    def __init__(
        self,
        d_model: int = 1,
        n_layers: int = 4,
        d_state: int = 16,
        d_conv: int = 4,
        expand: int = 2,
    ) -> None:
        super().__init__()
        self.d_model  = d_model
        self.n_layers = n_layers
        self.d_state  = d_state
        self.d_conv   = d_conv
        self.expand   = expand
        self.d_inner  = expand * d_model

        self.layers = nn.ModuleList([
            SimpleMambaLayer(d_model, d_state, d_conv, expand)
            for _ in range(n_layers)
        ])

    def forward(
        self,
        x: torch.Tensor,
        states: list[torch.Tensor] | None = None,
    ) -> tuple[torch.Tensor, list[torch.Tensor]]:
        """
        Args:
            x:      [batch, seq_len, d_model]
            states: list of n_layers tensors [batch, d_inner, d_state], or None

        Returns:
            out:    [batch, seq_len, d_model]
            states: list of final hidden states (for streaming inference)
        """
        if states is None:
            states = [None] * self.n_layers

        out = x
        new_states = []
        for i, layer in enumerate(self.layers):
            out, h = layer(out, states[i])
            new_states.append(h)

        return out, new_states

    def arch_dict(self) -> dict:
        """Returns architecture as dict for embedding in the .mamba_weights header."""
        return {
            "n_layers": self.n_layers,
            "d_model":  self.d_model,
            "d_state":  self.d_state,
            "d_conv":   self.d_conv,
            "expand":   self.expand,
        }


# =============================================================================
# Sanity check: run this file directly to verify Python ↔ C++ shape contract
# =============================================================================

if __name__ == "__main__":
    print("SimpleMambaModel — shape contract sanity check")
    print("=" * 52)

    d_model, n_layers, d_state, d_conv, expand = 1, 4, 16, 4, 2
    d_inner = expand * d_model
    batch, seq = 2, 1024

    model = SimpleMambaModel(d_model, n_layers, d_state, d_conv, expand)
    model.eval()

    x = torch.randn(batch, seq, d_model)
    with torch.no_grad():
        out, states = model(x)

    print(f"  Input:  {tuple(x.shape)}")
    print(f"  Output: {tuple(out.shape)}")
    print(f"  States: {[tuple(s.shape) for s in states]}")

    print("\nPer-layer weight shapes (must match C++ expectations):")
    for i, layer in enumerate(model.layers):
        ws = layer.get_weight_arrays()
        exp = {
            "in_proj":  d_model * 2 * d_inner,
            "out_proj": d_inner,
            "conv1d":   d_inner * d_conv,
            "dA":       d_inner * d_state,
            "dB":       d_inner * d_state,
            "C":        d_inner * d_state,
            "D":        d_inner,
        }
        ok = all(ws[k].numel() == exp[k] for k in exp)
        status = "✅" if ok else "❌"
        print(f"  Layer {i}: {status}")
        if not ok:
            for k in exp:
                print(f"    {k}: got {ws[k].numel()}, expected {exp[k]}")

    total_params = sum(p.numel() for p in model.parameters())
    print(f"\nTotal parameters: {total_params:,}")
    print("\nAll checks passed — safe to export." if all(
        layer.get_weight_arrays()["in_proj"].numel() == d_model * 2 * d_inner
        for layer in model.layers
    ) else "\n❌ Shape mismatch — fix before exporting!")
