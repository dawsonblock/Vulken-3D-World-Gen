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

## GPU/Driver Compatibility Matrix

### Supported GPUs

| Vendor | Series | Vulkan Support | Status | Notes |
|--------|--------|----------------|--------|-------|
| **NVIDIA** | RTX 40xx | 1.3 | ✅ Full | Recommended for best performance |
| **NVIDIA** | RTX 30xx | 1.3 | ✅ Full | Excellent performance |
| **NVIDIA** | RTX 20xx | 1.3 | ✅ Full | Good performance |
| **NVIDIA** | GTX 16xx | 1.3 | ✅ Full | Basic performance |
| **NVIDIA** | GTX 10xx | 1.2 | ⚠️ Limited | Some features disabled |
| **AMD** | RX 7000 | 1.3 | ✅ Full | Excellent performance |
| **AMD** | RX 6000 | 1.3 | ✅ Full | Good performance |
| **AMD** | RX 5000 | 1.3 | ✅ Full | Basic performance |
| **AMD** | RX 400/500 | 1.2 | ⚠️ Limited | Some features disabled |
| **Intel** | Arc A7xx | 1.3 | ✅ Full | Good performance |
| **Intel** | Arc A3xx | 1.3 | ✅ Full | Basic performance |
| **Intel** | Xe Graphics | 1.2 | ⚠️ Limited | Some features disabled |

### Driver Requirements

| Platform | Minimum Driver | Recommended Driver | Notes |
|----------|----------------|-------------------|-------|
| **Windows** | 472.12 (NVIDIA)<br/>21.11.2 (AMD)<br/>30.0.101.1404 (Intel) | Latest | Use DDU for clean installs |
| **Linux** | 470.57.02 (NVIDIA)<br/>21.40.1 (AMD)<br/>22.3.0 (Intel) | Latest | Use distro packages or official drivers |
| **Ubuntu** | nvidia-driver-470+<br/>mesa-vulkan-drivers | nvidia-driver-525+<br/>mesa-vulkan-drivers | Enable additional drivers |
| **Arch** | nvidia-470xx+<br/>mesa | nvidia<br/>mesa | Use AUR packages |

### Validation Layer Setup

#### Windows
```powershell
# Set environment variables
$env:VK_LAYER_PATH = "C:\VulkanSDK\1.3.250.0\Bin"
$env:VK_INSTANCE_LAYERS = "VK_LAYER_KHRONOS_validation"

# Verify installation
vulkaninfo
```

#### Linux
```bash
# Set environment variables
export VK_LAYER_PATH="/usr/share/vulkan/explicit_layer.d"
export VK_INSTANCE_LAYERS="VK_LAYER_KHRONOS_validation"

# Verify installation
vulkaninfo
```

## Troubleshooting

### Common Issues

**Vulkan SDK not found**
- Ensure `VULKAN_SDK` environment variable is set
- Verify `vulkaninfo` command works
- Check Vulkan drivers are installed
- **Windows**: Add Vulkan SDK to PATH
- **Linux**: Install `vulkan-tools` package

**Shader compilation fails**
- Ensure `glslc` or `glslangValidator` is in PATH  
- Check Vulkan SDK installation
- Verify shader syntax with `glslangValidator -V shader.frag`
- **Linux**: Install `glslang-tools` package
- **Windows**: Check Vulkan SDK installation

**vcpkg dependency issues**
- Set `VCPKG_ROOT` environment variable
- Ensure vcpkg manifest mode is working: `vcpkg integrate install`
- Check vcpkg.json syntax
- **Linux**: Install `pkg-config` and development packages
- **Windows**: Use Visual Studio Developer Command Prompt

**Performance warnings**
- Disable validation layers for performance builds
- Use Release or RelWithDebInfo build type
- Check GPU drivers are up to date
- **NVIDIA**: Use Game Ready drivers, not Studio drivers
- **AMD**: Use Adrenalin drivers, not Pro drivers

