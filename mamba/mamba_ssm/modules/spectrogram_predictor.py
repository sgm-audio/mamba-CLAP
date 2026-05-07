import torch
import torch.nn as nn

try:
    from mamba_ssm import Mamba
    MAMBA_SSM_AVAILABLE = True
except ImportError:
    MAMBA_SSM_AVAILABLE = False
    Mamba = None


class MambaSpectrogramPredictor(nn.Module):
    def __init__(
        self,
        n_mels: int = 80,
        d_model: int = 512,
        n_layer: int = 8,
        d_state: int = 16,
        d_conv: int = 4,
        expand: int = 2,
    ):
        super().__init__()
        self.n_mels = n_mels

        self.input_proj = nn.Sequential(
            nn.Linear(n_mels, d_model),
            nn.GELU(),
            nn.LayerNorm(d_model),
        )

        if MAMBA_SSM_AVAILABLE:
            self.layers = nn.ModuleList(
                [
                    Mamba(
                        d_model=d_model,
                        d_state=d_state,
                        d_conv=d_conv,
                        expand=expand,
                    )
                    for _ in range(n_layer)
                ]
            )
        else:
            self.layers = nn.ModuleList(
                [nn.Linear(d_model, d_model) for _ in range(n_layer)]
            )

        self.output_proj = nn.Linear(d_model, n_mels)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        x = x.transpose(1, 2)

        hidden = self.input_proj(x)

        for layer in self.layers:
            hidden = layer(hidden)

        out = self.output_proj(hidden)

        out = out.transpose(1, 2)

        return out