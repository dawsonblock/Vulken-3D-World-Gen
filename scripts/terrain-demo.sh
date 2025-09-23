#!/bin/bash
# Advanced terrain generation demo script

set -e

echo "Advanced Terrain Generation Demo"
echo "================================"

# Check if graphics build exists
if [ ! -d "build_graphics" ]; then
    echo "Graphics build not found. Building first..."
    ./scripts/build.sh --type graphics --no-tests
fi

# Run advanced terrain demo
echo "Running advanced terrain demo..."
./build_graphics/apps/advanced_terrain_demo

echo ""
echo "Advanced terrain demo complete!"
echo "Generated files:"
echo "- advanced_terrain.vox (3D voxel terrain with caves)"
echo ""
echo "Features demonstrated:"
echo "- 3D voxel-based terrain generation"
echo "- Procedural cave networks using distance fields"
echo "- Multi-layer terrain (water, grass, stone, mountains)"
echo "- Fractal noise for realistic height variation"
echo "- Density-based voxel classification"
