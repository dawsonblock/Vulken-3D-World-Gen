#!/bin/bash
# GPU-accelerated terrain generation demo script

set -e

echo "GPU-Accelerated Terrain Generation Demo"
echo "======================================="

# Check if graphics build exists
if [ ! -d "build_graphics" ]; then
    echo "Graphics build not found. Building first..."
    ./scripts/build.sh --type graphics --no-tests
fi

# Run GPU terrain demo
echo "Running GPU terrain demo..."
./build_graphics/apps/gpu_terrain_demo

echo ""
echo "GPU terrain demo complete!"
echo "Generated files:"
echo "- gpu_terrain.vox (GPU-accelerated terrain with chunk-based processing)"
echo ""
echo "Features demonstrated:"
echo "- GPU-accelerated noise generation"
echo "- Chunk-based terrain processing"
echo "- Parallel computation simulation"
echo "- Performance benchmarking (CPU vs GPU)"
echo "- Memory-efficient data structures"
echo "- Scalable terrain generation"
