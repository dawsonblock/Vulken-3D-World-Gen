#!/bin/bash
# Cross-platform build script for Vulken-3D-World-Gen

set -e

# Default values
BUILD_TYPE="headless"
ENABLE_GRAPHICS="false"
WARNINGS_AS_ERRORS="false"
CLEAN="false"
TESTS="true"
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "4")

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --graphics)
            ENABLE_GRAPHICS="true"
            shift
            ;;
        --warnings-as-errors)
            WARNINGS_AS_ERRORS="true"
            shift
            ;;
        --clean)
            CLEAN="true"
            shift
            ;;
        --no-tests)
            TESTS="false"
            shift
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --type TYPE          Build type: headless, graphics, debug, release (default: headless)"
            echo "  --graphics           Enable graphics support"
            echo "  --warnings-as-errors Treat warnings as errors"
            echo "  --clean              Clean build directory before building"
            echo "  --no-tests           Disable tests"
            echo "  --jobs N             Number of parallel jobs (default: auto-detect)"
            echo "  --help               Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Determine build directory and configuration
case $BUILD_TYPE in
    headless)
        BUILD_DIR="build_headless"
        CMAKE_BUILD_TYPE="RelWithDebInfo"
        ;;
    graphics)
        BUILD_DIR="build_graphics"
        CMAKE_BUILD_TYPE="RelWithDebInfo"
        ENABLE_GRAPHICS="true"
        ;;
    debug)
        BUILD_DIR="build_debug"
        CMAKE_BUILD_TYPE="Debug"
        WARNINGS_AS_ERRORS="false"
        ;;
    release)
        BUILD_DIR="build_release"
        CMAKE_BUILD_TYPE="Release"
        WARNINGS_AS_ERRORS="true"
        ;;
    *)
        echo "Invalid build type: $BUILD_TYPE"
        echo "Valid types: headless, graphics, debug, release"
        exit 1
        ;;
esac

echo "Building Vulken-3D-World-Gen"
echo "  Build type: $BUILD_TYPE"
echo "  Build directory: $BUILD_DIR"
echo "  CMake build type: $CMAKE_BUILD_TYPE"
echo "  Graphics enabled: $ENABLE_GRAPHICS"
echo "  Warnings as errors: $WARNINGS_AS_ERRORS"
echo "  Tests enabled: $TESTS"
echo "  Parallel jobs: $JOBS"

# Clean build directory if requested
if [ "$CLEAN" = "true" ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Configure with CMake
echo "Configuring with CMake..."
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DENABLE_GRAPHICS="$ENABLE_GRAPHICS" \
    -DWARNINGS_AS_ERRORS="$WARNINGS_AS_ERRORS" \
    -DENABLE_TESTS="$TESTS"

# Build
echo "Building..."
cmake --build "$BUILD_DIR" -j "$JOBS"

# Run tests if enabled
if [ "$TESTS" = "true" ]; then
    echo "Running tests..."
    cd "$BUILD_DIR"
    ctest --output-on-failure
    cd ..
fi

echo "Build completed successfully!"
echo "  Executables: $BUILD_DIR/bin/"
echo "  Libraries: $BUILD_DIR/lib/"
