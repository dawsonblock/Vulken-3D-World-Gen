#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)/..
BUILD_DIR="${ROOT_DIR}/build_ci"

mkdir -p "$BUILD_DIR"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE="${ROOT_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build "$BUILD_DIR" --target gui_fullscreen_demo main_imgui_fullscreen vulkan_fullscreen_demo --parallel

echo "[CI] Demo builds completed successfully."