# Vulken-3D Quick Start Guide
=============================

## Prerequisites

### System Requirements
- **OS**: Windows 10+, Ubuntu 20.04+, or macOS 12+
- **GPU**: Vulkan 1.1+ compatible (NVIDIA GTX 1060, AMD RX 580, or better)
- **RAM**: 8GB minimum, 16GB recommended
- **Storage**: 2GB free space

### Software Dependencies
- **Vulkan SDK**: Download from https://vulkan.lunarg.com/
- **CMake 3.27+**: https://cmake.org/download/
- **Python 3.8+**: For build scripts and tools

## Quick Start (5 minutes)

### Option A: Pre-built Release Bundle
```bash
# Download release bundle
wget https://github.com/your-org/vulken-3d/releases/latest/download/vulken3d_linux_x64.zip

# Extract and run
unzip vulken3d_linux_x64.zip
cd vulken3d_linux_x64/
./run.sh
```

### Option B: Docker (Recommended for testing)
```bash
# Clone repository
git clone https://github.com/your-org/vulken-3d-world-gen.git
cd vulken-3d-world-gen/

# Start with Docker Compose
docker-compose up -d

# Check status
docker-compose ps
docker-compose logs vulken3d

# Access operator console
open http://localhost:8080
```

### Option C: Build from Source
```bash
# Clone repository
git clone https://github.com/your-org/vulken-3d-world-gen.git
cd vulken-3d-world-gen/

# Install Python dependencies
pip3 install pyyaml redis

# Configure and build
cmake --preset default
cmake --build build -j$(nproc)

# Run headless demo
./build/apps/smoke_graphics_headless --config config/engine.yaml

# Run operator console
./build/apps/operator_console
```

## Available Applications

### Core Applications
- **`smoke_graphics_headless`**: Validation and testing without GUI
- **`operator_console`**: Real-time monitoring and control interface  
- **`gui_fullscreen_demo`**: Full-screen graphics demonstration
- **`weather_demo`**: Interactive weather system showcase

### Usage Examples
```bash
# Headless validation (CI/testing)
./smoke_graphics_headless --headless --validate --config config/engine.yaml

# Interactive operator console
./operator_console --config config/renderer.yaml

# Weather demonstration with specific conditions
./weather_demo --weather thunderstorm --time 18:30
```

## Configuration

### Quick Configuration Changes

**Performance Tuning (`config/renderer.yaml`)**
```yaml
renderer:
  msaa_samples: 4        # 1-8, lower = better performance
  resolution:
    width: 1920         
    height: 1080

post_processing:
  ssao:
    enabled: true       # Disable for better performance
  ssr: 
    enabled: false      # Expensive, disable on lower-end hardware
```

**World Settings (`config/engine.yaml`)**  
```yaml
world:
  render_distance: 8    # 4-16, lower = better performance
  chunk_size: 64
  generation_threads: 4

performance:
  target_fps: 60
  vsync: true
```

**Asset Storage (`config/datasets.yaml`)**
```yaml
storage:
  mode: "filesystem"    # "filesystem" | "redis" | "s3"
  cache_size_mb: 512
```

### Environment Variables
```bash
# Logging control
export VULKEN_LOG_LEVEL=INFO  # TRACE, DEBUG, INFO, WARN, ERROR

# Performance tuning
export VULKEN_HEADLESS=true   # Disable GUI for servers
export VULKAN_DRIVER=swiftshader  # Software rendering fallback

# Development options
export VULKEN_DEBUG=1         # Enable debug features
export VK_LAYER_PATH=/usr/share/vulkan/explicit_layer.d
```

## Development Workflow

### Building for Development
```bash
# Debug build with validation
cmake --preset linux-default -DCMAKE_BUILD_TYPE=Debug -DVALIDATION_LAYERS=ON
cmake --build build -j$(nproc)

# Run with validation
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
./build/apps/smoke_graphics_headless --vulkan-validation
```

