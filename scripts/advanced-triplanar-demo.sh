#!/bin/bash
# Advanced triplanar mapping demo script

set -e

echo "Advanced Triplanar Mapping Demo"
echo "==============================="

# Check if graphics build exists
if [ ! -d "build_graphics" ]; then
    echo "Graphics build not found. Building first..."
    ./scripts/build.sh --type graphics --no-tests
fi

# Create output directories
mkdir -p textures decals

# Run advanced triplanar mapping demo
echo "Running advanced triplanar mapping demo..."
./build_graphics/apps/advanced_triplanar_demo

echo ""
echo "Advanced triplanar mapping demo complete!"
echo "Generated files:"
echo "- textures/ (advanced procedural textures with material properties)"
echo "- decals/ (advanced decals with albedo, normal, and roughness maps)"
echo ""
echo "Features demonstrated:"
echo "- Advanced procedural texture generation with material properties"
echo "- Enhanced triplanar mapping with normal-based blending"
echo "- Advanced decal system with albedo, normal, and roughness maps"
echo "- Material property integration (roughness, metallic, AO)"
echo "- Sharpness control for blending transitions"
echo "- Environmental property calculation"
