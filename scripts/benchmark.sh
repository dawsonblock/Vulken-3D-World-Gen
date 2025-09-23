#!/bin/bash
# Performance benchmarking script for Vulken-3D-World-Gen

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Running performance benchmarks for Vulken-3D-World-Gen...${NC}"

# Check if build exists
if [ ! -d "build" ] && [ ! -d "build_graphics" ] && [ ! -d "build_headless" ]; then
    echo -e "${RED}Error: No build directory found. Please run build script first.${NC}"
    exit 1
fi

# Find the build directory
BUILD_DIR=""
if [ -d "build_graphics" ]; then
    BUILD_DIR="build_graphics"
elif [ -d "build" ]; then
    BUILD_DIR="build"
elif [ -d "build_headless" ]; then
    BUILD_DIR="build_headless"
fi

if [ -z "$BUILD_DIR" ]; then
    echo -e "${RED}Error: No valid build directory found.${NC}"
    exit 1
fi

echo "Using build directory: $BUILD_DIR"

# Run performance tests
echo "Running performance tests..."
cd "$BUILD_DIR"

# Run CTest with performance tests
if command -v ctest &> /dev/null; then
    echo "Running CTest performance tests..."
    ctest -R "perf" --output-on-failure
else
    echo -e "${YELLOW}Warning: CTest not found. Skipping performance tests.${NC}"
fi

# Run specific benchmarks if they exist
if [ -f "bin/VoxelVK_Elite_ALL" ]; then
    echo "Running main application benchmark..."
    time ./bin/VoxelVK_Elite_ALL --benchmark 2>&1 | tee benchmark_results.txt
fi

echo -e "${GREEN}Performance benchmarking completed!${NC}"
echo "Results saved to: $BUILD_DIR/benchmark_results.txt"
