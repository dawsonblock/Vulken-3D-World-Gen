#!/usr/bin/env bash
set -e

echo "Testing shader compilation..."

# Check if glslc is available
if ! command -v glslc &> /dev/null; then
    echo "glslc not found, trying glslangValidator..."
    if ! command -v glslangValidator &> /dev/null; then
        echo "Neither glslc nor glslangValidator found. Skipping shader tests."
        exit 0
    fi
    VALIDATOR="glslangValidator -V"
else
    VALIDATOR="glslc"
fi

# Minimal shader compile smoke (adjust paths if different)
SHADER_ERRORS=0
TOTAL_SHADERS=0

# Test both shader directories
for shader_dir in shaders shaders_vk; do
    if [ -d "$shader_dir" ]; then
        echo "Testing shaders in $shader_dir/"
        
        while IFS= read -r -d '' shader; do
            TOTAL_SHADERS=$((TOTAL_SHADERS + 1))
            echo "  Compiling: $shader"
            
            if [ "$VALIDATOR" = "glslc" ]; then
                if ! glslc "$shader" -o "/tmp/$(basename "$shader").spv"; then
                    echo "    ERROR: Failed to compile $shader"
                    SHADER_ERRORS=$((SHADER_ERRORS + 1))
                fi
            else
                if ! glslangValidator -V "$shader" -o "/tmp/$(basename "$shader").spv"; then
                    echo "    ERROR: Failed to compile $shader" 
                    SHADER_ERRORS=$((SHADER_ERRORS + 1))
                fi
            fi
        done < <(find "$shader_dir" -type f \( -name "*.vert" -o -name "*.frag" -o -name "*.comp" -o -name "*.glsl" \) -print0)
    fi
done

echo ""
echo "Shader compilation summary:"
echo "  Total shaders tested: $TOTAL_SHADERS"
echo "  Compilation errors: $SHADER_ERRORS"

if [ $SHADER_ERRORS -eq 0 ]; then
    echo "✅ All shaders compiled successfully!"
    exit 0
else
    echo "❌ $SHADER_ERRORS shader(s) failed compilation"
    exit 1
fi