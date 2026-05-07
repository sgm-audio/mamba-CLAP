"""Crest Factor Loss for dynamic range preservation."""

import torch
import torch.nn as nn
import torch.nn.functional as F


class CrestFactorLoss(nn.Module):
    """
    Crest Factor Loss to prevent over-compression.
    
    The crest factor is the ratio of peak to RMS amplitude.
    This loss penalizes predictions that have lower crest factor
    than the target, encouraging preservation of dynamic range.
    
    Args:
        eps: Small constant for numerical stability (default: 1e-8)
    """
    
    def __init__(self, eps: float = 1e-8):
        super().__init__()
        self.eps = eps
    
    def _compute_crest_factor(
        self,
        x: torch.Tensor,
        dim: int = -1,
    ) -> torch.Tensor:
        """
        Compute crest factor (peak / RMS) along specified dimension.
        
        Args:
            x: Input tensor
            dim: Dimension to reduce over
        
        Returns:
            Crest factor tensor
        """
        peak = x.abs().max(dim=dim, keepdim=True).values
        rms = x.pow(2).mean(dim=dim, keepdim=True).sqrt()
        return peak / (rms + self.eps)
    
    def forward(self, pred: torch.Tensor, target: torch.Tensor) -> torch.Tensor:
        """
        Compute crest factor loss.
        
        Only penalizes if prediction has LOWER crest factor than target
        (i.e., over-compressed). Does not penalize higher crest factor.
        
        Args:
            pred: Predicted audio (B, T_samples)
            target: Target audio (B, T_samples)
        
        Returns:
            Scalar loss value
        """
        pred_cf = self._compute_crest_factor(pred)
        target_cf = self._compute_crest_factor(target)
        
        # Penalize only if pred_cf < target_cf (over-compression)
        loss = F.relu(target_cf - pred_cf)
        
        return loss.mean()
