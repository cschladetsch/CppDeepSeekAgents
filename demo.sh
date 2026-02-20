#!/usr/bin/env bash
set -euo pipefail

export CCACHE_DISABLE=1
export GGML_CCACHE=OFF
BIN="${BIN:-./build/CppDeepSeek}"
MEM="/tmp/agent_memory.json"

bold=$'\033[1m'
cyan=$'\033[36m'
yellow=$'\033[33m'
red=$'\033[31m'
reset=$'\033[0m'

echo "${bold}${cyan}=== Build + Test ===${reset}"
USE_CUDA=1
if [[ "${DEMO_NO_CUDA:-0}" == "1" ]]; then
  USE_CUDA=0
fi
if [[ "$(uname -s)" == "Darwin" ]]; then
  USE_CUDA=0
fi

if [[ "$USE_CUDA" -eq 1 ]]; then
  if grep -qiE "microsoft|wsl" /proc/version 2>/dev/null; then
    NVCC_PATH="$(command -v nvcc || true)"
    if [[ -z "$NVCC_PATH" || "$NVCC_PATH" == /mnt/c/* ]]; then
      echo "${red}CUDA is required but not ready in WSL2.${reset}"
      echo "${yellow}Install the CUDA toolkit inside WSL2, then re-run:${reset}"
      echo "  scripts/install_cuda_wsl.sh"
      exit 1
    fi
  fi
  ./b --tests --cuda
else
  ./b --tests --no-cuda
fi
ctest --test-dir build

DEMO_GPU_LAYERS="${DEMO_GPU_LAYERS:-0}"
GPU_LAYERS_ARG=()
if [[ "$DEMO_GPU_LAYERS" == "auto" ]]; then
  GPU_LAYERS_ARG=(--gpu-layers auto)
elif [[ "$DEMO_GPU_LAYERS" -gt 0 ]]; then
  GPU_LAYERS_ARG=(--gpu-layers "$DEMO_GPU_LAYERS")
fi

echo
echo "${bold}${cyan}=== Demo 1: Local-only, multi-round debate (no API key) ===${reset}"
"$BIN" ${GPU_LAYERS_ARG:+${GPU_LAYERS_ARG[@]}} --topic "Is C++ a good agent runtime?" --rounds 2 --no-stream

echo
echo "${bold}${cyan}=== Demo 2: Local-only with memory persistence ===${reset}"
"$BIN" ${GPU_LAYERS_ARG:+${GPU_LAYERS_ARG[@]}} --topic "Should agents write tests first in software engineering?" --rounds 1 --save "$MEM"
"$BIN" ${GPU_LAYERS_ARG:+${GPU_LAYERS_ARG[@]}} --topic "Continue the software engineering debate about agent testing." --rounds 1 --load "$MEM" --save "$MEM"

echo
echo "${bold}${cyan}=== Demo 3: Remote mode (optional) ===${reset}"
if [[ -z "${DEEPSEEK_API_KEY:-}" ]]; then
  echo "${yellow}Skipping remote demo: set DEEPSEEK_API_KEY to enable.${reset}"
else
  "$BIN" --remote --topic "Is C++ the future of AI agents?" --rounds 1 --no-stream
fi

echo
echo "${bold}${cyan}=== Demo 4: Logic gate rejection (constraint enforcement) ===${reset}"
if "$BIN" --topic "Tell me about cooking recipes." --rounds 1 --no-stream; then
  echo "${red}Unexpected: gate did not reject.${reset}"
else
  echo "${yellow}Gate correctly rejected the topic.${reset}"
fi
