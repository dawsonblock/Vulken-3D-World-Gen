#!/bin/bash
# Code formatting script for Vulken-3D-World-Gen

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Formatting Vulken-3D-World-Gen code...${NC}"

# Check if clang-format is available
if ! command -v clang-format &> /dev/null; then
    echo -e "${RED}Error: clang-format not found. Please install clang-format.${NC}"
    exit 1
fi

# Find all C++ source files
SOURCES=$(find . -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" | grep -v build | grep -v .git)

if [ -z "$SOURCES" ]; then
    echo -e "${YELLOW}No C++ source files found.${NC}"
    exit 0
fi

# Format files
echo "Formatting ${#SOURCES[@]} files..."
for file in $SOURCES; do
    echo "  Formatting: $file"
    clang-format -i "$file"
done

echo -e "${GREEN}Code formatting completed successfully!${NC}"
