# VoxelVK Build Guide

## Prerequisites

### System Requirements
- **OS**: Linux (Ubuntu 20.04+) or Windows 10/11
- **GPU**: Vulkan 1.3 compatible (NVIDIA RTX 20xx+ recommended)
- **RAM**: 8GB minimum, 16GB recommended
- **Storage**: 2GB for build artifacts and dependencies

### Required Software

#### Linux (Ubuntu/Debian)
```bash
# System packages
sudo apt update && sudo apt install -y \
  cmake ninja-build build-essential \
  libvulkan1 mesa-vulkan-drivers vulkan-tools vulkan-validationlayers-dev \
  glslang-dev libglfw3-dev libglm-dev libyaml-cpp-dev \
  libxkbcommon-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev

# Install Vulkan SDK (latest)
wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list https://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list
sudo apt update
sudo apt install vulkan-sdk
```

#### Windows
- **Vulkan SDK**: Download from [LunarG](https://vulkan.lunarg.com/)
- **Visual Studio 2022** with C++ development tools
- **CMake 3.24+** and **Ninja** (via Visual Studio or standalone)
- **vcpkg** (recommended for dependencies)

#### vcpkg (Optional but Recommended)
```bash
# Linux/macOS
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg

# Windows (PowerShell)
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
$env:VCPKG_ROOT="C:\vcpkg"
```

## Building

### Linux
```bash
# Clone repository
git clone <repository-url> voxelvk
cd voxelvk

# Configure with vcpkg (recommended)
export VCPKG_ROOT=~/vcpkg
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DENABLE_TESTS=ON \
  -DENABLE_IMGUI_OVERLAY=ON \
  -DVOXELVK_ENABLE_WEATHER=ON \
  -DVOXELVK_ENABLE_P0_RELIABILITY=ON \
  -DVOXELVK_ENABLE_P1_MEMORY=ON \
  -DVOXELVK_ENABLE_P2_PERFORMANCE=ON

# Build all targets
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure
```

### Windows (Developer PowerShell)
```powershell
# Clone repository
git clone <repository-url> voxelvk
cd voxelvk

# Configure with vcpkg
$env:VCPKG_ROOT="C:\vcpkg"
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  -DENABLE_TESTS=ON `
  -DENABLE_IMGUI_OVERLAY=ON `
  -DVOXELVK_ENABLE_WEATHER=ON `
  -DVOXELVK_ENABLE_P0_RELIABILITY=ON `
  -DVOXELVK_ENABLE_P1_MEMORY=ON `
  -DVOXELVK_ENABLE_P2_PERFORMANCE=ON

# Build all targets
cmake --build build -j

# Run tests
ctest --test-dir build --output-on-failure
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_TESTS` | ON | Build test suite |
| `ENABLE_VALIDATION_LAYERS` | ON | Enable Vulkan validation |
| `ENABLE_IMGUI_OVERLAY` | ON | Enable ImGui debug overlay |
| `VOXELVK_HEADLESS_ONLY` | OFF | Build headless targets only |
| `VOXELVK_ENABLE_WEATHER` | ON | Weather & sky systems |
| `VOXELVK_ENABLE_P0_RELIABILITY` | ON | Device recovery & error handling |
| `VOXELVK_ENABLE_P1_MEMORY` | ON | VMA & memory management |
| `VOXELVK_ENABLE_P2_PERFORMANCE` | ON | Frame pacing & TAA |
| `VOXELVK_ENABLE_CUDA` | OFF | CUDA acceleration |
| `VOXELVK_ENABLE_TENSORRT` | OFF | TensorRT AI inference |

### Quality Presets

#### Development Build
```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_VALIDATION_LAYERS=ON \
  -DENABLE_TESTS=ON
```

#### Performance Build  
```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_VALIDATION_LAYERS=OFF \
  -DENABLE_TESTS=OFF
```

#### Production Build
```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DENABLE_VALIDATION_LAYERS=OFF \
  -DENABLE_TESTS=OFF \
  -DVOXELVK_HEADLESS_ONLY=ON
```

## Verification

### Quick Smoke Test
```bash
# Basic engine functionality
./build/smoke_headless

# Weather system demo
./build/weather_demo

# Performance validation
./build/p2_concepts_validator
```

### Full Test Suite
```bash
# All tests
ctest --test-dir build --output-on-failure

# Specific test categories
ctest --test-dir build -R "test_weather"
ctest --test-dir build -R "test_rl" 
ctest --test-dir build -R "test_shaders"
```

### Performance Benchmarks
```bash
# Run performance validation
./scripts/p0_validation.sh
./scripts/p1_validation.sh
./scripts/p2_validation.sh

# Generate performance report
./build/VoxelVK_Elite_ALL --bench performance_report.json
```

## Troubleshooting

### Common Issues

**Vulkan SDK not found**
- Ensure `VULKAN_SDK` environment variable is set
- Verify `vulkaninfo` command works
- Check Vulkan drivers are installed

**Shader compilation fails**
- Ensure `glslc` or `glslangValidator` is in PATH  
- Check Vulkan SDK installation
- Verify shader syntax with `glslangValidator -V shader.frag`

**vcpkg dependency issues**
- Set `VCPKG_ROOT` environment variable
- Ensure vcpkg manifest mode is working: `vcpkg integrate install`
- Check vcpkg.json syntax

**Performance warnings**
- Disable validation layers for performance builds
- Use Release or RelWithDebInfo build type
- Check GPU drivers are up to date

### Build Artifacts

After successful build:
```
build/
├── .cache/spv/          # Compiled SPIR-V shaders
├── apps/                # Demo applications  
├── tests/               # Test executables
└── *.so / *.dll        # Engine libraries
```

### CI/Development Workflow

```bash
# Development cycle
cmake --build build -j && ctest --test-dir build && ./build/weather_demo

# Performance validation
cmake --build build -j && ./scripts/p2_validation.sh

# Full integration check  
cmake --build build -j && ./build/test_integration
```

For detailed usage instructions, see [RUN.md](RUN.md).