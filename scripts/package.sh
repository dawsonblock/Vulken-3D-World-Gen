#!/bin/bash
# Package creation script for Vulken-3D-World-Gen

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Creating package for Vulken-3D-World-Gen...${NC}"

# Get version from git or default
VERSION=$(git describe --tags --always 2>/dev/null || echo "0.1.0")
PACKAGE_NAME="vulken-3d-world-gen-${VERSION}"

echo "Creating package: $PACKAGE_NAME"

# Create package directory
PACKAGE_DIR="packages/$PACKAGE_NAME"
mkdir -p "$PACKAGE_DIR"

# Copy binaries
echo "Copying binaries..."
if [ -d "build_graphics" ]; then
    cp -r build_graphics/bin "$PACKAGE_DIR/"
    cp -r build_graphics/lib "$PACKAGE_DIR/" 2>/dev/null || true
elif [ -d "build" ]; then
    cp -r build/bin "$PACKAGE_DIR/"
    cp -r build/lib "$PACKAGE_DIR/" 2>/dev/null || true
fi

# Copy shaders
echo "Copying shaders..."
if [ -d "build_graphics/.cache/spv" ]; then
    cp -r build_graphics/.cache/spv "$PACKAGE_DIR/shaders"
elif [ -d "build/.cache/spv" ]; then
    cp -r build/.cache/spv "$PACKAGE_DIR/shaders"
fi

# Copy assets
echo "Copying assets..."
if [ -d "assets" ]; then
    cp -r assets "$PACKAGE_DIR/"
fi

# Copy configs
echo "Copying configs..."
if [ -d "config" ]; then
    cp -r config "$PACKAGE_DIR/"
fi

# Copy documentation
echo "Copying documentation..."
cp README.md "$PACKAGE_DIR/" 2>/dev/null || true
cp LICENSE "$PACKAGE_DIR/" 2>/dev/null || true
cp CHANGELOG.md "$PACKAGE_DIR/" 2>/dev/null || true

# Create package info
echo "Creating package info..."
cat > "$PACKAGE_DIR/package_info.txt" << EOF
Vulken-3D-World-Gen Package
Version: $VERSION
Build Date: $(date)
Platform: $(uname -s)-$(uname -m)
EOF

# Create archive
echo "Creating archive..."
cd packages
tar -czf "${PACKAGE_NAME}.tar.gz" "$PACKAGE_NAME"
cd ..

echo -e "${GREEN}Package created successfully!${NC}"
echo "Package location: packages/${PACKAGE_NAME}.tar.gz"
echo "Package size: $(du -h packages/${PACKAGE_NAME}.tar.gz | cut -f1)"
