#!/usr/bin/env bash
set -euo pipefail

# Default Vulkan pipeline cache path if not provided
: "${VK_PIPELINE_CACHE_PATH:=$(cd "$(dirname "$0")/.." && pwd)/.cache/vk_pipeline_cache/voxelvk_pipelines.cache}"
mkdir -p "$(dirname "$VK_PIPELINE_CACHE_PATH")"
export VK_PIPELINE_CACHE_PATH

# Try likely GUI app names
CANDIDATES=(
  "./build/apps/VoxelVK_Elite_ALL"
  "./build/apps/vulkan_fullscreen_demo"
  "./build/apps/voxelvk_gui"
)
for exe in "${CANDIDATES[@]}"; do
  if [ -x "$exe" ]; then
    echo "[run_gui] Using $exe with VK_PIPELINE_CACHE_PATH=$VK_PIPELINE_CACHE_PATH"
    exec "$exe"
  fi
done

echo "[run_gui] GUI executable not found in expected locations."
exit 1