### Vulkan Error Debugging

#### VK_ERROR_DEVICE_LOST
```bash
# Enable validation layers
export VK_INSTANCE_LAYERS="VK_LAYER_KHRONOS_validation"
export VK_LAYER_ENABLES="VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT"

# Run with debug output
./build/apps/main_imgui_vulkan --debug --validation
```

#### VK_ERROR_OUT_OF_HOST_MEMORY
```bash
# Check system memory
free -h
# Check Vulkan memory usage
vulkaninfo --summary

# Enable memory debugging
export VK_LAYER_ENABLES="VK_VALIDATION_FEATURE_ENABLE_MEMORY_TRACKING_EXT"
```

#### VK_ERROR_INITIALIZATION_FAILED
```bash
# Check Vulkan installation
vulkaninfo
# Check GPU support
vulkaninfo --summary | grep -i device
# Check driver version
nvidia-smi  # or lspci -v for AMD/Intel
```

### Performance Issues

#### Low FPS / High Frame Times
1. **Check GPU utilization**: Use `nvidia-smi` or `radeontop`
2. **Disable validation layers**: Set `ENABLE_VALIDATION_LAYERS=OFF`
3. **Use Release build**: `CMAKE_BUILD_TYPE=Release`
4. **Update drivers**: Ensure latest GPU drivers
5. **Check thermal throttling**: Monitor GPU temperature

#### Memory Issues
1. **Enable VMA debugging**: Set `VMA_DEBUG_MARGIN=16`
2. **Check memory leaks**: Use AddressSanitizer (`VOXELVK_ENABLE_ASAN=ON`)
3. **Monitor memory usage**: Use `htop` or Task Manager
4. **Reduce texture quality**: Lower resolution textures

#### Shader Compilation Issues
1. **Check shader syntax**: Use `glslangValidator -V shader.frag`
2. **Verify SPIR-V output**: Use `spirv-dis` to disassemble
3. **Check shader stage**: Ensure correct stage flags
4. **Update shader tools**: Use latest Vulkan SDK

### Platform-Specific Issues

#### Windows
- **D3D12 conflicts**: Disable D3D12 in Windows settings
- **Antivirus interference**: Add build directory to exclusions
- **Power management**: Set to High Performance mode
- **Windows Defender**: Add exclusion for build directory

#### Linux
- **Mesa drivers**: Use `MESA_VK_DEVICE_SELECT=0` for discrete GPU
- **Wayland issues**: Use `GDK_BACKEND=x11` for X11
- **Permission issues**: Add user to `video` group
- **Missing libraries**: Install `libx11-dev libxrandr-dev`

#### Ubuntu/Debian
```bash
# Install missing dependencies
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
sudo apt install libvulkan1 mesa-vulkan-drivers vulkan-tools
sudo apt install vulkan-validationlayers-dev glslang-tools

# Add user to video group
sudo usermod -a -G video $USER
# Log out and back in
```

### Debug Builds

#### AddressSanitizer
```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DVOXELVK_ENABLE_ASAN=ON \
  -DENABLE_VALIDATION_LAYERS=ON
```

#### UndefinedBehaviorSanitizer
```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DVOXELVK_ENABLE_UBSAN=ON \
  -DENABLE_VALIDATION_LAYERS=ON
```

#### Memory Debugging
```bash
# Linux: Use Valgrind
valgrind --tool=memcheck --leak-check=full ./build/apps/main_imgui_vulkan

# Windows: Use Application Verifier
# Install from Windows SDK, enable for the executable
```

### Getting Help

1. **Check logs**: Look for error messages in console output
2. **Enable validation layers**: Use `VK_LAYER_KHRONOS_validation`
3. **Run diagnostics**: Use `vulkaninfo --summary`
4. **Check system requirements**: Ensure GPU and drivers are supported
5. **Report issues**: Include system info, driver versions, and error logs

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