# VoxelVK Runtime Guide

## Quick Start

### Basic Demo Sequence
```bash
# 1. Basic functionality test
./build/smoke_headless

# 2. Weather system demonstration  
./build/weather_demo

# 3. Production system validation
./scripts/p2_validation.sh

# 4. Interactive demo with GUI
./build/main_imgui_vulkan
```

## Applications

### Core Engine

#### `smoke_headless`
**Purpose**: Basic engine stability test  
**Runtime**: <5 seconds  
**Output**: "smoke_headless: OK"  
**Use**: CI validation, system health check

#### `weather_demo`  
**Purpose**: Weather & sky system showcase  
**Runtime**: ~10 seconds (automated demo)  
**Features**: All weather states, lightning, frame graph execution  
**Output**: Comprehensive weather system demonstration

#### `weather_integration_test`
**Purpose**: Weather system integration validation  
**Runtime**: ~15 seconds  
**Features**: Console commands, real-time changes, performance monitoring

### Graphics Rendering

#### `vulkan_fullscreen_demo`
**Purpose**: Full Vulkan rendering pipeline  
**Runtime**: Interactive (until ESC)  
**Features**: Swapchain management, resize handling, fullscreen toggle  
**Controls**:
- **F11**: Toggle fullscreen
- **ESC**: Exit
- **Alt+Tab**: Test device resilience

#### `main_imgui_vulkan`  
**Purpose**: Production ImGui application with all systems  
**Runtime**: Interactive  
**Features**: Real-time HUD, weather controls, performance monitoring, RAG integration

**Controls**:
- **F11**: Fullscreen toggle
- **F5**: Hot-reload configuration  
- **Mouse**: ImGui interaction

### AI & Machine Learning

#### `rl_nav_demo`
**Purpose**: Reinforcement learning demonstration  
**Runtime**: ~2 minutes (100 episodes)  
**Features**: Navigation task, MLP training, learning progress  
**Output**: Saved model (`navigation_policy.vxml`)

**Expected Results**:
- Episode rewards improve over time
- Policy learns to reach target efficiently
- Model saves successfully

### Performance & Validation

#### `p2_concepts_validator`
**Purpose**: P2 performance system validation  
**Runtime**: ~5 seconds  
**Features**: 120 FPS analysis, TAA concepts, screen space validation  
**Expected**: All P2 systems validated

#### System Validation Scripts

**P0 Reliability**: `./scripts/p0_validation.sh`  
- Device capabilities probing
- Error handling infrastructure  
- Swapchain resilience testing
- Pipeline cache validation

**P1 Memory Management**: `./scripts/p1_validation.sh`  
- VMA integration validation
- VRAM budget analysis
- Frame allocator concepts  
- Asset optimization verification

**P2 Frame Pacing**: `./scripts/p2_validation.sh`  
- 120 FPS target validation
- TAA system verification
- Screen space effects analysis
- Performance monitoring validation

## Interactive Usage

### Weather System Controls

When running `main_imgui_vulkan` or other ImGui applications:

**Weather State Controls**:
- Clear, Cloudy, Rain, Snow, Storm, Fog buttons
- Real-time weather state transitions
- Lightning toggle for storms

**Weather Parameters**:
- Wind speed and direction sliders
- Cloud coverage adjustment  
- Precipitation rate control
- Fog density settings

**Console Commands** (in applications with console):
```bash
wx.set STORM          # Change weather state
wx.wind 15 270 0.8    # Wind: speed, direction, gustiness
wx.clouds 0.9         # Cloud coverage
wx.precip 25.0        # Precipitation rate (mm/h)
wx.fog 0.05           # Fog density
wx.lightning on       # Enable lightning flashes
```

### Performance Monitoring

**Real-Time HUD** (ImGui applications):
- FPS counter and frame timing
- GPU pass timing breakdown  
- VRAM budget utilization
- Weather system status
- Performance budget compliance

**Console Commands**:
```bash
r.targetfps 120       # Set target frame rate
r.framebudget 8.33    # Set frame time budget (ms)
r.perf.monitor true   # Enable performance tracking
r.perf.nvtx true      # Enable NVTX for Nsight
```

### Screen Space Effects

**SSAO (Screen Space Ambient Occlusion)**:
```bash
r.ssao.enable true    # Enable SSAO (default: ON)
r.ssao.radius 1.5     # Sampling radius
r.ssao.strength 1.0   # AO darkening strength
r.ssao.halfres true   # Half-resolution rendering
```

