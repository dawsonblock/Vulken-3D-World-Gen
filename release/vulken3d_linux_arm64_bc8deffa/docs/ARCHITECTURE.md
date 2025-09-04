# Vulken-3D Engine Architecture
===============================

## System Overview

Vulken-3D is a high-performance voxel-based world generation and rendering engine built on Vulkan. The engine emphasizes modularity, performance, and scalability for large-scale procedural world generation.

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
├─────────────────────────────────────────────────────────────┤
│  World Gen  │  Rendering  │  Physics  │  Weather  │   AI   │
├─────────────────────────────────────────────────────────────┤
│              Vulkan Abstraction Layer                       │
├─────────────────────────────────────────────────────────────┤
│    Memory Mgmt   │   Pipeline Cache   │   Device Caps      │
├─────────────────────────────────────────────────────────────┤
│                     Vulkan API                              │
└─────────────────────────────────────────────────────────────┘
```

## Core Systems

### 1. Vulkan Device Management (`src/vk/`)

**Device Capabilities (`device_caps.*`)**
- Hardware feature detection and validation
- Memory type selection and optimization
- Queue family management
- Extension and layer enumeration

**Memory Manager (`memory_manager.*`)**
- VMA (Vulkan Memory Allocator) integration
- Buffer and image allocation strategies
- Memory budget tracking and enforcement
- Garbage collection for unused resources

**Frame Allocator (`frame_allocator.*`)**
- Per-frame resource allocation
- Automatic cleanup of temporary resources
- Ring buffer allocation patterns
- GPU/CPU synchronization

### 2. Rendering Pipeline (`src/render/`)

**Frame Graph (`frame_graph.*`)**
- Automatic render pass generation
- Resource dependency tracking
- GPU timeline optimization
- Multi-threaded command buffer recording

**Cascaded Shadow Maps (`csm.*`)**
- Dynamic cascade distribution
- Texel snapping for stable shadows
- PCSS soft shadow filtering
- GPU-based frustum culling

**Temporal Anti-Aliasing (`taa_system.*`)**
- Motion vector generation
- History buffer management
- Variance clipping for ghosting reduction
- Adaptive sample weighting

**Screen Space Effects (`screen_space_effects.*`)**
- SSAO with bilateral filtering
- Screen-space reflections
- Height-based fog rendering
- Temporal accumulation buffers

### 3. World Generation (`src/world/`, `src/env/`)

**Chunk Management**
- 64³ voxel chunks with LOD support
- Asynchronous generation pipeline
- Memory-efficient storage (RLE compression)
- Streaming and caching strategies

**Meshing Algorithms (`tests/unit/test_mesher_core.cpp`)**
- Naive face-per-voxel meshing
- Greedy meshing optimization
- GPU compute-based mesh generation
- Mesh optimization and LOD generation

**Procedural Generation**
- Multi-octave noise generation
- Biome-based material assignment
- Cave and structure placement
- Erosion and weathering simulation

### 4. Physics System (`src/physics/cpp/`)

**Collision Detection**
- AABB and capsule collision primitives
- Voxel-to-geometry collision queries
- Broad-phase spatial partitioning
- Continuous collision detection

**Player Controller (`cpp_player_controller.*`)**
- Capsule-based character representation
- Ground detection and slope handling
- Smooth movement and jumping
- Integration with rendering camera

### 5. Weather Simulation (`src/env/weather/`)

**Weather State Machine (`weather_system.*`)**
- 8 distinct weather types
- Realistic parameter transitions
- Seasonal and daily variations
- Performance-optimized updates

**Sky Model (`sky_model.*`)**
- Atmospheric scattering simulation
- Dynamic cloud generation
- Sun/moon positioning
- HDR environment mapping

## Performance Systems

### Memory Management Strategy

**Allocation Patterns**
```cpp
// Frame-based allocation
class FrameAllocator {
    VkBuffer frameBuffers[3];  // Triple buffering
    uint32_t currentFrame;
    
    void* allocateTransient(size_t size);
    void resetFrame();  // Called each frame
};

// Persistent allocation
class MemoryManager {
    VmaAllocator allocator;
    
    VkBuffer createBuffer(const BufferCreateInfo& info);
    VkImage createImage(const ImageCreateInfo& info);
    void trackResource(Resource* resource);
};
```

**Memory Budget Management**
- Dynamic allocation based on GPU memory availability
- Aggressive eviction of unused resources
- Streaming for large datasets (textures, meshes)
- Compressed storage for voxel data

### Compute Shader Pipeline

**Voxel Meshing (`shaders/core/voxel_mesher.comp`)**
- Work group size: 8×8×8 (512 threads)
- Shared memory optimization
- Face culling and optimization
- Output to structured buffers

**Chunk Culling (`shaders/core/chunk_cull.comp`)**
- Frustum culling per chunk
- Occlusion culling with depth buffer
- Distance-based LOD selection
- GPU-driven rendering pipeline

**Greedy Meshing (`shaders/core/greedy_mesh.comp`)**
- Slice-based optimization
- Material consistency checking
- Quad merging algorithms
- Significant vertex reduction

### Rendering Optimizations

**GPU-Driven Rendering**
- Indirect draw commands
- GPU culling and LOD selection
- Multi-draw indirect with count
- Reduced CPU-GPU synchronization

**Pipeline State Management**
```cpp
class PipelineCache {
    std::unordered_map<PipelineKey, VkPipeline> cache;
    
