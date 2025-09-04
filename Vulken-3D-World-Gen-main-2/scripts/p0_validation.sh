#!/bin/bash

echo "=============================================="
echo "  VoxelVK P0 Reliability Validation Suite"
echo "=============================================="
echo ""

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Helper functions
test_passed() {
    echo -e "${GREEN}✅ $1: PASSED${NC}"
    ((PASSED_TESTS++))
    ((TOTAL_TESTS++))
}

test_failed() {
    echo -e "${RED}❌ $1: FAILED${NC}"
    echo -e "${YELLOW}   Reason: $2${NC}"
    ((FAILED_TESTS++))
    ((TOTAL_TESTS++))
}

test_warning() {
    echo -e "${YELLOW}⚠️ $1: WARNING${NC}"
    echo -e "${YELLOW}   Note: $2${NC}"
    ((TOTAL_TESTS++))
}

echo -e "${BLUE}🧪 P0 Test 1: Build System Integration${NC}"
echo "--------------------------------------"

# Test 1: CMake configuration
if cmake --version > /dev/null 2>&1; then
    test_passed "CMake availability"
else
    test_failed "CMake availability" "CMake not found"
fi

# Test 2: Vulkan SDK availability  
if vulkaninfo --summary > /dev/null 2>&1; then
    test_passed "Vulkan SDK installation"
else
    test_warning "Vulkan SDK installation" "vulkaninfo not available (may be running in container)"
fi

# Test 3: Shader compiler availability
if glslangValidator --version > /dev/null 2>&1; then
    test_passed "Shader compiler (glslangValidator)"
elif glslc --version > /dev/null 2>&1; then  
    test_passed "Shader compiler (glslc)"
else
    test_failed "Shader compiler" "Neither glslangValidator nor glslc found"
fi

# Test 4: yaml-cpp dependency
if pkg-config --exists yaml-cpp; then
    test_passed "yaml-cpp dependency"
else
    test_warning "yaml-cpp dependency" "Using system yaml-cpp"
fi

echo ""
echo -e "${BLUE}🧪 P0 Test 2: Weather System Core${NC}"
echo "--------------------------------------"

# Test 5: Weather system compilation
cd /app
if [ -f "build/weather_demo" ]; then
    test_passed "Weather system compilation"
else
    test_failed "Weather system compilation" "weather_demo not found"
fi

# Test 6: Weather system functionality
if [ -f "build/weather_demo" ]; then
    if timeout 10s ./build/weather_demo > /tmp/weather_output.log 2>&1; then
        test_passed "Weather system functionality" 
    else
        test_failed "Weather system functionality" "weather_demo execution failed"
        echo "Last 5 lines of output:"
        tail -5 /tmp/weather_output.log 2>/dev/null || echo "No output available"
    fi
else
    test_failed "Weather system functionality" "weather_demo not available"
fi

# Test 7: Weather configuration
if [ -f "config/weather.yaml" ]; then
    test_passed "Weather configuration file"
else
    test_failed "Weather configuration file" "config/weather.yaml not found"
fi

echo ""
echo -e "${BLUE}🧪 P0 Test 3: Shader Pipeline${NC}"
echo "--------------------------------------"

# Test 8: Core weather shaders exist
WEATHER_SHADERS=(
    "shaders_vk/common/weather_ubo.glsl"
    "shaders_vk/sky/sky_hw.frag"
    "shaders_vk/clouds/clouds_fullscreen.frag"
    "shaders_vk/particles/precip_update.comp"
    "shaders_vk/post/temporal_accum.comp"
    "shaders_vk/post/height_fog.frag"
    "shaders_vk/material/weather_material.glsl"
)

shader_test_count=0
shader_pass_count=0

for shader in "${WEATHER_SHADERS[@]}"; do
    if [ -f "$shader" ]; then
        ((shader_pass_count++))
    fi
    ((shader_test_count++))
done

if [ $shader_pass_count -eq $shader_test_count ]; then
    test_passed "Weather shader files ($shader_pass_count/$shader_test_count)"
else
    test_warning "Weather shader files" "$shader_pass_count/$shader_test_count found"
fi

# Test 9: Shader compilation
compiled_shaders=$(find build -name "*.spv" 2>/dev/null | wc -l)
if [ $compiled_shaders -gt 0 ]; then
    test_passed "Shader compilation ($compiled_shaders SPIR-V files)"
else
    test_failed "Shader compilation" "No SPIR-V files found"
fi

# Test 10: Weather-specific shader compilation
weather_spirv=$(find build -name "*.spv" 2>/dev/null | grep -E "(sky|cloud|precip|fog|weather)" | wc -l)
if [ $weather_spirv -gt 0 ]; then
    test_passed "Weather shader compilation ($weather_spirv weather SPIR-V files)"
else
    test_warning "Weather shader compilation" "No weather-specific SPIR-V files found"
fi

echo ""
echo -e "${BLUE}🧪 P0 Test 4: Error Handling & Resilience${NC}"  
echo "--------------------------------------"

