#pragma once
#include <vulkan/vulkan.h>
namespace voxelvk {
struct TRPInputs { VkImage curr; VkImage history; VkImage out; int w,h; float alpha; float dt; };
void dispatch_temporal_accum(VkCommandBuffer cmd, const TRPInputs& in);
}