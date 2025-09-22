#!/bin/bash
# Test execution script for Vulken-3D-World-Gen

set -e

# Default values
BUILD_DIR="build"
TEST_TYPE="all"
VERBOSE="false"
FILTER=""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --type)
            TEST_TYPE="$2"
            shift 2
            ;;
        --filter)
            FILTER="$2"
            shift 2
            ;;
        --verbose)
            VERBOSE="true"
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --build-dir DIR      Build directory (default: build)"
            echo "  --type TYPE          Test type: all, unit, graphics, integration (default: all)"
            echo "  --filter PATTERN     Filter tests by pattern"
            echo "  --verbose            Verbose output"
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

echo "Running tests for Vulken-3D-World-Gen"
echo "  Build directory: $BUILD_DIR"
echo "  Test type: $TEST_TYPE"
echo "  Filter: ${FILTER:-none}"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory '$BUILD_DIR' does not exist"
    echo "Please run the build script first"
    exit 1
fi

# Change to build directory
cd "$BUILD_DIR"

# Set up test command
CTEST_CMD="ctest"

if [ "$VERBOSE" = "true" ]; then
    CTEST_CMD="$CTEST_CMD --verbose"
fi

if [ -n "$FILTER" ]; then
    CTEST_CMD="$CTEST_CMD --output-on-failure -R $FILTER"
else
    CTEST_CMD="$CTEST_CMD --output-on-failure"
fi

# Run tests based on type
case $TEST_TYPE in
    all)
        echo "Running all tests..."
        $CTEST_CMD
        ;;
    unit)
        echo "Running unit tests..."
        $CTEST_CMD -R "test_mesher|test_staging|test_math"
        ;;
    graphics)
        echo "Running graphics tests..."
        $CTEST_CMD -R "test_staging|test_shader|test_swapchain"
        ;;
    integration)
        echo "Running integration tests..."
        $CTEST_CMD -R "test_headless|test_smoke"
        ;;
    *)
        echo "Invalid test type: $TEST_TYPE"
        echo "Valid types: all, unit, graphics, integration"
        exit 1
        ;;
esac

echo "Tests completed successfully!"
