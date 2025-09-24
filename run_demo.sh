#!/bin/bash

# Vulkan 3D World Generation Demo Runner
# This script sets up the Vulkan environment and runs demos

# Set up Vulkan environment
export VULKAN_SDK="$HOME/VulkanSDK/1.4.321.0/macOS"
export PATH="$VULKAN_SDK/bin:$PATH"
export VK_ICD_FILENAMES="/opt/homebrew/Cellar/molten-vk/1.4.0/etc/vulkan/icd.d/MoltenVK_icd.json"
export DYLD_LIBRARY_PATH="/opt/homebrew/Cellar/molten-vk/1.4.0/lib"

# Navigate to build directory
cd build_graphics

# Function to show available demos
show_demos() {
    echo "Available Demos:"
    echo "================="
    echo "1.  simple_world_viewer_demo     - Basic world viewer with WASD controls"
    echo "2.  advanced_terrain_demo        - Advanced 3D terrain generation"
    echo "3.  procedural_world_demo        - Procedural world generation with modules"
    echo "4.  marching_cubes_demo          - Marching cubes terrain generation"
    echo "5.  gpu_terrain_demo             - GPU-accelerated terrain generation"
    echo "6.  advanced_texturing_demo      - Advanced texturing techniques"
    echo "7.  advanced_triplanar_demo      - Triplanar texturing demo"
    echo "8.  advanced_3d_terrain_demo     - Advanced 3D terrain with caves"
    echo "9.  gpu_compute_terrain_demo     - GPU compute terrain generation"
    echo "10. world_generator_demo         - World generator demo"
    echo "11. world_visualizer_demo        - World visualizer demo"
    echo "12. simple_marching_cubes_demo   - Simple marching cubes demo"
    echo ""
    echo "Usage: $0 [demo_name] [options]"
    echo "Example: $0 simple_world_viewer_demo --width 64 --height 64"
    echo "Example: $0 advanced_terrain_demo"
    echo "Example: $0 procedural_world_demo --width 128 --height 32 --depth 128"
}

# Check if no arguments provided
if [ $# -eq 0 ]; then
    show_demos
    exit 0
fi

# Get demo name
DEMO_NAME="$1"
shift

# Check if demo exists
if [ ! -f "apps/$DEMO_NAME" ]; then
    echo "Error: Demo '$DEMO_NAME' not found!"
    echo ""
    show_demos
    exit 1
fi

# Run the demo
echo "Running: $DEMO_NAME with arguments: $@"
echo "=========================================="
./apps/$DEMO_NAME "$@"
