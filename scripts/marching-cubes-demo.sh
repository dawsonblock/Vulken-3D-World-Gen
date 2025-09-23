#!/bin/bash
# Marching cubes terrain generation demo script

set -e

echo "Marching Cubes Terrain Generation Demo"
echo "======================================"

# Check if graphics build exists
if [ ! -d "build_graphics" ]; then
    echo "Graphics build not found. Building first..."
    ./scripts/build.sh --type graphics --no-tests
fi

# Run marching cubes terrain demo
echo "Running marching cubes terrain demo..."
./build_graphics/apps/marching_cubes_terrain_demo

echo ""
echo "Marching cubes terrain demo complete!"
echo "Generated files:"
echo "- marching_cubes_terrain.obj (3D mesh in OBJ format)"
echo "- marching_cubes_terrain.ply (3D mesh in PLY format)"
echo ""
echo "Features demonstrated:"
echo "- Advanced marching cubes algorithm for complex 3D geometries"
echo "- Multi-octave noise for realistic terrain features"
echo "- Cave systems and mountain ranges"
echo "- Smooth surface generation with proper normals"
echo "- Export to OBJ and PLY formats"
echo "- Isosurface extraction with configurable threshold"
