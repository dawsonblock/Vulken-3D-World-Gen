#!/usr/bin/env bash
set -euo pipefail

# Default Vulkan pipeline cache path if not provided
: "${VK_PIPELINE_CACHE_PATH:=$(cd "$(dirname "$0")/.." && pwd)/.cache/vk_pipeline_cache/voxelvk_pipelines.cache}"
mkdir -p "$(dirname "$VK_PIPELINE_CACHE_PATH")"
export VK_PIPELINE_CACHE_PATH

# Optional extra args to pass to the GUI executable (e.g. --smoke)
GUI_ARGS=${GUI_ARGS:-}

# Optional override for the exact GUI executable to run
GUI_EXEC=${GUI_EXEC:-}

# Try likely GUI app names (both in build/ and build/apps/), prioritizing actual GUI windows first
CANDIDATES=(
  "./build/gui_fullscreen_demo"
  "./build/main_imgui_vulkan"
  "./build/x11_basic_demo"
  "./build/apps/vulkan_fullscreen_demo" "./build/vulkan_fullscreen_demo"
  "./build/apps/voxelvk_gui" "./build/voxelvk_gui"
  "./build/apps/VoxelVK_Elite_ALL" "./build/VoxelVK_Elite_ALL"
)

# Determine if we should run under a virtual X display
USE_XVFB=0
if [ -z "${DISPLAY:-}" ]; then
  if command -v xvfb-run >/dev/null 2>&1; then
    USE_XVFB=1
  fi
fi

if [ -n "$GUI_EXEC" ]; then
  if [ -x "$GUI_EXEC" ]; then
    CANDIDATES=("$GUI_EXEC")
  else
    # Try resolving common build locations
    if [ -x "./build/$GUI_EXEC" ]; then
      CANDIDATES=("./build/$GUI_EXEC")
    elif [ -x "./build/apps/$GUI_EXEC" ]; then
      CANDIDATES=("./build/apps/$GUI_EXEC")
    else
      echo "[run_gui] GUI_EXEC is set but not found/executable: $GUI_EXEC" >&2
      echo "[run_gui] Tried: $GUI_EXEC, ./build/$GUI_EXEC, ./build/apps/$GUI_EXEC" >&2
      exit 2
    fi
  fi
fi

for exe in "${CANDIDATES[@]}"; do
  if [ -x "$exe" ]; then
    if [ "$USE_XVFB" -eq 1 ]; then
      echo "[run_gui] Using $exe under xvfb-run (headless) with VK_PIPELINE_CACHE_PATH=$VK_PIPELINE_CACHE_PATH"
      exec xvfb-run -a -s "-screen 0 1280x720x24" "$exe" $GUI_ARGS
    else
      echo "[run_gui] Using $exe with VK_PIPELINE_CACHE_PATH=$VK_PIPELINE_CACHE_PATH"
      exec "$exe" $GUI_ARGS
    fi
  fi
done

echo "[run_gui] GUI executable not found in expected locations: ${CANDIDATES[*]}"
exit 1
