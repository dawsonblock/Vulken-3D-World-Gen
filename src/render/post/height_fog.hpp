#pragma once
#include <vulkan/vulkan.h>
namespace voxelvk {
struct HeightFogInputs { 
    VkImage scene; 
    VkImage depth; 
    VkImage out; 
    int w, h;
    // Camera matrices would be in UBO
};
void dispatch_height_fog(VkCommandBuffer cmd, const HeightFogInputs& in);
}