#!/usr/bin/env bash
set -euo pipefail

echo "=== VoxelRL_All Build Script ==="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Configuration
BUILD_TYPE=${BUILD_TYPE:-RelWithDebInfo}
PRESET=${PRESET:-default}
PARALLEL_JOBS=${PARALLEL_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}
ENABLE_LTO=${ENABLE_LTO:-ON}
ENABLE_CUDA=${ENABLE_CUDA:-ON}
ENABLE_TESTS=${ENABLE_TESTS:-ON}
BUILD_TARGET=${BUILD_TARGET:-}
GENERATOR=""
BUILD_DIR=""

echo "Project root: $PROJECT_ROOT"
echo "Build type: $BUILD_TYPE"
echo "Preset: $PRESET"
echo "Parallel jobs: $PARALLEL_JOBS"
echo "Enable LTO: $ENABLE_LTO"
echo "Enable CUDA: $ENABLE_CUDA"
echo "Enable tests: $ENABLE_TESTS"

# Load environment if setup script was run
if [[ -f "$PROJECT_ROOT/setup_env.sh" ]]; then
    echo "Loading environment..."
    source "$PROJECT_ROOT/setup_env.sh"
fi

# Verify dependencies
check_dependencies() {
    echo "Checking dependencies..."
    local missing_deps=0
    
    # Check CMake
    if ! command -v cmake &> /dev/null; then
        echo "Error: CMake not found. Install with: sudo apt install cmake"
        missing_deps=1
    fi
    
    # Check Ninja
    if ! command -v ninja &> /dev/null; then
        echo "Warning: Ninja not found, using make instead"
        echo "  To install Ninja: sudo apt install ninja-build"
        GENERATOR="Unix Makefiles"
    else
        GENERATOR="Ninja"
    fi
    
    # Check CUDA if enabled
    if [[ "$ENABLE_CUDA" == "ON" ]] && ! command -v nvcc &> /dev/null; then
        echo "Warning: CUDA not found, disabling CUDA support"
        echo "  To install CUDA: sudo apt install nvidia-cuda-toolkit"
        ENABLE_CUDA=OFF
    fi
    
    # Enhanced Vulkan checking
    if [[ -z "${VULKAN_SDK:-}" ]]; then
        if ! pkg-config --exists vulkan; then
            echo "Error: Vulkan development libraries not found"
            echo "  Install with: sudo apt install libvulkan-dev vulkan-tools"
            missing_deps=1
        fi
        
        # Shader compilers optional for this project
        if ! command -v glslangValidator &> /dev/null && ! command -v glslc &> /dev/null; then
            echo "Warning: Vulkan shader compiler not found (optional)"
            echo "  To install: sudo apt install glslang-tools"
        fi
    fi
    
    # Check for required graphics libraries
    if ! pkg-config --exists glfw3; then
        echo "Warning: GLFW3 not found (required for GUI builds)"
        echo "  Install with: sudo apt install libglfw3-dev"
    fi
    
    # GLM is not required by this scaffold
    # if [[ ! -f "/usr/include/glm/glm.hpp" ]] && [[ ! -f "/usr/local/include/glm/glm.hpp" ]]; then
    #     echo "Warning: GLM library not found (not required)"
    # fi
    
    # VMA optional
    if [[ ! -f "/usr/include/vk_mem_alloc.h" ]] && [[ ! -f "/usr/local/include/vk_mem_alloc.h" ]]; then
        echo "Warning: VulkanMemoryAllocator not found (optional)"
    fi
    
    if [[ $missing_deps -eq 1 ]]; then
        echo ""
        echo "Critical dependencies are missing. Please install them before building."
        echo "See BUILD_DEPENDENCIES.md for detailed installation instructions."
        exit 1
    fi
    
    echo "Dependencies checked"
    echo "Generator: $GENERATOR"
}

# Determine build directory (shared across steps)
compute_build_dir() {
    if [[ -f "$PROJECT_ROOT/CMakePresets.json" ]]; then
        case "$PRESET" in
            debug|Debug) BUILD_DIR="$PROJECT_ROOT/build_debug" ;;
            release|Release) BUILD_DIR="$PROJECT_ROOT/build_release" ;;
            headless) BUILD_DIR="$PROJECT_ROOT/build_headless" ;;
            *) BUILD_DIR="$PROJECT_ROOT/build" ;;
        esac
    else
        BUILD_DIR="$PROJECT_ROOT/build"
    fi
}