### Testing Changes
```bash
# Run unit tests
ctest --test-dir build -R unit --output-on-failure

# Run integration tests
ctest --test-dir build -R integration --output-on-failure

# Run performance benchmarks
python3 scripts/bench/run_bench.py
```

### Hot-Reload Development
```bash
# Start operator console
./build/apps/operator_console &

# Edit shaders
vim shaders/core/voxel_mesher.comp

# Reload in console: Actions → Reload Shaders
```

## Common Operations

### Asset Management
```bash
# Load assets to Redis
python3 scripts/data/load_assets_to_redis.py --seed

# Validate asset integrity
python3 tools/validate_redis_integration.py

# Clear asset cache
redis-cli FLUSHALL  # If using Redis
rm -rf .cache/      # If using filesystem
```

### Performance Analysis
```bash
# Generate performance report
python3 scripts/bench/run_bench.py --output reports/bench/current.json

# Visualize results
python3 scripts/bench/visualize_results.py \
  --input reports/bench/current.json \
  --output performance_chart.png
```

### Debugging
```bash
# Enable verbose logging
./build/apps/smoke_graphics_headless --log-level DEBUG

# Run with profiler
perf record -g ./build/apps/smoke_graphics_headless --headless
perf report

# GPU profiling (NVIDIA)
nsys profile --trace=vulkan ./build/apps/smoke_graphics_headless
```

## Troubleshooting

### Common Issues

**"No Vulkan devices found"**
```bash
# Check Vulkan installation
vulkaninfo

# Update graphics drivers
# NVIDIA: Download from nvidia.com  
# AMD: sudo apt install mesa-vulkan-drivers
# Intel: sudo apt install intel-media-va-driver
```

**Build failures**
```bash
# Clean build directory
rm -rf build/
cmake --preset default
cmake --build build -j$(nproc)

# Check dependencies
vcpkg list
```

**Performance issues**
```bash
# Check GPU usage
nvidia-smi  # NVIDIA
radeontop   # AMD

# Monitor system resources  
htop

# Reduce settings
vim config/renderer.yaml  # Lower MSAA, disable post-processing
```

**Asset loading failures**
```bash
# Check Redis connectivity
redis-cli ping

# Reload assets
python3 scripts/data/load_assets_to_redis.py --seed

# Use filesystem fallback
export VULKEN_STORAGE_MODE=filesystem
```

### Log Analysis
```bash
# Filter for errors
grep ERROR logs/vulken3d.log

# Monitor frame times
grep "Frame time" logs/vulken3d.log | tail -20

# Check memory usage
grep "Memory" logs/vulken3d.log
```

### Getting Help
1. Check documentation in `docs/` directory
2. Review configuration options
3. Enable debug logging (`--log-level DEBUG`)
4. Check GitHub Issues for known problems
5. Run validation tests to identify specific failures

## Advanced Usage

### Custom Configuration
```bash
# Create custom config
cp config/engine.yaml config/my_config.yaml
vim config/my_config.yaml

# Run with custom config
./build/apps/smoke_graphics_headless --config config/my_config.yaml
```

### Multi-GPU Setup
```yaml
# config/engine.yaml
vulkan:
  device_selection: "discrete"  # Prefer discrete GPU
  enable_multi_gpu: true
  memory_budget_mb: 4096
```

### Network Deployment (Kubernetes)
```bash
# Deploy to Kubernetes
helm install vulken3d ./helm/vulken-3d \
  --set resources.limits."nvidia\.com/gpu"=1 \
  --set redis.enabled=true

# Monitor deployment
kubectl get pods -l app.kubernetes.io/name=vulken-3d
kubectl logs deployment/vulken3d -f
```

---

**Next Steps**: See `docs/ARCHITECTURE.md` for detailed engine information and `docs/OPERATOR_GUIDE.md` for advanced usage.