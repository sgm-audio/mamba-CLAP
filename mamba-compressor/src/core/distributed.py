"""Distributed training utilities for multi-GPU setups."""

import os
import torch
import torch.distributed as dist
from typing import Optional


def init_distributed(rank: Optional[int] = None, world_size: Optional[int] = None) -> bool:
    """
    Initialize distributed training environment.
    
    Args:
        rank: Process rank (optional, auto-detected from env vars)
        world_size: Total number of processes (optional, auto-detected)
    
    Returns:
        True if distributed training is initialized, False otherwise
    """
    # Auto-detect from environment variables (torchrun, SLURM, etc.)
    if rank is None:
        rank = int(os.environ.get("RANK", 0))
    if world_size is None:
        world_size = int(os.environ.get("WORLD_SIZE", 1))
    
    # Check if distributed training is requested
    if world_size == 1:
        return False
    
    # Initialize process group
    if not dist.is_initialized():
        dist.init_process_group(
            backend="nccl",
            init_method="env://",
            world_size=world_size,
            rank=rank,
        )
        torch.cuda.set_device(rank % torch.cuda.device_count())
    
    return True


def is_main_process() -> bool:
    """Check if current process is the main process (rank 0)."""
    if not dist.is_initialized():
        return True
    return dist.get_rank() == 0


def get_rank() -> int:
    """Get current process rank."""
    if not dist.is_initialized():
        return 0
    return dist.get_rank()


def get_world_size() -> int:
    """Get total number of processes."""
    if not dist.is_initialized():
        return 1
    return dist.get_world_size()


def barrier() -> None:
    """Synchronize all processes."""
    if dist.is_initialized():
        dist.barrier()


def cleanup() -> None:
    """Clean up distributed training environment."""
    if dist.is_initialized():
        dist.destroy_process_group()


def all_reduce(tensor: torch.Tensor, op: str = "sum") -> torch.Tensor:
    """
    Perform all-reduce operation across all processes.
    
    Args:
        tensor: Tensor to reduce
        op: Reduction operation ("sum", "avg", "min", "max")
    
    Returns:
        Reduced tensor (in-place modification)
    """
    if not dist.is_initialized():
        return tensor
    
    op_map = {
        "sum": dist.ReduceOp.SUM,
        "avg": dist.ReduceOp.AVG,
        "min": dist.ReduceOp.MIN,
        "max": dist.ReduceOp.MAX,
    }
    
    dist.all_reduce(tensor, op=op_map[op])
    
    if op == "avg":
        tensor /= get_world_size()
    
    return tensor
