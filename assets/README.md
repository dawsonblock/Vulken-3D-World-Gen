# VoxelVK Assets

This directory contains sample assets for the VoxelVK engine.

## Directory Structure

```
assets/
├── README.md           # This file
├── samples/           # Sample assets (<10MB total)
│   ├── textures/     # Sample textures
│   ├── meshes/       # Sample mesh files
│   ├── voxels/       # Sample voxel data
│   └── audio/        # Sample audio files
└── config/           # Asset configuration files
    └── assets.yaml   # Asset pack configuration
```

## Licensing

All sample assets in this directory are either:
- Created specifically for VoxelVK (CC0 - Public Domain)
- Licensed under permissive licenses (MIT, CC0, CC BY)
- Procedurally generated content

### License Details

- **Textures**: Procedurally generated or CC0 licensed
- **Meshes**: Simple geometric shapes (CC0)
- **Voxel Data**: Procedurally generated test patterns (CC0)
- **Audio**: Synthetic test tones (CC0)

## Size Policy

Sample assets are kept under 10MB total to ensure:
- Fast repository clones
- Reasonable CI/CD build times
- Easy distribution

For larger assets, use the asset fetching system via `scripts/fetch_assets.py`.

## Asset Formats

### Supported Texture Formats
- PNG (preferred for textures with transparency)
- JPEG (for photos/complex textures)
- DDS (for compressed GPU textures)

### Supported Mesh Formats
- OBJ (simple meshes)
- GLTF/GLB (complex scenes with materials)

### Supported Voxel Formats
- Custom VXL format (compressed voxel chunks)
- Raw binary format for simple data

### Supported Audio Formats
- WAV (uncompressed)
- OGG Vorbis (compressed)

## Configuration

Assets are configured via `config/assets.yaml`:

```yaml
asset_packs:
  - name: "default"
    path: "samples/"
    description: "Default sample assets"
    
  - name: "test_data" 
    path: "test/"
    description: "Test assets for automated testing"

texture_settings:
  default_filter: linear
  generate_mipmaps: true
  compression: auto

audio_settings:
  default_format: ogg
  quality: medium
```

## Usage

Assets can be loaded through the VoxelVK asset system:

```cpp
#include "core/asset_manager.hpp"

// Load texture
auto texture = AssetManager::loadTexture("samples/textures/grass.png");

// Load mesh
auto mesh = AssetManager::loadMesh("samples/meshes/cube.obj");

// Load voxel data
auto voxels = AssetManager::loadVoxelChunk("samples/voxels/terrain.vxl");
```

## Adding New Assets

1. Ensure the asset has a compatible license
2. Keep total size under the 10MB limit
3. Add appropriate entries to `config/assets.yaml`
4. Update this README if adding new formats
5. Test loading through the asset system

## Procedural Generation

Many sample assets are generated procedurally to avoid licensing issues and keep sizes small. See `scripts/generate_sample_assets.py` for the generation code.

## External Asset Packs

For larger asset collections, use the fetch system:

```bash
# Fetch additional asset packs
python scripts/fetch_assets.py --pack extended_textures
python scripts/fetch_assets.py --pack demo_scenes
```

This downloads assets from external sources with proper license verification.