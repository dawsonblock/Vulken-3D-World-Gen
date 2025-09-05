#!/bin/bash
# VoxelVK External Asset Fetcher
# This script fetches large assets from the external voxelvk-assets repository

set -e

EXTERNAL_REPO="https://github.com/voxelvk/voxelvk-assets.git"
ASSETS_DIR="assets"
EXTERNAL_DIR="voxelvk-assets"

echo "VoxelVK External Asset Fetcher"
echo "=============================="

# Check if git-lfs is available
if ! command -v git-lfs &> /dev/null; then
    echo "Warning: git-lfs not found. Large files may not download properly."
    echo "Install git-lfs: https://git-lfs.github.io/"
fi

# Clone or update external assets
if [ -d "$EXTERNAL_DIR" ]; then
    echo "Updating external assets..."
    cd "$EXTERNAL_DIR"
    git pull
    cd ..
else
    echo "Cloning external assets..."
    git clone "$EXTERNAL_REPO" "$EXTERNAL_DIR"
fi

# Copy assets to local directory
echo "Copying assets..."
cp -r "$EXTERNAL_DIR"/* "$ASSETS_DIR/"

echo "External assets fetched successfully!"
echo "You can now run VoxelVK with full asset support."
