# Developer Guide — Vulken 3D World Gen

## Quick Start
```bash
cmake --preset default
cmake --build build/default -j
./build/default/apps/gui_fullscreen_demo
```

## Presets
- `default` (RelWithDebInfo)
- `ci-linux`, `ci-windows` (strict, hardened)

## Demos
- `gui_fullscreen_demo`, `vulkan_fullscreen_demo`, `smoke_headless`, `rl_nav_demo`
- Extras behind `-DBUILD_EXTRAS=ON`

## Tests
```bash
ctest --test-dir build/default --output-on-failure
```

## Security & Hardening
- `-D VULKEN_ENABLE_HARDENING=ON` → fortify, stack protector, RELRO/NOW
- ASAN/UBSAN in Debug

## Docker
```bash
docker build -t vulken:latest .
```

## Build Options

### Essential Demos (Default)
- `gui_fullscreen_demo` - ImGui-based GUI with Vulkan backend
- `vulkan_fullscreen_demo` - Pure Vulkan fullscreen demo
- `smoke_headless` - Headless smoke test
- `rl_nav_demo` - Reinforcement learning navigation demo

### Experimental Demos (BUILD_EXTRAS=ON)
- `p0_reliability_demo` - P0 reliability systems demo
- `p1_concepts_simple` - P1 memory management demo
- `p2_concepts_validator` - P2 performance concepts demo
- `operator_console` - Real-time monitoring and control interface

### Operator Console
```bash
# Basic usage
./build/default/apps/operator_console --config configs/operator_console.yaml

# Headless mode with timeout
./build/default/apps/operator_console --config configs/operator_console.yaml --timeout 2000 --headless

# Help
./build/default/apps/operator_console --help
```

## CI/CD

### Linux CI
- Uses `ci-linux` preset with hardening enabled
- Runs unit tests with `ctest`
- Operator console smoke test (2-second headless run)
- Artifacts: `linux-binaries`

### Windows CI
- Uses `ci-windows` preset with hardening enabled
- Runs unit tests with `ctest`
- Artifacts: `windows-binaries`

## Configuration

### vcpkg
- Pinned to specific baseline: `6f0e3b3aa9c4f5a3a2a6d2a7c88d0c3a8f1c7a92`
- Binary caching enabled via `.vcpkg-cache`
- Configuration: `vcpkg-configuration.json`

### CMake Options
- `VULKEN_ENABLE_WARNINGS_AS_ERRORS=ON` - Treat warnings as errors
- `VULKEN_ENABLE_HARDENING=ON` - Enable security hardening flags
- `BUILD_EXTRAS=OFF` - Gate experimental demos

## Development Workflow

1. **Local Development**
   ```bash
   cmake --preset default
   cmake --build build/default -j
   ctest --test-dir build/default --output-on-failure
   ```

2. **Testing Experimental Features**
   ```bash
   cmake -S . -B build/extras -G Ninja -DBUILD_EXTRAS=ON
   cmake --build build/extras -j
   ```

3. **CI Validation**
   ```bash
   cmake --preset ci-linux
   cmake --build build/ci-linux -j
   ctest --test-dir build/ci-linux --output-on-failure
   ./build/ci-linux/apps/operator_console --config configs/operator_console.yaml --timeout 2000 --headless
   ```

## Security Features

### Compiler Hardening
- Stack protector (`-fstack-protector-strong`)
- Fortify source (`-D_FORTIFY_SOURCE=2`)
- RELRO/NOW linking (`-Wl,-z,relro,-z,now`)

### Debug Instrumentation
- AddressSanitizer (`-fsanitize=address`)
- UndefinedBehaviorSanitizer (`-fsanitize=undefined`)

### Release Optimizations
- Link Time Optimization (LTO)
- Interprocedural Optimization (IPO)

## Troubleshooting

### Build Issues
- Ensure vcpkg is properly configured
- Check that all dependencies are installed
- Verify CMake preset is correct

### Test Failures
- Run tests with `--output-on-failure` for detailed output
- Check that test data files are present
- Verify test environment setup

### Docker Issues
- Ensure base image is available
- Check that build context includes all necessary files
- Verify runtime dependencies are installed