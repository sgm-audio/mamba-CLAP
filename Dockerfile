# Dockerfile for Mamba repository
# Provides a consistent development environment with all dependencies

FROM nvidia/cuda:11.8.0-cudnn8-runtime-ubuntu22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

# Install system dependencies
RUN apt-get update && apt-get install -y \
    python3-pip \
    python3-dev \
    git \
    build-essential \
    cmake \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Install Python dependencies
RUN pip3 install --no-cache-dir \
    torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu118 \
    mamba-ssm \
    causal-conv1d>=1.4.0 \
    numpy \
    einops \
    packaging \
    ninja \
    transformers \
    tilelang==0.1.8 \
    apache-tvm-ffi<=0.1.9 \
    quack-kernels>=0.3.4

# Install development tools
RUN pip3 install --no-cache-dir \
    pytest \
    black \
    ruff \
    mypy \
    pre-commit

# Install DSP system dependencies
RUN pip3 install --no-cache-dir \
    ray \
    hypothesis \
    langsmith \
    neo4j

# Set working directory
WORKDIR /workspace

# Copy repository files
COPY . .

# Install the package in development mode
RUN pip3 install -e .

# Set up pre-commit hooks
RUN pre-commit install

# Default command
CMD ["bash"]
