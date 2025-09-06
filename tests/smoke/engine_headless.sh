#!/usr/bin/env bash
set -euo pipefail
# Adjust to your actual binary name/flags:
BIN="./build/apps/vulkan_fullscreen_demo"
if [ ! -x "$BIN" ]; then
  echo "Engine binary not found at $BIN"; exit 3
fi
"$BIN" --headless --frames 10
echo "Headless smoke test OK."
