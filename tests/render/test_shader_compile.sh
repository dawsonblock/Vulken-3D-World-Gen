#!/usr/bin/env bash
set -euo pipefail
scripts/compile_shaders.sh
# Simple validation: ensure we produced at least one .spv
count=$(find build/shaders -type f -name '*.spv' | wc -l | tr -d ' ')
if [ "$count" -lt 1 ]; then
  echo "No SPIR-V outputs produced"; exit 2
fi
echo "Shader compile test OK ($count files)."
