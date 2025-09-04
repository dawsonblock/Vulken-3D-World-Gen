#!/usr/bin/env bash
set -euo pipefail

# Default Vulkan pipeline cache path if not provided
: "${VK_PIPELINE_CACHE_PATH:=$(cd "$(dirname "$0")/.." && pwd)/.cache/vk_pipeline_cache/voxelvk_pipelines.cache}"
mkdir -p "$(dirname "$VK_PIPELINE_CACHE_PATH")"
export VK_PIPELINE_CACHE_PATH

EXE="./build/apps/smoke_headless"
if [ ! -x "$EXE" ]; then
  # Alternate demo target name fallback
  EXE="./build/apps/smoke_graphics_headless"
fi

if [ -x "$EXE" ]; then
  echo "[run_headless] Using VK_PIPELINE_CACHE_PATH=$VK_PIPELINE_CACHE_PATH"
  exec "$EXE"
else:
  echo "[run_headless] No headless executable found at $EXE"
  exit 1
fi
