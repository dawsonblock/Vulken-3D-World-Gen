# Vulken-3D Testing Guide
=========================

## Testing Infrastructure Overview

The Vulken-3D engine employs a comprehensive testing strategy with multiple layers of validation to ensure production reliability and performance.

## Test Categories

### 1. Unit Tests (`tests/unit/`)

**Core Mathematics (`test_voxel_math.cpp`)**
- Coordinate system conversions (world ↔ chunk ↔ voxel)
- Boundary condition handling  
- Distance calculations and neighbor finding
- Performance validation (10k conversions < 10ms)

```bash
# Run voxel math tests
ctest -R voxel_math --verbose
```

**Mesher Algorithms (`test_mesher_core.cpp`)**  
- Naive meshing validation
- Greedy meshing optimization
- Material consistency
- Vertex/index correctness
- Performance benchmarks

```bash
# Run meshing tests
ctest -R mesher_core --verbose
```

**Device Capabilities (`test_device_caps.cpp`)**
- Vulkan feature detection
- Memory type validation
- Queue family support
- Extension availability

**Persistence (`test_persistence.cpp`)**
- World data serialization
- Asset loading/saving
- Cache consistency

### 2. Integration Tests (`tests/integration/`)

**Render Smoke Testing (`test_render_smoke.cpp`)**
- Headless rendering pipeline validation
- Frame hash regression testing
- Multi-resolution support
- Deterministic rendering verification

```bash
# Run render integration tests
ctest -R render_smoke --verbose
```

**Weather System (`test_weather_cycle.cpp`)**
- All weather type parameter validation
- State transition verification  
- Seasonal variation testing
- Long-term stability (95%+ validity rate)

```bash
# Run weather integration tests  
ctest -R weather_cycle --verbose
```

### 3. Shader Validation (`tests/unit/test_shaders_compile.cpp`)

**Compilation Testing**
- GLSL syntax validation
- SPIR-V output verification
- Layout consistency checking
- Compute shader functionality

```bash
# Run shader tests
ctest -R shaders --verbose
```

**Layout Validation**
```bash
# Validate shader layouts against C++ structs
python3 scripts/shaders/validate_layout.py \
  --shader-dirs build/shaders_cache \
  --cpp-dirs src include \
  --report reports/shaders/layout_check.json
```

### 4. Performance Benchmarks (`tests/bench/`)

**Benchmarking Framework**
- Google Benchmark integration
- JSON output for regression analysis
- Automated comparison against baselines
- Performance gates with ±10% tolerance

```bash
# Run all benchmarks
python3 scripts/bench/run_bench.py --output reports/bench/latest.json

# Check for regressions
python3 scripts/bench/check_regression.py \
  --current reports/bench/latest.json \
  --baseline reports/bench/baseline.json \
  --threshold 0.10
```

## Running Tests

### Complete Test Suite
```bash
# Configure project
cmake --preset linux-default

# Build with tests
cmake --build build --target all -j$(nproc)

# Run all tests
ctest --test-dir build --output-on-failure --parallel 4
```

### Selective Testing
```bash
# Unit tests only
ctest --test-dir build -R "unit" --output-on-failure

# Integration tests only  
ctest --test-dir build -R "integration" --output-on-failure

# Specific test patterns
ctest --test-dir build -R "voxel|mesh" --verbose
```

### Headless Testing
```bash
# For CI/CD environments without GPU
export VULKAN_DRIVER=swiftshader
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/VkICD_mock_icd.json

ctest --test-dir build --output-on-failure
```

## Performance Thresholds

### Frame Rate Targets
- **Headless Rendering**: >100 FPS (lightweight scenes)
- **Full Rendering**: >60 FPS (normal complexity)  
- **Complex Scenes**: >30 FPS (high complexity + effects)

### Memory Usage Limits
- **GPU Memory**: <4GB for recommended hardware
- **System Memory**: <8GB total engine usage
- **Cache Memory**: <1GB for asset caching

### Algorithm Performance
- **Coordinate Conversion**: <1μs per conversion
- **Naive Meshing**: <10ms per 64³ chunk
- **Greedy Meshing**: <50ms per 64³ chunk  
- **Shader Compilation**: <5s for full shader set

### Regression Tolerance
- **CPU Performance**: ±10% compared to baseline
- **GPU Performance**: ±15% (driver variations)
- **Memory Usage**: ±5% increase maximum

## Test Data Management

### Golden Reference Files (`tests/golden/`)

**Frame Hashes**
- Deterministic rendering validation
- Regression detection for visual changes
- Platform-specific golden references

**Sample Assets**
- Minimal test textures and meshes
- Validation voxel chunks
- Reference configuration files

### Test Asset Generation
```bash
# Generate test assets
python3 scripts/data/load_assets_to_redis.py --seed --assets-dir assets

# Create golden reference frames
./build/apps/smoke_graphics_headless --generate-golden --output tests/golden/
```

## Continuous Integration

### GitHub Actions Integration

The CI pipeline runs all tests automatically:

```yaml
# .github/workflows/build.yml
- name: Run tests
  run: ctest --test-dir build --output-on-failure --parallel 4
```

