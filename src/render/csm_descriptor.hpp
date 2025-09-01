
#pragma once
#include <vulkan/vulkan.h>

namespace voxelvk {

inline void bind_csm_depth_array(VkDevice device, VkDescriptorSet dstSet, uint32_t binding,
                                 VkImageView depthArrayView, VkSampler sampler)
{
    VkDescriptorImageInfo ii{};
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ii.imageView   = depthArrayView;
    ii.sampler     = sampler;

    VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = dstSet;
    w.dstBinding = binding; // usually 5
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &ii;

    vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
}

} // namespace voxelvk
