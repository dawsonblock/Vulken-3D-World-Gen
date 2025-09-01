# VoxelRL_All

[![CI](https://github.com/dawsonblock/3D-WORLD/actions/workflows/ci.yml/badge.svg)](https://github.com/dawsonblock/3D-WORLD/actions/workflows/ci.yml)

**VoxelRL_All** is a high-performance reinforcement learning framework for voxel-based environments, built on Vulkan and CUDA. It provides a complete solution for training RL agents in procedurally generated 3D voxel worlds with GPU acceleration and optional Vulkan-based visualization.

## Features

### 🚀 High Performance
- **CUDA Acceleration**: GPU-accelerated world generation, voxel operations, and training
- **Vectorized Environments**: Run 256+ parallel environments efficiently
- **Optimized Memory**: Structure-of-Arrays (SoA) voxel storage with LRU caching
- **Batched Operations**: Efficient raycast DDA and voxel operations

### 🧠 Advanced RL Framework
- **PPO Implementation**: Proximal Policy Optimization with GAE and clipping
- **96-Action Space**: Comprehensive action discretization for voxel interaction
- **Goal-Directed Learning**: Flexible goal system with curriculum learning
- **Mixed Precision**: BF16 forward passes for faster training

### 🌍 Rich 3D Environments
- **Procedural Generation**: FBM terrain generation with caves and biomes
- **Block Physics**: Complete block registry with 256 block types
- **Advanced Lighting**: Dynamic lighting with shadow casting
- **Chunk System**: Infinite worlds with efficient chunk loading/unloading

### 🔧 Developer Friendly
- **CMake Presets**: Easy configuration for different build types
- **Docker Support**: H100-optimized containerized training
- **Profiling Tools**: NVTX integration for Nsight profiling
- **Comprehensive Testing**: Unit tests and benchmarks

## Quick Start

### Prerequisites
- **GPU**: NVIDIA GPU with compute capability 7.0+ (RTX 20xx series or newer)
- **CUDA**: CUDA 12.0 or newer
- **Vulkan**: Vulkan 1.3 compatible GPU and drivers
- **OS**: Linux (Ubuntu 20.04+) or Windows 10/11

### Installation

1. **Clone the repository**
   ```bash
   git clone https://github.com/dawsonblock/3D-WORLD.git
   cd 3D-WORLD
   ```

2. **Setup dependencies**
   ```bash
   ./scripts/setup.sh
   source setup_env.sh
   ```

3. **Build the project**
   ```bash
   ./scripts/build.sh
   ```

### Training

**Headless Training (Default)**
```bash
./scripts/run_headless.sh
```

**GUI Training with Vulkan Preview**
```bash
./scripts/run_gui.sh
```

**Custom Configuration**
```bash
./scripts/run_headless.sh \
  --num-envs 512 \
  --total-timesteps 50000000 \
  --learning-rate 1e-4 \
  --config-world config/world.yaml \
  --config-training config/training.yaml
```

## Architecture

This framework provides a complete reinforcement learning environment for voxel-based worlds with:

- **Vectorized Environments**: 256+ parallel training environments
- **CUDA Acceleration**: GPU kernels for terrain generation and voxel operations  
- **Advanced RL**: PPO with 96-action discrete space and curriculum learning
- **Infinite Worlds**: Chunk-based streaming with procedural generation
- **High Performance**: Optimized memory layout and batched operations

## Configuration

Build with CMake presets for different use cases:
```bash
# Development build with debug info
./scripts/build.sh --preset debug

# Optimized release build
./scripts/build.sh --preset release  

# Headless training (no GUI)
./scripts/build.sh --preset headless
```


<!-- Perf badge: replace <OWNER_OR_USER> and <REPO> -->
![frametime](https://img.shields.io/endpoint?url=https://<OWNER_OR_USER>.github.io/<REPO>/badge.json)
