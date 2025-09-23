#!/bin/bash
# Advanced 3D terrain generation demo script

set -e

echo "Advanced 3D Terrain Generation Demo"
echo "===================================="

# Check if graphics build exists
if [ ! -d "build_graphics" ]; then
    echo "Graphics build not found. Building first..."
    ./scripts/build.sh --type graphics --no-tests
fi

# Run advanced 3D terrain demo
echo "Running advanced 3D terrain demo..."
./build_graphics/apps/advanced_3d_terrain_demo

echo ""
echo "Advanced 3D terrain demo complete!"
echo "Generated files:"
echo "- advanced_3d_terrain.vox (complex 3D terrain with caves, arches, overhangs)"
echo ""
echo "Features demonstrated:"
echo "- Complex 3D geometries (caves, arches, overhangs)"
echo "- Advanced noise functions (simplex, ridged, domain warping)"
echo "- Erosion simulation for realistic terrain"
echo "- Multi-layer terrain with temperature and humidity"
echo "- Support for caves, arches, and overhangs"
