#!/usr/bin/env bash
set -euo pipefail

if ! command -v apt-get >/dev/null 2>&1; then
  echo "This script supports Debian/Ubuntu via apt-get." >&2
  echo "Install libcurl dev packages for your distro, then re-run CMake." >&2
  exit 1
fi

STAMP="${XDG_CACHE_HOME:-$HOME/.cache}/cppdeepseek_deps_installed"
if [[ "${CPPDEEPSEEK_FORCE_DEPS:-0}" != "1" && -f "$STAMP" ]]; then
  echo "Dependencies already installed (stamp: $STAMP)."
else
  sudo apt-get update
  sudo apt-get install -y build-essential cmake ninja-build libcurl4-openssl-dev nlohmann-json3-dev
  mkdir -p "$(dirname "$STAMP")"
  touch "$STAMP"
fi

if grep -qiE "microsoft|wsl" /proc/version; then
  if [[ "${CPPDEEPSEEK_SKIP_WSL_CUDA:-0}" != "1" ]]; then
    echo "WSL2 detected. Installing CUDA toolkit for WSL2..."
    if ! bash scripts/install_cuda_wsl.sh; then
      echo "CUDA setup failed in WSL2. Fix the issue and re-run ./b --deps." >&2
      exit 1
    fi
  fi
fi

MODEL_PATH="${DEEPSEEK_MODEL_HOME:-$HOME/.local/share/deepseek/models}/deepseek-r1/model.gguf"
MODEL_DIR="$(dirname "$MODEL_PATH")"
DEFAULT_MODEL_URL="https://huggingface.co/bartowski/DeepSeek-R1-Distill-Qwen-14B-GGUF/resolve/main/DeepSeek-R1-Distill-Qwen-14B-Q4_K_M.gguf"

if [[ ! -f "$MODEL_PATH" ]]; then
  echo "Local model not found at: $MODEL_PATH"
  mkdir -p "$MODEL_DIR"
  if [[ "${DEEPSEEK_MODEL_SKIP:-0}" == "1" ]]; then
    echo "Skipping model download. Set DEEPSEEK_MODEL_URL or place your GGUF at:"
    echo "  $MODEL_PATH"
  else
    MODEL_URL="${DEEPSEEK_MODEL_URL:-$DEFAULT_MODEL_URL}"
    echo "Downloading default model (Q4_K_M, ~9GB)."
    echo "Source: $MODEL_URL"
    curl -L "$MODEL_URL" -o "$MODEL_PATH"
  fi
fi