    VkPipeline getPipeline(const PipelineKey& key);
    void precompileCommonPipelines();
    void savePipelineCache(const std::string& filename);
};
```

## Asset Pipeline

### Asset Storage (`config/datasets.yaml`)

**Storage Backends**
- **Filesystem**: Development and small deployments
- **Redis**: Distributed caching and multiplayer
- **S3-Compatible**: Cloud storage for large assets

**Asset Types**
- **Textures**: PBR materials with compression
- **Meshes**: Optimized geometry with LODs  
- **Palettes**: Biome-specific color schemes
- **Voxel Data**: Compressed chunk representations

### Content Pipeline

**Build-Time Processing**
```bash
# Texture optimization
scripts/assets/optimize_textures.py --input raw/ --output assets/textures/

# Mesh processing
scripts/assets/process_meshes.py --optimize --generate-lods

# Voxel data compilation
scripts/world/compile_chunks.py --compression rle --output chunks/
```

**Runtime Loading**
- Asynchronous asset loading
- Progressive quality upgrades
- Memory budget enforcement
- Cache warming strategies

## Scalability Architecture

### Threading Model

**Main Thread**
- Input handling and game logic
- High-level resource management
- Vulkan command submission

**Render Thread** 
- Command buffer recording
- GPU resource management
- Frame synchronization

**World Generation Threads** (4x)
- Chunk generation and meshing
- Biome and structure placement
- Physics mesh generation

**Asset Loading Threads** (2x)
- File I/O and decompression
- GPU resource upload
- Cache management

### Multi-GPU Support

**Resource Distribution**
- Primary GPU: Rendering and compute
- Secondary GPU: World generation
- Shared memory pools via external memory
- Cross-device synchronization

### Networking Architecture (Future)

**Client-Server Model**
- Authoritative server for world state
- Client-side prediction and rollback
- Delta compression for world updates
- P2P for local multiplayer

## Configuration System

### Hierarchical Configuration

**Priority Order**
1. Command-line arguments
2. Environment variables  
3. User configuration files
4. Default configuration
5. Compiled-in defaults

**Hot Reload Support**
```cpp
class ConfigManager {
    void watchFile(const std::string& path);
    void reloadConfiguration();
    
    template<typename T>
    T get(const std::string& key) const;
    
    void registerCallback(const std::string& key, 
                         std::function<void(const ConfigValue&)> callback);
};
```

### Performance Profiles

**Quality Presets**
- **Ultra**: Maximum visual quality, high-end hardware
- **High**: Balanced quality/performance  
- **Medium**: Good quality, mid-range hardware
- **Low**: Performance-focused, integrated graphics
- **Potato**: Minimum viable rendering

## Debugging and Profiling

### Vulkan Validation

**Validation Layers**
- Standard validation for API usage
- GPU-assisted validation for shaders
- Best practices advisor
- Synchronization validation

**Debug Markers**
```cpp
void VulkanDebugUtils::beginLabel(VkCommandBuffer cmd, 
                                 const char* name, 
                                 const float color[4]) {
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = name;
    memcpy(label.color, color, 4 * sizeof(float));
    
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
}
```

### Performance Analysis

**Built-in Profiler**
- GPU timestamp queries
- CPU timing with high precision
- Memory usage tracking
- Frame time histograms

**Integration with External Tools**
- NVIDIA Nsight Graphics integration
- AMD Radeon GPU Profiler support
- Intel GPA compatibility
- Custom profiler hooks

## Platform Support

### Supported Platforms
- **Windows 10/11**: Primary development platform
- **Linux**: Ubuntu 22.04+, Arch, Fedora
- **macOS**: Via MoltenVK (experimental)

### Hardware Requirements

**Minimum Specifications**
- GPU: Vulkan 1.1 compatible
- RAM: 8GB system memory
- VRAM: 4GB dedicated graphics memory
- CPU: 4-core processor

**Recommended Specifications**  
- GPU: RTX 3060 / RX 6600 XT or better
- RAM: 16GB system memory
- VRAM: 8GB dedicated graphics memory
- CPU: 8-core processor with high single-thread performance

## Future Roadmap

### Short Term (6 months)
- WebGPU compatibility layer
- Improved Linux support
- Mobile rendering profile
- Vulkan 1.3 feature adoption

### Medium Term (1 year)
- AI-enhanced world generation
- Real-time ray tracing integration
- Multi-GPU rendering
- Cloud rendering support

### Long Term (2+ years)
- Full networking and multiplayer
- VR/AR support
- Advanced physics simulation
- Procedural animation system