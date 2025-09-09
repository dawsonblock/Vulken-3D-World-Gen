#!/bin/bash
# Simple glslc wrapper that handles the specific cases for VoxelVK

OUTPUT=""
INPUT=""
OPTIMIZATION=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -O)
            OPTIMIZATION="-Os"  # Use size optimization for glslangValidator
            shift
            ;;
        -o)
            OUTPUT="$2"
            shift 2
            ;;
        *.vert|*.frag|*.comp|*.geom|*.tesc|*.tese|*.glsl)
            INPUT="$1"
            shift
            ;;
        *)
            shift
            ;;
    esac
done

# Run glslangValidator with proper arguments
if [[ -n "$INPUT" && -n "$OUTPUT" ]]; then
    glslangValidator -V $OPTIMIZATION "$INPUT" -o "$OUTPUT"
else
    echo "Error: Missing input or output file"
    exit 1
fi
