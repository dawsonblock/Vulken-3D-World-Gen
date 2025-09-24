# Vulkan 3D World Generation - Quick Start Guide

## 🚀 Getting Started

The project is now built and ready to use! Here's how to run the demos:

### Quick Run
```bash
# Show all available demos
./run_demo.sh

# Run a specific demo
./run_demo.sh simple_world_viewer_demo
./run_demo.sh advanced_terrain_demo
./run_demo.sh procedural_world_demo
```

### Manual Run (if you prefer)
```bash
# Set up Vulkan environment
export VULKAN_SDK="$HOME/VulkanSDK/1.4.321.0/macOS"
export PATH="$VULKAN_SDK/bin:$PATH"
export VK_ICD_FILENAMES="$(brew --prefix)/share/vulkan/icd.d/MoltenVK_icd.json"

# Navigate to build directory
cd build_graphics

# Run any demo
./apps/simple_world_viewer_demo
./apps/advanced_terrain_demo
./apps/procedural_world_demo
```

## 🎮 Available Demos

| Demo | Description | Key Features |
|------|-------------|--------------|
| `simple_world_viewer_demo` | Basic world viewer | WASD controls, mouse look, ESC to exit |
| `advanced_terrain_demo` | Advanced 3D terrain generation | Multi-layer terrain, caves, fractal noise |
| `procedural_world_demo` | Procedural world generation | Modular system, configurable parameters |
| `marching_cubes_demo` | Marching cubes terrain | Isosurface generation, smooth terrain |
| `gpu_terrain_demo` | GPU-accelerated terrain | High-performance GPU generation |
| `advanced_texturing_demo` | Advanced texturing | Triplanar mapping, decals, PBR materials |
| `advanced_triplanar_demo` | Triplanar texturing | Seamless texture blending |
| `advanced_3d_terrain_demo` | Advanced 3D terrain with caves | Complex cave networks, realistic terrain |
| `gpu_compute_terrain_demo` | GPU compute terrain | Compute shader-based generation |
| `world_generator_demo` | World generator | Batch world generation |
| `world_visualizer_demo` | World visualizer | Interactive world exploration |
| `simple_marching_cubes_demo` | Simple marching cubes | Basic isosurface demo |

## 🎯 Demo Examples

### Basic World Viewer
```bash
./run_demo.sh simple_world_viewer_demo --width 64 --height 64 --depth 64
```
**Controls:** WASD (move), Space (up), Shift (down), Mouse (look), ESC (exit)

### Advanced Terrain Generation
```bash
./run_demo.sh advanced_terrain_demo
```
**Output:** Generates `advanced_terrain.vox` with multi-layer terrain

### Procedural World with Custom Parameters
```bash
./run_demo.sh procedural_world_demo --width 128 --height 32 --depth 128 --seed 42
```

### GPU-Accelerated Terrain
```bash
./run_demo.sh gpu_terrain_demo
```

## 📁 Project Structure

```
Vulken-3D-World-Gen-4/
├── build_graphics/          # Main build directory
│   └── apps/               # All executable demos
├── src/                    # Source code
├── shaders_vk/            # Vulkan shaders
├── assets/                # Game assets
├── docs/                  # Documentation
└── run_demo.sh           # Demo runner script
```

## 🔧 Troubleshooting

### If demos don't run:
1. Make sure you're in the project root directory
2. Ensure Vulkan environment is set up:
   ```bash
   export VULKAN_SDK="$HOME/VulkanSDK/1.4.321.0/macOS"
   export PATH="$VULKAN_SDK/bin:$PATH"
   export VK_ICD_FILENAMES="$(brew --prefix)/share/vulkan/icd.d/MoltenVK_icd.json"
   ```

### If you get permission errors:
```bash
chmod +x run_demo.sh
chmod +x build_graphics/apps/*
```

### If Vulkan validation errors appear:
These are normal warnings and don't affect functionality. The demos will work fine.

## 🎨 Features

- **3D Voxel-based Terrain**: Procedural generation with multiple material types
- **Advanced Rendering**: Vulkan-based rendering with modern graphics techniques
- **GPU Acceleration**: Compute shader-based terrain generation
- **Modular System**: Pluggable world generation modules
- **Real-time Visualization**: Interactive 3D world exploration
- **Cross-platform**: Works on macOS with MoltenVK

## 📚 Next Steps

1. **Explore the demos**: Try different demos to see various features
2. **Modify parameters**: Use command-line options to customize generation
3. **Check the source**: Look at `src/` directory for implementation details
4. **Read documentation**: Check `docs/` directory for detailed guides
5. **Experiment**: Modify shaders in `shaders_vk/` or source code

Happy world building! 🌍✨
