
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

namespace voxelvk {

// Small helper to slot a sampler2DArray at `binding` (default 5) into your lighting set.
inline VkDescriptorSetLayoutBinding csm_shadow_binding(uint32_t binding = 5,
                                                       VkShaderStageFlags stages = VK_SHADER_STAGE_FRAGMENT_BIT)
{
    VkDescriptorSetLayoutBinding b{};
    b.binding = binding;
    b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b.descriptorCount = 1;
    b.stageFlags = stages;
    b.pImmutableSamplers = nullptr;
    return b;
}

// Optional: standalone descriptor set for shadows (use if you prefer a separate set)
struct CSMSet {
    VkDescriptorSetLayout layout{VK_NULL_HANDLE};
    VkDescriptorPool      pool{VK_NULL_HANDLE};
    VkDescriptorSet       set{VK_NULL_HANDLE};
};

bool create_csm_set(VkDevice device, uint32_t binding, CSMSet& out);
void update_csm_set(VkDevice device, CSMSet& out, uint32_t binding,
                    VkImageView depthArrayView, VkSampler sampler);
void destroy_csm_set(VkDevice device, CSMSet& out);

} // namespace voxelvk
