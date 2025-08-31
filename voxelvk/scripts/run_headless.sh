#!/usr/bin/env bash
set -euo pipefail

# Default Vulkan pipeline cache path if not provided
: "${VK_PIPELINE_CACHE_PATH:=$(cd "$(dirname "$0")/.." && pwd)/.cache/vk_pipeline_cache/voxelvk_pipelines.cache}"
mkdir -p "$(dirname "$VK_PIPELINE_CACHE_PATH")"
export VK_PIPELINE_CACHE_PATH

# Try multiple candidate paths for the headless executable
CANDIDATES=(
  "./build/smoke_headless"
  "./build/apps/smoke_headless"
  "./build/apps/smoke_graphics_headless"
)

EXE=""
for c in "${CANDIDATES[@]}"; do
  if [ -x "$c" ]; then EXE="$c"; break; fi
done

if [ -n "$EXE" ]; then
  echo "[run_headless] Using VK_PIPELINE_CACHE_PATH=$VK_PIPELINE_CACHE_PATH"
  echo "[run_headless] Executable: $EXE"
  exec "$EXE"
else
  echo "[run_headless] No headless executable found in candidates: ${CANDIDATES[*]}"
  exit 1
fi