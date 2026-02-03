#!/usr/bin/env bash
set -euo pipefail

WITH_DEPS=0
WITH_MODELS=0
WITH_TESTS=0
FORCE_DEPS=0
WITH_CUDA=1
RUN_DEMO=0

for arg in "$@"; do
  case "$arg" in
    --deps) WITH_DEPS=1 ;;
    --force-deps) FORCE_DEPS=1 ;;
    --models) WITH_MODELS=1 ;;
    --tests) WITH_TESTS=1 ;;
    --cuda) WITH_CUDA=1 ;;
    --no-cuda) WITH_CUDA=0 ;;
    --demo) RUN_DEMO=1 ;;
    *)
      echo "Unknown option: $arg" >&2
      echo "Usage: ./b [--deps] [--force-deps] [--models] [--tests] [--cuda|--no-cuda] [--demo]" >&2
      exit 1
      ;;
  esac
done

if [[ "$WITH_DEPS" -eq 1 ]]; then
  if [[ "$FORCE_DEPS" -eq 1 ]]; then
    CPPDEEPSEEK_FORCE_DEPS=1 bash scripts/install_deps.sh
  else
    bash scripts/install_deps.sh
  fi
fi

DEFAULT_MODEL_URL="https://huggingface.co/bartowski/DeepSeek-R1-Distill-Qwen-14B-GGUF/resolve/main/DeepSeek-R1-Distill-Qwen-14B-Q4_K_M.gguf"
MODEL_PATH="${DEEPSEEK_MODEL_HOME:-$HOME/.local/share/deepseek/models}/deepseek-r1/model.gguf"
if [[ ! -f "$MODEL_PATH" && "${DEEPSEEK_MODEL_SKIP:-0}" != "1" ]]; then
  MODEL_URL="${DEEPSEEK_MODEL_URL:-$DEFAULT_MODEL_URL}"
  mkdir -p "$(dirname "$MODEL_PATH")"
  echo "Downloading default model (Q4_K_M, ~9GB) to $MODEL_PATH"
  echo "Source: $MODEL_URL"
  curl -L "$MODEL_URL" -o "$MODEL_PATH"
fi

LLAMA_ARG=()
if [[ -n "${LLAMA_CPP_DIR:-}" ]]; then
  LLAMA_ARG=(-DLLAMA_CPP_DIR="${LLAMA_CPP_DIR}")
fi

CPP_TESTS=OFF
MODEL_TESTS=OFF
FETCHCONTENT=OFF
if [[ "$WITH_TESTS" -eq 1 ]]; then
  CPP_TESTS=ON
  MODEL_TESTS=ON
  FETCHCONTENT=ON
fi

CUDA_ARGS=()
if [[ "$WITH_CUDA" -eq 1 ]]; then
  if command -v nvidia-smi >/dev/null 2>&1; then
    CUDA_ARGS=(-DGGML_CUDA=ON)
    if grep -qiE "microsoft|wsl" /proc/version 2>/dev/null; then
      NVCC_PATH="$(command -v nvcc || true)"
      if [[ -n "$NVCC_PATH" && "$NVCC_PATH" != /mnt/c/* ]]; then
        CUDA_ARGS+=(-DCMAKE_CUDA_COMPILER="${NVCC_PATH}" -DCUDAToolkit_ROOT="/usr")
        if [[ -f build/CMakeCache.txt ]] && rg -q "/mnt/c/Program Files|LLAMA_CUDA" build/CMakeCache.txt; then
          echo "Clearing stale CMake cache (Windows CUDA paths detected)."
          rm -f build/CMakeCache.txt
          rm -rf build/CMakeFiles
        fi
      else
        echo "CUDA is required but not installed in WSL2."
        echo "Install CUDA inside WSL2 and retry: scripts/install_cuda_wsl.sh" >&2
        exit 1
      fi
    else
      if [[ -n "${CUDA_HOME:-}" ]]; then
        CUDA_ARGS+=(-DCUDAToolkit_ROOT="${CUDA_HOME}")
      fi
    fi
  else
    echo "CUDA enabled by default but GPU not detected (nvidia-smi missing)." >&2
    exit 1
  fi
fi

export CCACHE_DISABLE=1
export GGML_CCACHE=OFF

cmake -S . -B build \
  -DCPPDEEPSEEK_BUILD_TESTS=${CPP_TESTS} \
  -DCPPDEEPSEEK_ALLOW_FETCHCONTENT=${FETCHCONTENT} \
  -DMODELSTORE_BUILD_TESTS=${MODEL_TESTS} \
  -DMODELSTORE_ALLOW_FETCHCONTENT=${FETCHCONTENT} \
  -DCPPDEEPSEEK_ALLOW_FETCHCONTENT_LLAMA=ON \
  "${LLAMA_ARG[@]}" \
  "${CUDA_ARGS[@]}"

cmake --build build

if [[ "$WITH_MODELS" -eq 1 ]]; then
  cmake --build build --target ensure_models
fi

if [[ "$RUN_DEMO" -eq 1 ]]; then
  bash ./demo.sh
fi
