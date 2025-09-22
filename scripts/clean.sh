#!/bin/bash
# Clean build artifacts script for Vulken-3D-World-Gen

set -e

# Default values
CLEAN_ALL="false"
CLEAN_SHADERS="false"
CLEAN_CACHE="false"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --all)
            CLEAN_ALL="true"
            shift
            ;;
        --shaders)
            CLEAN_SHADERS="true"
            shift
            ;;
        --cache)
            CLEAN_CACHE="true"
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --all       Clean all build artifacts (default)"
            echo "  --shaders   Clean only compiled shaders"
            echo "  --cache     Clean only CMake cache"
            echo "  --help      Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "Cleaning Vulken-3D-World-Gen build artifacts..."

# Clean build directories
if [ "$CLEAN_ALL" = "true" ] || [ "$CLEAN_CACHE" = "true" ]; then
    echo "Cleaning build directories..."
    rm -rf build*/
    rm -rf out/
    rm -rf bin/
    rm -rf dist/
    rm -rf obj/
    rm -rf CMakeFiles/
    rm -f CMakeCache.txt
    rm -f cmake_install.cmake
    rm -f CTestTestfile.cmake
    rm -f install_manifest.txt
    rm -rf vcpkg_installed/
    rm -rf _deps/
fi

# Clean shaders
if [ "$CLEAN_ALL" = "true" ] || [ "$CLEAN_SHADERS" = "true" ]; then
    echo "Cleaning compiled shaders..."
    find . -name "*.spv" -type f -delete
    rm -rf spv/
    rm -rf build/shaders/
    rm -rf .cache/spv/
fi

# Clean other artifacts
if [ "$CLEAN_ALL" = "true" ]; then
    echo "Cleaning other artifacts..."
    rm -rf .cache/
    rm -rf __pycache__/
    rm -f *.log
    rm -f *.nsys-rep
    rm -f *.qdrep
    rm -f imgui.ini
    rm -f screenshot.png
    rm -rf reports/
    rm -rf performance_report.json
    rm -rf redis_validation_report.json
    rm -f *.env
    rm -f .vcpkg-configuration.json
    rm -f vcpkg-configuration.json
    rm -rf .ccls-cache/
    rm -rf .ipynb_checkpoints/
    rm -rf .emergent/
    rm -f *.tmp
    rm -rf .venv/
    rm -rf *.egg-info/
    rm -rf pip-wheel-metadata/
    rm -rf coverage/
    rm -f *.gcda
    rm -f *.gcno
    rm -f *.lcov
fi

echo "Clean completed successfully!"
