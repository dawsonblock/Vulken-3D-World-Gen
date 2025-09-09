#!/usr/bin/env bash
set -euo pipefail

echo "=== Minimal VoxelVK Build Script ==="

# Install additional missing packages that might be needed
sudo apt install -y pkg-config libjsoncpp-dev

# Create build directory
mkdir -p build_minimal
cd build_minimal

# Configure with minimal dependencies
cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DENABLE_TESTS=OFF \
    -DVOXELVK_ENABLE_CUDA=OFF \
    -DVOXELVK_ENABLE_TENSORRT=OFF \
    -DENABLE_IMGUI_OVERLAY=OFF \
    -DENABLE_VALIDATION_LAYERS=OFF \
    -DVOXELVK_DEMO_WERROR=OFF \
    -DVULKEN_ENABLE_WARNINGS_AS_ERRORS=OFF \
    -DCMAKE_CXX_FLAGS="-Wno-unknown-pragmas -Wno-error" \
    -DCMAKE_TOOLCHAIN_FILE="" \
    ..

# Build
make -j$(nproc)

echo "Build completed! Executables are in build_minimal/"
find . -type f -executable -name "*demo*" -o -name "*vulkan*" -o -name "*main*" | head -10
