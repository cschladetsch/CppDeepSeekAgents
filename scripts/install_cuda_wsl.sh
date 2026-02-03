#!/usr/bin/env bash
set -euo pipefail

if ! grep -qiE "microsoft|wsl" /proc/version; then
  echo "This script is intended for WSL2." >&2
  exit 1
fi

if ! command -v apt-get >/dev/null 2>&1; then
  echo "apt-get not found. This script targets Ubuntu/Debian on WSL2." >&2
  exit 1
fi

sudo apt-get update
sudo apt-get install -y nvidia-cuda-toolkit

echo "Verifying CUDA in WSL2..."
if command -v nvidia-smi >/dev/null 2>&1; then
  nvidia-smi || true
else
  echo "nvidia-smi not found in WSL2. Ensure GPU support is enabled for WSL2." >&2
  exit 1
fi

if command -v nvcc >/dev/null 2>&1; then
  nvcc --version
else
  echo "nvcc not found. Ensure nvidia-cuda-toolkit installed correctly." >&2
  exit 1
fi
