# VoxelVK Architecture Overview

## Table of Contents
1. [System Architecture](#system-architecture)
2. [Rendering Pipeline](#rendering-pipeline)
3. [Threading Model](#threading-model)
4. [GPU Queue Management](#gpu-queue-management)
5. [Memory Management](#memory-management)
6. [Asset Pipeline](#asset-pipeline)
7. [Subsystem Integration](#subsystem-integration)

## System Architecture

VoxelVK follows a modular, component-based architecture designed for high performance and maintainability. The system is built around several core subsystems that work together to provide efficient voxel rendering.

```
┌─────────────────────────────────────────────────────────────┐
│                     Application Layer                       │
├─────────────────────────────────────────────────────────────┤
│  Demos & Tools  │  ImGui Interface  │  Headless Runner     │
├─────────────────────────────────────────────────────────────┤
│                      Engine Core                            │
├─────────────────────────────────────────────────────────────┤
│ Asset Manager │ Scene Graph │ Input System │ Audio System   │
├─────────────────────────────────────────────────────────────┤
│                   Rendering System                          │
├─────────────────────────────────────────────────────────────┤
│  Voxel Mesher │ Frame Graph │ Post-Process │ Weather/Sky    │
├─────────────────────────────────────────────────────────────┤
│                     Vulkan Layer                            │
├─────────────────────────────────────────────────────────────┤
│ Command Pools │ Memory Alloc │ Pipeline Mgr │ Descriptor Mgr │
├─────────────────────────────────────────────────────────────┤
│                    Platform Layer                           │
├─────────────────────────────────────────────────────────────┤
│    GLFW/Win32    │    File I/O    │    Threading           │
└─────────────────────────────────────────────────────────────┘
```

### Core Principles

- **Performance First**: All systems are designed with performance as the primary concern
- **Memory Efficiency**: Custom allocators and memory pools minimize allocation overhead
- **Thread Safety**: Lock-free data structures where possible, clear ownership models
- **Modularity**: Components can be disabled or swapped out based on build configuration
- **Deterministic**: Reproducible builds and predictable runtime behavior

## Rendering Pipeline

The VoxelVK rendering pipeline is built around a frame graph architecture that automatically manages resource dependencies and GPU synchronization.

### Frame Graph Structure

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Voxel     │    │   Shadow    │    │  Lighting   │
│  Meshing    │───▶│   Mapping   │───▶│    Pass     │
│             │    │             │    │             │
└─────────────┘    └─────────────┘    └─────────────┘
       │                   │                   │
       ▼                   ▼                   ▼
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  Geometry   │    │    CSM      │    │  Forward    │
│   Buffer    │    │   Depth     │    │  Shading    │
│             │    │             │    │             │
└─────────────┘    └─────────────┘    └─────────────┘
       │                   │                   │
       └───────────────────┼───────────────────┘
                           ▼
                  ┌─────────────┐
                  │ Post-Process│
                  │   & TAA     │
                  │             │
                  └─────────────┘
                           │
                           ▼
                  ┌─────────────┐
                  │  Present    │
                  │             │
                  └─────────────┘
```

### Render Passes

1. **Voxel Meshing Pass**
   - Converts voxel data to triangle meshes
   - Uses compute shaders for parallel processing
   - Implements greedy meshing algorithms

2. **Shadow Mapping Pass**
   - Cascaded Shadow Maps (CSM) for directional light
   - Point light shadow cubes for local lighting
   - Percentage-Closer Soft Shadows (PCSS)

3. **Lighting Pass**
   - Physically Based Rendering (PBR)
   - Image-Based Lighting (IBL)
   - Dynamic weather integration

4. **Post-Processing**
   - Temporal Anti-Aliasing (TAA)
   - Screen Space Ambient Occlusion (SSAO)
   - Screen Space Reflections (SSR)
   - Tone mapping and gamma correction

## Threading Model

VoxelVK uses a job-based threading system to maximize CPU utilization while maintaining deterministic behavior.

```
Main Thread                 Worker Threads
     │                           │
     ├─ Input Processing         ├─ Voxel Generation
     ├─ Scene Updates           ├─ Mesh Processing  
     ├─ Render Commands         ├─ Asset Loading
     ├─ GPU Submission          ├─ Audio Processing
     └─ Present                 └─ AI/Physics
```

### Thread Categories

- **Main Thread**: UI, input, high-level coordination
- **Render Thread**: Vulkan command recording and submission
- **Worker Threads**: Parallel processing of voxel data, assets, AI
- **I/O Thread**: Asynchronous file operations and network requests

### Synchronization

- Lock-free ring buffers for inter-thread communication
- Atomic counters for job completion tracking
- Memory barriers for GPU/CPU synchronization
- Timeline semaphores for GPU work dependencies

## GPU Queue Management

VoxelVK efficiently utilizes multiple GPU queues to overlap different types of work.

### Queue Types

```
Graphics Queue:  [Render Commands] → [Present]
Compute Queue:   [Voxel Processing] → [Post-Effects]
Transfer Queue:  [Asset Upload] → [Buffer Copies]
```

### Queue Synchronization

- **Timeline Semaphores**: For fine-grained dependency tracking
- **Fences**: For CPU/GPU synchronization points
- **Events**: For intra-command buffer synchronization
- **Barriers**: For memory access ordering

## Memory Management

### Vulkan Memory Allocator (VMA) Integration

VMA provides efficient GPU memory management with the following allocation strategies:

- **Buffer Pools**: Pre-allocated pools for common buffer types
- **Staging Buffers**: Ring buffer for CPU→GPU transfers  
- **Frame Allocators**: Per-frame linear allocators for temporary data
- **Persistent Allocations**: Long-lived resources with dedicated memory

### Memory Types

```
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│   Device Local  │  │  Host Visible   │  │   Host Cached   │
│                 │  │                 │  │                 │
│ • Textures      │  │ • Staging       │  │ • Readback      │
│ • Vertex Buffers│  │ • Uniform Data  │  │ • Debug Data    │
│ • Index Buffers │  │ • Dynamic Mesh  │  │ • Profiling     │
└─────────────────┘  └─────────────────┘  └─────────────────┘
```

## Asset Pipeline

### Asset Types and Processing

```
Raw Assets                 Processed Assets
    │                           │
    ├─ Textures (.png/.jpg) ───▶ DDS/KTX2 + Mipmaps
    ├─ Meshes (.obj/.gltf)  ───▶ Optimized Geometry
    ├─ Voxels (.vox/.vxl)   ───▶ Compressed Chunks
    ├─ Audio (.wav/.ogg)    ───▶ Compressed Audio
    └─ Shaders (.glsl)      ───▶ SPIR-V Bytecode
```

### Asset Manager Features

- **Hot Reloading**: Automatic asset refresh during development
- **Streaming**: On-demand loading of large assets
- **Compression**: LZ4/Zstd compression for storage efficiency
- **Validation**: Asset integrity checking and format validation

## Subsystem Integration

### Weather System

The weather system integrates with multiple engine components:

- **Sky Rendering**: Atmospheric scattering and cloud simulation
- **Lighting**: Dynamic sun position and intensity
- **Particle Systems**: Rain, snow, and atmospheric effects
- **Audio**: Environmental audio based on weather conditions

### AI System

- **World Generation**: Procedural voxel world creation
- **Biome Detection**: Intelligent biome placement and transitions
- **Palette Management**: AI-driven color palette generation
- **Performance Optimization**: ML-based LOD and culling decisions

### Physics Integration

- **Voxel Collision**: Efficient collision detection with voxel data
- **Rigid Bodies**: Integration with physics engine for dynamic objects
- **Particle Physics**: GPU-accelerated particle simulations
- **Fluid Simulation**: Voxel-based fluid dynamics

## Performance Characteristics

### Target Performance

- **60+ FPS**: At 1080p with complex voxel scenes
- **120+ FPS**: P2 performance tier target
- **<16ms Frame Time**: Consistent frame pacing
- **<100MB**: Memory usage for core engine

### Optimization Techniques

- **Frustum Culling**: GPU-based visibility determination
- **Occlusion Culling**: Hardware occlusion queries
- **Level of Detail**: Automatic mesh simplification
- **Batch Rendering**: Instanced rendering for similar objects
- **GPU-Driven Rendering**: Minimize CPU/GPU synchronization

## Build Configuration

The architecture supports multiple build configurations:

### Release Configurations

- **Debug**: Full debugging, validation layers enabled
- **RelWithDebInfo**: Optimized with debug symbols
- **Release**: Full optimization, minimal debug info
- **CI-Release**: Deterministic builds for continuous integration

### Feature Toggles

```cpp
// Compile-time feature flags
#define VOXELVK_ENABLE_VALIDATION_LAYERS 1
#define VOXELVK_ENABLE_IMGUI_OVERLAY 1
#define VOXELVK_ENABLE_METRICS 1
#define VOXELVK_ENABLE_CRASH_HANDLER 1
```

## Future Architecture Considerations

### Planned Enhancements

- **Multi-GPU Support**: Explicit multi-GPU rendering
- **Ray Tracing**: Hardware RT for global illumination
- **Mesh Shaders**: Next-gen geometry pipeline
- **Variable Rate Shading**: Adaptive shading quality
- **GPU Scheduling**: Timeline semaphores for complex dependencies

This architecture provides a solid foundation for high-performance voxel rendering while maintaining flexibility for future enhancements and optimizations.