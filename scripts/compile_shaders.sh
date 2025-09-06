#!/usr/bin/env bash
set -euo pipefail

: "${GLSLC:=glslc}"

if ! command -v "$GLSLC" >/dev/null 2>&1; then
  echo "ERROR: glslc not found. Set GLSLC=/path/to/glslc or install Vulkan SDK."
  exit 1
fi

SRC_DIR="shaders_vk"
OUT_DIR="build/shaders"
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

# Find all shader files
FILES=$(find "$SRC_DIR" -type f \( -name '*.vert' -o -name '*.frag' -o -name '*.comp' -o -name '*.geom' -o -name '*.tesc' -o -name '*.tese' -o -name '*.glsl' \))

for f in $FILES; do
  rel="${f#$SRC_DIR/}"
  dir="$(dirname "$rel")"
  mkdir -p "$OUT_DIR/$dir"
  
  # Determine shader stage based on file extension
  case "$f" in
    *.vert) stage="-fshader-stage=vertex" ;;
    *.frag) stage="-fshader-stage=fragment" ;;
    *.comp) stage="-fshader-stage=compute" ;;
    *.geom) stage="-fshader-stage=geometry" ;;
    *.tesc) stage="-fshader-stage=tesc" ;;
    *.tese) stage="-fshader-stage=tese" ;;
    *.glsl) 
      # For .glsl files, try to determine stage from filename or content
      if echo "$f" | grep -q "\.vert\.glsl$"; then
        stage="-fshader-stage=vertex"
      elif echo "$f" | grep -q "\.frag\.glsl$"; then
        stage="-fshader-stage=fragment"
      elif echo "$f" | grep -q "\.comp\.glsl$"; then
        stage="-fshader-stage=compute"
      elif grep -q "main.*void" "$f" && grep -q "gl_Position" "$f"; then
        stage="-fshader-stage=vertex"
      elif grep -q "main.*void" "$f" && grep -q "gl_FragColor\|out.*vec4" "$f"; then
        stage="-fshader-stage=fragment"
      elif grep -q "main.*void" "$f" && grep -q "gl_GlobalInvocationID\|gl_LocalInvocationID" "$f"; then
        stage="-fshader-stage=compute"
      else
        echo "Skipping $f - cannot determine shader stage"
        continue
      fi
      ;;
    *) echo "Skipping $f - unknown shader type"; continue ;;
  esac
  
  "$GLSLC" -O -c $stage "$f" -o "$OUT_DIR/$rel.spv"
  echo "Compiled: $f -> $OUT_DIR/$rel.spv"
done

echo "All shaders compiled to $OUT_DIR"
