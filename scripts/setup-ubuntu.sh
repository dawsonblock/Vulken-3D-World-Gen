#!/usr/bin/env bash
set -euo pipefail

sudo apt-get update
sudo apt-get install -y \
  build-essential cmake pkg-config \
  libglfw3-dev libvulkan-dev vulkan-validationlayers-dev \
  python3 python3-venv python3-pip

# Optional shader tools if you later add shaders
sudo apt-get install -y glslang-tools shaderc

# Python deps
python3 -m pip install --upgrade pip
python3 -m pip install -r "$(dirname "$0")/../python/requirements.txt" || true

echo "Done. You can now run ./scripts/build.sh"
