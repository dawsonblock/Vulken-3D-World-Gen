# Build System Upgrade Guide

This guide covers the comprehensive build system upgrades implemented for Vulken-3D-World-Gen.

## Overview

The build system has been enhanced with modern CMake practices, improved dependency management, and comprehensive testing infrastructure.

## New Features

### 1. Enhanced CMake Presets

The project now includes comprehensive CMake presets for different build configurations:

- **vulkan-1.4**: Build with Vulkan 1.4+ features and modern extensions
- **performance-optimized**: Maximum performance build with PGO and LTO
- **development**: Development build with all debugging and analysis tools
- **headless**: Headless build without graphics dependencies
- **graphics-debug/release**: Graphics builds with different optimization levels

### 2. VCPkg Integration

- **vcpkg.json**: Manifest-based dependency management
- **Feature flags**: Optional features like Vulkan, CUDA, and Redis
- **Version constraints**: Pinned dependency versions for reproducible builds

### 3. Enhanced Build Scripts

New utility scripts in the `scripts/` directory:

- **format.sh**: Code formatting with clang-format
- **lint.sh**: Static analysis with clang-tidy and cppcheck
- **benchmark.sh**: Performance testing and benchmarking
- **package.sh**: Package creation for distribution

### 4. Improved Testing Infrastructure

- **Conditional compilation**: Tests only compile when dependencies are available
- **Graphics/headless separation**: Different test suites for different build modes
- **Comprehensive coverage**: Unit tests, integration tests, and performance tests

## Usage

### Building with Presets

```bash
# Configure with a preset
cmake --preset vulkan-1.4

# Build with a preset
cmake --build --preset vulkan-1.4

# Test with a preset
ctest --preset vulkan-1.4
```

### Using Build Scripts

```bash
# Format code
./scripts/format.sh

# Run linters
./scripts/lint.sh

# Run benchmarks
./scripts/benchmark.sh

# Create package
./scripts/package.sh
```

### VCPkg Integration

```bash
# Install dependencies
vcpkg install --triplet=x64-linux

# Install with features
vcpkg install --triplet=x64-linux --feature-flags=manifests
```

## Migration Guide

### From Old Build System

1. **Update CMake**: Ensure you're using CMake 3.25+
2. **Use presets**: Replace manual CMake configuration with presets
3. **Update dependencies**: Use VCPkg manifest for dependency management
4. **Run tests**: Use the new test infrastructure

### Configuration Changes

- **ENABLE_GRAPHICS**: Controls graphics vs headless builds
- **WARNINGS_AS_ERRORS**: Enables strict warning handling
- **VCPKG integration**: Automatic toolchain detection

## Troubleshooting

### Common Issues

1. **Shader compilation errors**: Ensure only actual shaders are compiled, not include files
2. **Missing dependencies**: Use VCPkg manifest for consistent dependency management
3. **Test failures**: Check that required dependencies are available

### Getting Help

- Check the build logs for detailed error messages
- Use the development preset for debugging
- Run linters to identify code quality issues

## Future Enhancements

- **Vulkan 1.4+ features**: Timeline semaphores, unified image layouts
- **Performance optimizations**: PGO, LTO, and advanced compiler flags
- **CI/CD improvements**: Enhanced GitHub Actions workflows
- **Cross-platform support**: Windows, macOS, and Linux optimizations
