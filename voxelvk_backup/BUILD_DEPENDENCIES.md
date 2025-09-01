# Build Dependencies Guide

This document lists all the dependencies required to build the 3D-WORLD project successfully.

## Required System Dependencies

### Ubuntu/Debian
```bash
# Update package list
sudo apt update

# Install core build tools
sudo apt install -y cmake ninja-build build-essential

# Install Vulkan SDK and development libraries
sudo apt install -y libvulkan-dev vulkan-tools glslang-tools spirv-tools

# Install graphics libraries
sudo apt install -y libglfw3-dev libglm-dev

# Install Vulkan Memory Allocator
sudo apt install -y libvulkan-memory-allocator-dev

# Optional: CUDA development (if available)
# sudo apt install -y nvidia-cuda-toolkit
```

### Package Descriptions

- **libvulkan-dev**: Vulkan loader library development files
- **vulkan-tools**: Vulkan utilities and validation layers
- **glslang-tools**: Shader compilation tools (glslangValidator)
- **spirv-tools**: SPIR-V binary utilities
- **libglfw3-dev**: OpenGL framework development files
- **libglm-dev**: OpenGL Mathematics library headers
- **libvulkan-memory-allocator-dev**: VMA development package

## Build Presets

The project supports multiple build configurations:

### Default Build
```bash
./scripts/build.sh --preset default
```
- Build Type: RelWithDebInfo
- Features: Full Vulkan support, ImGui overlay, validation layers
- Requirements: All dependencies above

### Debug Build
```bash
./scripts/build.sh --preset debug
```
- Build Type: Debug
- Features: Full debugging support, validation layers enabled
- Requirements: All dependencies above

### Release Build
```bash
./scripts/build.sh --preset release
```
- Build Type: Release with LTO
- Features: Optimized build, validation layers disabled
- Requirements: All dependencies above

### Headless Build
```bash
./scripts/build.sh --preset headless
```
- Build Type: RelWithDebInfo
- Features: Server/headless mode without Vulkan/GLFW
- Requirements: Only core build tools (cmake, ninja, gcc)

## Troubleshooting

### Common Issues

1. **Vulkan SDK not found**
   - Install `libvulkan-dev` package
   - Verify installation: `pkg-config --exists vulkan`

2. **Shader compiler not found**
   - Install `glslang-tools` and `spirv-tools`
   - Verify: `which glslangValidator`

3. **VMA header not found**
   - Install `libvulkan-memory-allocator-dev`
   - Verify: `ls /usr/include/vk_mem_alloc.h`

4. **GLM not found**
   - Install `libglm-dev`
   - Verify: `ls /usr/include/glm/`

### Build Options

- `--no-cuda`: Disable CUDA support
- `--no-lto`: Disable Link Time Optimization
- `--no-tests`: Disable test building
- `--jobs N`: Set parallel job count

### Verification

After successful build, verify executables:
```bash
# Test basic functionality
./build/smoke_headless

# Run tests
ctest --test-dir build --output-on-failure
```

## Build Artifacts

Successful builds create:
- `VoxelVK_Elite_ALL`: Main application executable
- `smoke_headless`: Headless test application
- `spv/`: Compiled shader SPIR-V binaries
- Test executables and validation results