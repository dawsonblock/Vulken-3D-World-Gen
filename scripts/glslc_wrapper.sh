#!/bin/bash
# glslc compatibility wrapper using glslangValidator
# This converts glslc arguments to glslangValidator format

ARGS=()
OUTPUT=""
INPUT=""
STAGE=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -O)
            # Skip optimization flag - not supported by glslangValidator in same way
            shift
            ;;
        -o)
            OUTPUT="$2"
            shift 2
            ;;
        -fshader-stage=*)
            STAGE="${1#*=}"
            shift
            ;;
        *)
            if [[ "$1" =~ \.(vert|frag|comp|geom|tesc|tese|glsl)$ ]]; then
                INPUT="$1"
            else
                ARGS+=("$1")
            fi
            shift
            ;;
    esac
done

# Convert to glslangValidator
if [[ -n "$INPUT" && -n "$OUTPUT" ]]; then
    glslangValidator -V "$INPUT" -o "$OUTPUT" "${ARGS[@]}"
else
    echo "Usage: glslc -o output.spv input.shader"
    exit 1
fi
