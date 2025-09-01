
#include "csm_layout.hpp"
#include <vector>

namespace voxelvk {

bool create_csm_set(VkDevice device, uint32_t binding, CSMSet& out)
{
    // layout
    VkDescriptorSetLayoutBinding b = csm_shadow_binding(binding);
    VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    lci.bindingCount = 1;
    lci.pBindings = &b;
    if (vkCreateDescriptorSetLayout(device, &lci, nullptr, &out.layout) != VK_SUCCESS) return false;

    // pool
    VkDescriptorPoolSize ps{};
    ps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ps.descriptorCount = 1;

    VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pci.maxSets = 1;
    pci.poolSizeCount = 1;
    pci.pPoolSizes = &ps;
    if (vkCreateDescriptorPool(device, &pci, nullptr, &out.pool) != VK_SUCCESS) return false;

    // allocate
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = out.pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &out.layout;
    if (vkAllocateDescriptorSets(device, &ai, &out.set) != VK_SUCCESS) return false;

    return true;
}

void update_csm_set(VkDevice device, CSMSet& out, uint32_t binding,
                    VkImageView depthArrayView, VkSampler sampler)
{
    VkDescriptorImageInfo ii{};
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ii.imageView   = depthArrayView;
    ii.sampler     = sampler;

    VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = out.set;
    w.dstBinding = binding;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &ii;

    vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
}

void destroy_csm_set(VkDevice device, CSMSet& out)
{
    if (out.pool)    { vkDestroyDescriptorPool(device, out.pool, nullptr); out.pool = VK_NULL_HANDLE; }
    if (out.layout)  { vkDestroyDescriptorSetLayout(device, out.layout, nullptr); out.layout = VK_NULL_HANDLE; }
    out.set = VK_NULL_HANDLE;
}

} // namespace voxelvk
