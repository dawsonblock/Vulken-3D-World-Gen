#!/bin/bash
# Advanced texturing system demo script

set -e

echo "Advanced Texturing System Demo"
echo "=============================="

# Check if graphics build exists
if [ ! -d "build_graphics" ]; then
    echo "Graphics build not found. Building first..."
    ./scripts/build.sh --type graphics --no-tests
fi

# Create output directories
mkdir -p textures decals

# Run advanced texturing demo
echo "Running advanced texturing demo..."
./build_graphics/apps/advanced_texturing_demo

echo ""
echo "Advanced texturing demo complete!"
echo "Generated files:"
echo "- textures/ (procedural textures: grass, stone, water, mountain, sand)"
echo "- decals/ (terrain decals: path, road, river)"
echo ""
echo "Features demonstrated:"
echo "- Procedural texture generation"
echo "- Triplanar mapping for seamless texture application"
echo "- Decal system for paths, roads, and rivers"
echo "- Weighted blending based on surface normals"
echo "- UV coordinate calculation for each plane"
