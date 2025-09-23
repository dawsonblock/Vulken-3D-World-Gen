#!/bin/bash
# GPU compute shader terrain generation demo script

set -e

echo "GPU Compute Shader Terrain Generation Demo"
echo "=========================================="

# Check if graphics build exists
if [ ! -d "build_graphics" ]; then
    echo "Graphics build not found. Building first..."
    ./scripts/build.sh --type graphics --no-tests
fi

# Run GPU compute shader terrain demo
echo "Running GPU compute shader terrain demo..."
./build_graphics/apps/gpu_compute_terrain_demo

echo ""
echo "GPU compute shader terrain demo complete!"
echo "Generated files:"
echo "- gpu_compute_terrain.vox (GPU-accelerated terrain with compute shader simulation)"
echo ""
echo "Features demonstrated:"
echo "- GPU compute shader simulation with parallel processing"
echo "- Work group-based execution with configurable group sizes"
echo "- Advanced noise computation with multiple octaves"
echo "- Memory bandwidth optimization and transfer simulation"
echo "- Performance benchmarking and metrics collection"
echo "- Environmental property calculation (temperature, humidity, pressure)"
echo "- CPU vs GPU performance comparison"
