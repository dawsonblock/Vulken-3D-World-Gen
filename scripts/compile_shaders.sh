#!/usr/bin/env bash
set -euo pipefail
# Compile all GLSL shaders into SPIR-V into .cache/spv using glslc if found.

OUT_DIR="${OUT_DIR:-.cache/spv}"
GLSLC="${GLSLC:-glslc}"

mkdir -p "$OUT_DIR"

# Find GLSL sources heuristically
shopt -s nullglob globstar
found=0
for dir in shaders src/shaders assets/shaders; do
  if [ -d "$dir" ]; then
    for f in "$dir"/**/*.{vert,frag,comp,glsl}; do
      [ -e "$f" ] || continue
      base=$(basename "$f")
      out="$OUT_DIR/${base}.spv"
      echo "[glslc] $f -> $out"
      "$GLSLC" -O "$f" -o "$out"
      found=$((found+1))
    done
  fi
done

echo "[glslc] compiled: $found file(s)"
