
#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h> // VMA allocator
#include <cstdint>

namespace voxelvk {

struct BRDFLUT {
    VkImage         image = VK_NULL_HANDLE;
    VkImageView     view  = VK_NULL_HANDLE;
    VkSampler       sampler = VK_NULL_HANDLE;
    VmaAllocation   allocation = VK_NULL_HANDLE;
    uint32_t        width = 0, height = 0;
    VkFormat        format = VK_FORMAT_R16G16_SFLOAT;
};

// Builds a split-sum BRDF LUT using a compute shader (ibl/brdf_lut.comp.spv).
// The resulting image is a sampled 2D RG16F texture (size x size).
// Requirements:
//  - Descriptor indexing NOT required; single storage image binding.
//  - Provide a one-time command buffer capable queue.
//  - 'allocator' must be valid VMA allocator for image memory.
bool CreateBRDFLUT(VkDevice device,
                   VkPhysicalDevice phys,
                   VmaAllocator allocator,
                   VkCommandPool cmdPool,
                   VkQueue queue,
                   BRDFLUT& out,
                   uint32_t size = 256);

// Destroys resources created by CreateBRDFLUT.
void DestroyBRDFLUT(VkDevice device, VmaAllocator allocator, BRDFLUT& lut);

} // namespace voxelvk