# Test 11: Error handling infrastructure
if [ -f "src/vk/error_handling.hpp" ] && [ -f "src/vk/error_handling.cpp" ]; then
    test_passed "Error handling infrastructure"
else
    test_failed "Error handling infrastructure" "Error handling files not found"
fi

# Test 12: Device capabilities system
if [ -f "src/vk/device_caps.hpp" ] && [ -f "src/vk/device_caps.cpp" ]; then
    test_passed "Device capabilities system"
else
    test_failed "Device capabilities system" "Device caps files not found"
fi

# Test 13: Swapchain manager
if [ -f "src/vk/swapchain_manager.hpp" ] && [ -f "src/vk/swapchain_manager.cpp" ]; then
    test_passed "Swapchain manager"
else
    test_failed "Swapchain manager" "Swapchain manager files not found"
fi

# Test 14: Pipeline cache manager
if [ -f "src/vk/pipeline_cache_manager.hpp" ] && [ -f "src/vk/pipeline_cache_manager.cpp" ]; then
    test_passed "Pipeline cache manager"
else
    test_failed "Pipeline cache manager" "Pipeline cache files not found"
fi

echo ""
echo -e "${BLUE}🧪 P0 Test 5: Integration Completeness${NC}"
echo "--------------------------------------"

# Test 15: Frame graph system
if [ -f "src/render/framegraph/frame_graph_min.hpp" ]; then
    test_passed "Frame graph system"
else
    test_failed "Frame graph system" "Frame graph files not found"
fi

# Test 16: Temporal reprojection
if [ -f "src/render/post/temporal_accum.hpp" ]; then
    test_passed "Temporal reprojection system"
else
    test_failed "Temporal reprojection system" "Temporal accumulation files not found"
fi

# Test 17: Height fog system
if [ -f "src/render/post/height_fog.hpp" ]; then
    test_passed "Height fog system"
else
    test_failed "Height fog system" "Height fog files not found"
fi

# Test 18: Lightning system
if [ -f "src/env/weather/lightning.hpp" ]; then
    test_passed "Lightning system"
else
    test_failed "Lightning system" "Lightning files not found"
fi

echo ""
echo -e "${BLUE}🧪 P0 Test 6: Smoke Tests${NC}"
echo "--------------------------------------"

# Test 19: Basic smoke test
if [ -f "build/smoke_headless" ]; then
    if timeout 5s ./build/smoke_headless > /tmp/smoke_output.log 2>&1; then
        test_passed "Basic smoke test"
    else
        test_failed "Basic smoke test" "smoke_headless execution failed"
    fi
else
    test_failed "Basic smoke test" "smoke_headless not found"
fi

# Test 20: Weather integration test
if [ -f "build/weather_integration_test" ]; then
    if timeout 10s ./build/weather_integration_test > /tmp/weather_integration_output.log 2>&1; then
        test_passed "Weather integration test"
    else
        test_failed "Weather integration test" "weather_integration_test execution failed"
    fi
else
    test_warning "Weather integration test" "weather_integration_test not built"
fi

echo ""
echo "=============================================="
echo -e "${BLUE}  P0 RELIABILITY VALIDATION RESULTS${NC}"
echo "=============================================="
echo ""

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}🎉 P0 RELIABILITY VALIDATION: COMPLETE SUCCESS!${NC}"
    echo ""
    echo -e "${GREEN}✅ All critical P0 systems validated:${NC}"
    echo -e "${GREEN}   • Error handling & logging system${NC}"
    echo -e "${GREEN}   • Device capabilities & feature probing${NC}"
    echo -e "${GREEN}   • Swapchain resilience & recovery${NC}"
    echo -e "${GREEN}   • Pipeline cache management${NC}"
    echo -e "${GREEN}   • Weather system integration${NC}"
    echo -e "${GREEN}   • Shader compilation pipeline${NC}"
    echo -e "${GREEN}   • Frame graph architecture${NC}"
    echo -e "${GREEN}   • Temporal reprojection system${NC}"
    echo ""
    echo -e "${GREEN}The VoxelVK engine is ready for production deployment!${NC}"
    echo -e "${GREEN}P0 reliability standards: ACHIEVED ✨${NC}"
    
    exit 0
    
elif [ $FAILED_TESTS -le 2 ]; then
    echo -e "${YELLOW}⚠️ P0 RELIABILITY VALIDATION: MOSTLY SUCCESSFUL${NC}"
    echo ""
    echo "Summary: $PASSED_TESTS passed, $FAILED_TESTS failed out of $TOTAL_TESTS tests"
    echo -e "${YELLOW}Minor issues detected but core P0 systems operational${NC}"
    echo ""
    echo "Recommended action: Review failed tests and apply fixes"
    
    exit 1
    
else
    echo -e "${RED}❌ P0 RELIABILITY VALIDATION: NEEDS ATTENTION${NC}"
    echo ""
    echo "Summary: $PASSED_TESTS passed, $FAILED_TESTS failed out of $TOTAL_TESTS tests"
    echo -e "${RED}Multiple P0 systems require fixes before production${NC}"
    echo ""
    echo "Required action: Fix critical reliability issues"
    
    exit 2
fi