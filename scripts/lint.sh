#!/bin/bash
# Linting script for Vulken-3D-World-Gen

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Running linters for Vulken-3D-World-Gen...${NC}"

# Check if clang-tidy is available
if ! command -v clang-tidy &> /dev/null; then
    echo -e "${YELLOW}Warning: clang-tidy not found. Skipping static analysis.${NC}"
else
    echo "Running clang-tidy..."
    # Find all C++ source files
    SOURCES=$(find . -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" | grep -v build | grep -v .git)

    for file in $SOURCES; do
        echo "  Analyzing: $file"
        clang-tidy "$file" -- -std=c++20 -I./src -I./external/third_party
    done
fi

# Check if cppcheck is available
if ! command -v cppcheck &> /dev/null; then
    echo -e "${YELLOW}Warning: cppcheck not found. Skipping static analysis.${NC}"
else
    echo "Running cppcheck..."
    cppcheck --enable=all --std=c++20 --suppress=missingIncludeSystem src/ apps/ tests/
fi

echo -e "${GREEN}Linting completed successfully!${NC}"