**Workflows:**
- **build.yml**: Complete build + test on Ubuntu/Windows  
- **shader-validate.yml**: Shader compilation and validation
- **lint.yml**: Code quality and formatting
- **bench.yml**: Nightly performance benchmarks

### Test Execution Matrix

| Platform | Build Type | Test Suite | Duration |
|----------|------------|------------|----------|
| Ubuntu 22.04 | Debug | Full | ~8 min |
| Ubuntu 22.04 | RelWithDebInfo | Full + Bench | ~12 min |
| Windows 2022 | Debug | Core | ~10 min |  
| Windows 2022 | Release | Core + Smoke | ~8 min |

## Debugging Failed Tests

### Common Test Failures

**Vulkan Initialization Failures**
```bash
# Check Vulkan driver
vulkaninfo

# Enable validation layers
export VK_LAYER_PATH=/usr/share/vulkan/explicit_layer.d
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation

# Run with validation
./build/apps/smoke_graphics_headless --vulkan-validation
```

**Frame Hash Mismatches**
```bash
# Regenerate golden reference
./build/apps/smoke_graphics_headless --generate-golden --force

# Compare frame differences
python3 tools/image_diff.py tests/golden/reference.png tests/golden/current.png
```

**Performance Regression Failures**  
```bash
# Check system load
top, htop, nvidia-smi

# Run benchmarks in isolation
./build/tests/bench_mesher --benchmark_filter=VoxelMeshing

# Reset performance baseline
cp reports/bench/latest.json reports/bench/baseline.json
```

### Test Environment Setup

**Development Environment**
```bash
# Install test dependencies
pip3 install pytest gtest numpy matplotlib

# Setup test data
mkdir -p tests/golden tests/temp
python3 scripts/data/load_assets_to_redis.py --seed
```

**CI Environment** 
```bash
# Install CI-specific dependencies
apt-get install xvfb  # Virtual display
pip3 install pytest-xdist  # Parallel testing

# Run with virtual display
xvfb-run -a ctest --test-dir build
```

### Memory Testing

**Leak Detection**
```bash
# Valgrind memory testing
valgrind --leak-check=full --track-origins=yes \
  ./build/apps/smoke_graphics_headless --headless

# AddressSanitizer (if built with -fsanitize=address)
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
./build/apps/smoke_graphics_headless --headless
```

**GPU Memory Testing**
```bash
# Monitor GPU memory during tests
nvidia-smi --query-gpu=memory.used --format=csv -l 1 &
ctest --test-dir build -R render
```

## Test Coverage Analysis

### Coverage Collection
```bash
# Build with coverage flags
cmake --preset linux-default -DCMAKE_CXX_FLAGS="--coverage"
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build

# Generate coverage report
gcov build/CMakeFiles/**/*.gcno
lcov --capture --directory build --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

### Coverage Targets
- **Unit Tests**: >80% line coverage for core modules
- **Integration Tests**: >60% system coverage
- **Critical Paths**: 100% coverage for safety-critical code

## Performance Analysis

### Benchmark Visualization
```bash
# Generate performance charts
python3 scripts/bench/visualize_results.py \
  --input reports/bench/latest.json \
  --output reports/bench/performance_chart.png

# Compare with historical data
python3 scripts/bench/trend_analysis.py \
  --history reports/bench/ \
  --days 30 \
  --output reports/bench/trends.html
```

### Profiling Integration
```bash
# CPU profiling
perf record -g ctest --test-dir build
perf report --stdio > reports/perf_analysis.txt

# GPU profiling (NVIDIA)
nsys profile --trace=vulkan,cuda ./build/apps/smoke_graphics_headless
nsight-sys reports/profile.qdrep
```

## Test Automation

### Pre-commit Hooks
```bash
# Install pre-commit
pip install pre-commit
pre-commit install

# Manual run
pre-commit run --all-files
```

### Automated Test Triggers
- **Push to main/develop**: Full test suite
- **Pull requests**: Core tests + affected components  
- **Nightly**: Extended benchmarks + memory tests
- **Release tags**: Complete validation suite

## Quality Gates

### Merge Requirements
- [ ] All unit tests pass
- [ ] All integration tests pass  
- [ ] Shader validation passes
- [ ] No performance regressions >10%
- [ ] Code coverage maintains threshold
- [ ] Linting checks pass

### Release Requirements  
- [ ] All quality gates pass
- [ ] Extended benchmark suite passes
- [ ] Memory leak tests pass
- [ ] Multi-platform validation
- [ ] Documentation updated
- [ ] Security scan passes

## Test Maintenance

### Adding New Tests
```cpp
// Unit test template
TEST_F(MyComponentTest, FeatureName) {
    // Arrange
    MyComponent component(testConfig);
    
    // Act  
    auto result = component.performOperation();
    
    // Assert
    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.getValue(), expectedValue);
}
```

### Updating Golden References
```bash
# When intentional visual changes are made
./build/apps/smoke_graphics_headless --update-golden
git add tests/golden/ && git commit -m "Update golden references"
```

### Performance Baseline Management
```bash
# Update performance baseline after optimization
cp reports/bench/latest.json reports/bench/baseline.json
git add reports/bench/baseline.json && git commit -m "Update performance baseline"
```

For detailed test implementation examples, see the test files in `tests/unit/` and `tests/integration/`.