#!/usr/bin/env bash
set -euo pipefail
# Compile only Vulkan-ready GLSL (shaders_vk) into SPIR-V into .cache/spv using glslc if found, else fallback to glslangValidator.

OUT_DIR="${OUT_DIR:-.cache/spv}"
GLSLC_BIN="${GLSLC:-}"
GLSLANG_BIN="${GLSLANG_VALIDATOR:-}"

mkdir -p "$OUT_DIR"

# Resolve compiler
if [ -z "${GLSLC_BIN}" ]; then
  if command -v glslc >/dev/null 2>&1; then
    GLSLC_BIN=$(command -v glslc)
  fi
fi
if [ -z "${GLSLANG_BIN}" ]; then
  if command -v glslangValidator >/dev/null 2>&1; then
    GLSLANG_BIN=$(command -v glslangValidator)
  fi
fi

if [ -z "${GLSLC_BIN}" ] && [ -z "${GLSLANG_BIN}" ]; then
  echo "[shaders] No shader compiler found (glslc/glslangValidator). Install Vulkan SDK."
  exit 2
fi

compile_one() {
  local src="$1"; local out="$2"
  mkdir -p "$(dirname "$out")"
  if [ -n "$GLSLC_BIN" ]; then
    echo "[glslc] $src -> $out"
    "$GLSLC_BIN" --target-env=vulkan1.3 -O -g "$src" -o "$out"
  else
    echo "[glslangValidator] $src -> $out"
    "$GLSLANG_BIN" -V --target-env vulkan1.3 -g "$src" -o "$out"
  fi
}

# Find GLSL sources in common directories, including shaders_vk
shopt -s nullglob globstar
found=0
for dir in shaders_vk; do
  if [ -d "$dir" ]; then
    for f in "$dir"/**/*.{vert.glsl,frag.glsl,comp.glsl,vert,frag,comp}; do
      [ -e "$f" ] || continue
      base=$(basename "$f")
      rel=$(dirname "${f#${dir}/}")
      out="$OUT_DIR/${dir//\//_}/$rel/${base}.spv"
      compile_one "$f" "$out"
      found=$((found+1))
    done
  fi
done

echo "[shaders] compiled: $found file(s) into $OUT_DIR"