# Configure build
configure_build() {
    echo "Configuring build..."
    
    cd "$PROJECT_ROOT"
    compute_build_dir
    
    # Use preset if available, otherwise manual configuration
    if [[ -f "CMakePresets.json" ]]; then
        echo "Using CMake preset: $PRESET"
        cmake --preset="$PRESET"
    else
        echo "Manual CMake configuration"

        # Additional CMake flags
        CMAKE_FLAGS=(
            -G "$GENERATOR"
            -S .
            -B "$BUILD_DIR"
            -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
            -DENABLE_TESTS="$ENABLE_TESTS"
            -DVOXELVK_ENABLE_CUDA="$ENABLE_CUDA"
            -DENABLE_IMGUI_OVERLAY=ON
            -DENABLE_VALIDATION_LAYERS=ON
            -DENABLE_VK_DEBUG_MARKERS=ON
        )
        
        # Add LTO for release builds
        if [[ "$BUILD_TYPE" == "Release" && "$ENABLE_LTO" == "ON" ]]; then
            CMAKE_FLAGS+=(-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON)
        fi
        
        # Add vcpkg toolchain if available
        if [[ -n "${VCPKG_ROOT:-}" && -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]]; then
            CMAKE_FLAGS+=(-DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        fi
        
        # Add LibTorch if available
        if [[ -n "${LIBTORCH_ROOT:-}" && -d "${LIBTORCH_ROOT}" ]]; then
            CMAKE_FLAGS+=(-DCMAKE_PREFIX_PATH="${LIBTORCH_ROOT}")
        fi
        
    cmake "${CMAKE_FLAGS[@]}"
    fi
    
    echo "Configuration complete"
}

# Build project
build_project() {
    echo "Building project..."
    
    cd "$PROJECT_ROOT"
    
    # Ensure BUILD_DIR set
    compute_build_dir
    
    echo "Building in directory: $BUILD_DIR"
    
    if [[ -n "$BUILD_TARGET" ]]; then
        echo "Building target: $BUILD_TARGET"
    fi
    if [[ "$GENERATOR" == "Ninja" ]]; then
        if [[ -n "$BUILD_TARGET" ]]; then
            cmake --build "$BUILD_DIR" --parallel "$PARALLEL_JOBS" --target "$BUILD_TARGET"
        else
            cmake --build "$BUILD_DIR" --parallel "$PARALLEL_JOBS"
        fi
    else
        if [[ -n "$BUILD_TARGET" ]]; then
            cmake --build "$BUILD_DIR" --target "$BUILD_TARGET" -- -j"$PARALLEL_JOBS"
        else
            cmake --build "$BUILD_DIR" -- -j"$PARALLEL_JOBS"
        fi
    fi
    
    echo "Build complete"
}

# Run tests
run_tests() {
    if [[ "$ENABLE_TESTS" == "ON" ]]; then
        echo "Running tests..."
        
        cd "$PROJECT_ROOT"
        
        compute_build_dir
        if [[ -f "CMakePresets.json" ]]; then
            ctest --preset="$PRESET" --output-on-failure
        else
            ctest --test-dir "$BUILD_DIR" --output-on-failure
        fi
        
        echo "Tests complete"
    else
        echo "Tests disabled"
    fi
}

# Print build summary
print_summary() {
    echo ""
    echo "=== Build Summary ==="
    echo "Build type: $BUILD_TYPE"
    echo "Preset: $PRESET"
    echo "Generator: $GENERATOR"
    echo "CUDA enabled: $ENABLE_CUDA"
    echo "LTO enabled: $ENABLE_LTO"
    echo "Tests enabled: $ENABLE_TESTS"
    compute_build_dir
    echo "Build directory: $BUILD_DIR"
    echo ""
    
    if [[ -d "$BUILD_DIR" ]]; then
        echo "Built executables:"
        # Tolerate no matches to avoid failing under 'set -euo pipefail'
        find "$BUILD_DIR" -type f -executable -name "*" | grep -E "(VoxelRL|Trainer|Evaluate|Preview)" | head -10 || true
    fi
    
    echo "====================="
}

# Main execution
main() {
    check_dependencies
    configure_build
    build_project
    run_tests
    print_summary
}

# Handle command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --config)
            # Map CMake multi-config style to BUILD_TYPE
            BUILD_TYPE="$2"
            shift 2
            ;;
        --preset)
            PRESET="$2"
            shift 2
            ;;
        --build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --target)
            BUILD_TARGET="$2"
            shift 2
            ;;
        --no-cuda)
            ENABLE_CUDA=OFF
            shift
            ;;
        --no-lto)
            ENABLE_LTO=OFF
            shift
            ;;
        --no-tests)
            ENABLE_TESTS=OFF
            shift
            ;;
        --jobs)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --config TYPE      Same as --build-type (Debug|Release|RelWithDebInfo)"
            echo "  --preset NAME      Use CMake preset (default: default)"
            echo "  --build-type TYPE  Build type (Debug|Release|RelWithDebInfo)"
            echo "  --target NAME      Build a specific target"
            echo "  --no-cuda          Disable CUDA support"
            echo "  --no-lto           Disable Link Time Optimization"
            echo "  --no-tests         Disable tests"
            echo "  --jobs N           Number of parallel jobs"
            echo "  --help             Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

main "$@"
