#!/bin/bash
# Procedural world generation demo script

set -e

echo "Procedural World Generation Demo"
echo "================================="

# Check if graphics build exists
if [ ! -d "build_graphics" ]; then
    echo "Graphics build not found. Building first..."
    ./scripts/build.sh --type graphics --no-tests
fi

# Run procedural world demo with different configurations
echo "Running procedural world demo..."

echo ""
echo "1. Basic world with all modules:"
./build_graphics/apps/procedural_world_demo --width 128 --height 32 --depth 128 --seed 42

echo ""
echo "2. Mountain world:"
./build_graphics/apps/procedural_world_demo --width 64 --height 32 --depth 64 --modules "mountains,hills" --seed 123

echo ""
echo "3. Water world:"
./build_graphics/apps/procedural_world_demo --width 64 --height 32 --depth 64 --modules "rivers,lakes,plains" --seed 456

echo ""
echo "4. Forest world:"
./build_graphics/apps/procedural_world_demo --width 64 --height 32 --depth 64 --modules "plains,forests" --seed 789

echo ""
echo "5. Mining world:"
./build_graphics/apps/procedural_world_demo --width 64 --height 32 --depth 64 --modules "hills,caves,iron_ore,gold_ore" --seed 999

echo ""
echo "Procedural world demo complete!"
echo "Generated files:"
echo "- procedural_world.vox (modular world generation)"
echo ""
echo "Features demonstrated:"
echo "- Modular world generation system"
echo "- Multiple terrain types (mountains, hills, plains)"
echo "- Water features (rivers, lakes)"
echo "- Vegetation (forests)"
echo "- Underground features (caves, minerals)"
echo "- User-configurable parameters"
echo "- Command-line interface for customization"