**SSR (Screen Space Reflections)**:
```bash
r.ssr.enable false          # Enable SSR (default: OFF, expensive)
r.ssr.maxdistance 50.0      # Maximum ray distance
r.ssr.thickness 0.5         # Surface thickness
r.ssr.roughnessaware true   # Fade with surface roughness
```

### AI Integration

#### RAG (Retrieval-Augmented Generation)

**Setup**:
```bash
# 1. Create knowledge base
mkdir -p data/rag_kb
echo "Elven architecture uses flowing organic curves" > data/rag_kb/elven_style.txt

# 2. Build search index
python scripts/build_rag_index.py

# 3. Run application with RAG
./build/main_imgui_vulkan
```

**Usage**:
- Toggle "Enable RAG" in ImGui interface
- Adjust "RAG Top-K" for retrieval count
- Generated content will use retrieved context

#### RL Training

**Navigation Demo**:
```bash
./build/rl_nav_demo

# Monitor training progress:
# Episode 0: reward=-15.2, avg_reward=0.0
# Episode 10: reward=2.3, avg_reward=1.2  
# Episode 50: reward=8.7, avg_reward=6.1
# Episode 90: reward=9.2, avg_reward=8.4
# Training converged! Average reward: 8.4
```

**Custom RL Integration**:
```cpp
// Create backend
auto backend = RLBackendFactory::create(RLBackendFactory::BackendType::MINIMAL_MLP);

// Training loop
std::vector<float> obs = env.reset();
std::vector<float> actions, values;
backend->forward(obs, actions, values);
auto result = env.step(actions);

// Update policy
backend->update(observations, actions, rewards, values, advantages);
```

## Performance Optimization

### Frame Rate Optimization

**Target 120 FPS**:
```bash
# Performance mode
r.targetfps 120
r.ssao.halfres true
r.ssr.enable false
r.perf.monitor true

# Check if target achieved
./scripts/p2_validation.sh
```

**Target 60 FPS (High Quality)**:
```bash
# Quality mode  
r.targetfps 60
r.ssao.halfres false
r.ssr.enable true
r.ssr.halfres false
```

### Memory Optimization

**VRAM Budget Management**:
- Monitor memory usage in ImGui HUD
- Automatic texture eviction when over budget
- Mesh LOD reduction for distant geometry
- Weather particle culling outside view frustum

**Performance Categories**:
- **Geometry**: Vertex/index buffers, mesh data
- **Textures**: Diffuse, normal, material textures
- **Weather**: Weather effects, particles, clouds
- **Render Targets**: Framebuffers, intermediate textures

## Development Workflow

### Hot-Reload Development

**Configuration Hot-Reload** (F5):
- Weather settings from `config/weather.yaml`
- AI palette from `ai_palette.cfg`  
- Shader hot-reload (Debug builds)

**Live Tuning**:
- Weather parameters via ImGui sliders
- Performance budgets via console commands
- Screen space effect parameters
- AI generation settings

### Debugging

**Validation Layers** (Debug builds):
```bash
# Enable comprehensive validation
export VK_LAYER_PATH=$VULKAN_SDK/etc/vulkan/explicit_layer.d
VK_LOADER_DEBUG=all ./build/weather_demo
```

**NVTX Profiling**:
```bash
# Nsight Graphics capture
nsight-gfx ./build/weather_demo

# CPU profiling  
perf record -g ./build/weather_demo
perf report
```

**Performance Analysis**:
```bash
# Generate performance report
./build/VoxelVK_Elite_ALL --bench performance.json
cat performance.json

# System validation
./build/test_integration
./build/test_performance_monitoring
```

## Production Deployment

### Performance Validation
```bash
# Complete validation suite
./scripts/p0_validation.sh && ./scripts/p1_validation.sh && ./scripts/p2_validation.sh

# Expected: All validation scripts report "COMPLETE SUCCESS"
```

### Quality Assurance
```bash
# Full test suite
ctest --test-dir build --output-on-failure

# Integration testing
./build/test_integration
./build/test_weather_system
./build/test_rl_backend
```

### Build Verification
```bash
# Shader compilation
cmake --build build --target ShaderSPV

# All applications
cmake --build build -j

# Performance benchmark
./build/weather_demo && ./build/p2_concepts_validator
```

The VoxelVK engine is now production-ready with comprehensive weather systems, 120 FPS performance optimization, AI integration, and extensive validation systems.