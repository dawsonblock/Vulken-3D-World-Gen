
#include "csm_pass.hpp"
#include <stdexcept>
#include <cstring>
#include "../util/vk_pipeline_cache_utils.hpp"

// Expect VMA headers available
#include "vk_mem_alloc.h"

namespace voxelvk {

static VkPipelineCache g_csm_pipeline_cache = VK_NULL_HANDLE;

static VkShaderModule loadShader(VkDevice dev, const char* path);

uint32_t CSMShadowPass::findMemoryType(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags flags){
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    for(uint32_t i=0;i<memProps.memoryTypeCount;i++){
        if((typeBits & (1u<<i)) && (memProps.memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    }
    throw std::runtime_error("No suitable memory type");
}

bool CSMShadowPass::init(VkPhysicalDevice phys, VkDevice dev, VmaAllocator alloc,
                         uint32_t mapSizePx, uint32_t numCascades){
    device = dev; allocator = alloc; mapSize = mapSizePx; cascades = numCascades;
    if(!createDepthArray(phys)) return false;
    if(!createSampler()) return false;
    if(!createUBO()) return false;
    if(!createPipeline(phys)) return false;
    return true;
}

void CSMShadowPass::destroy(){
    if(pipeline) vkDestroyPipeline(device, pipeline, nullptr);
    if(pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    if(uboSet) {} // freed with pool
    if(descPool) vkDestroyDescriptorPool(device, descPool, nullptr);
    if(uboSetLayout) vkDestroyDescriptorSetLayout(device, uboSetLayout, nullptr);
    if(ubo){ vmaDestroyBuffer(allocator, ubo, uboAlloc); ubo=nullptr; }
    if(depthSampler) vkDestroySampler(device, depthSampler, nullptr);
    for(auto v: layerViews) vkDestroyImageView(device, v, nullptr);
    if(depthArrayView) vkDestroyImageView(device, depthArrayView, nullptr);
    if(depthImage){ vmaDestroyImage(allocator, depthImage, depthAlloc); depthImage=VK_NULL_HANDLE; }
}

bool CSMShadowPass::createDepthArray(VkPhysicalDevice phys){
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = depthFormat;
    ci.extent = {mapSize, mapSize, 1};
    ci.mipLevels = 1;
    ci.arrayLayers = cascades;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    if(vmaCreateImage(allocator, &ci, &aci, &depthImage, &depthAlloc, nullptr) != VK_SUCCESS)
        return false;

    // Array view for sampling in lighting pass
    VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.image = depthImage;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    vci.format = depthFormat;
    vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.layerCount = cascades;
    if(vkCreateImageView(device, &vci, nullptr, &depthArrayView) != VK_SUCCESS) return false;

    // Per-layer views for rendering
    layerViews.resize(cascades);
    for(uint32_t i=0;i<cascades;i++){
        VkImageViewCreateInfo lv = vci;
        lv.viewType = VK_IMAGE_VIEW_TYPE_2D;
        lv.subresourceRange.baseArrayLayer = i;
        lv.subresourceRange.layerCount = 1;
        if(vkCreateImageView(device, &lv, nullptr, &layerViews[i]) != VK_SUCCESS) return false;
    }
    return true;
}

bool CSMShadowPass::createSampler(){
    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.compareEnable = VK_FALSE; // we sample as regular depth; PCSS manages compare
    si.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    return vkCreateSampler(device, &si, nullptr, &depthSampler) == VK_SUCCESS;
}

bool CSMShadowPass::createUBO(){
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = sizeof(CSMGpuUBO);
    bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT;
    if(vmaCreateBuffer(allocator, &bi, &aci, &ubo, &uboAlloc, nullptr) != VK_SUCCESS)
        return false;

    // descriptor
    VkDescriptorSetLayoutBinding b{};
    b.binding = 3;
    b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    b.descriptorCount = 1;
    b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    lci.bindingCount = 1; lci.pBindings = &b;
    if(vkCreateDescriptorSetLayout(device, &lci, nullptr, &uboSetLayout) != VK_SUCCESS) return false;

    VkDescriptorPoolSize ps{}; ps.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; ps.descriptorCount = 1;
    VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pci.maxSets = 1; pci.poolSizeCount = 1; pci.pPoolSizes = &ps;
    if(vkCreateDescriptorPool(device, &pci, nullptr, &descPool) != VK_SUCCESS) return false;

    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = descPool; ai.descriptorSetCount = 1; ai.pSetLayouts = &uboSetLayout;
    if(vkAllocateDescriptorSets(device, &ai, &uboSet) != VK_SUCCESS) return false;

    VkDescriptorBufferInfo dbi{ubo, 0, sizeof(CSMGpuUBO)};
    VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = uboSet; w.dstBinding = 3; w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.descriptorCount = 1; w.pBufferInfo = &dbi;
    vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
    return true;
}

bool CSMShadowPass::createPipeline(VkPhysicalDevice /*phys*/){
    // Vertex & fragment shaders
    VkShaderModule vs = loadShader(device, "spv/shadows/csm_depth.vert.spv");
    VkShaderModule fs = loadShader(device, "spv/shadows/csm_depth.frag.spv");
    if(!vs || !fs) return false;

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs; stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs; stages[1].pName = "main";

    // Vertex input: location 0 vec3 position
    VkVertexInputBindingDescription bind{}; bind.binding = 0; bind.stride = sizeof(float)*3; bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attr{}; attr.location = 0; attr.binding = 0; attr.format = VK_FORMAT_R32G32B32_SFLOAT; attr.offset = 0;
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1; vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = 1; vi.pVertexAttributeDescriptions = &attr;

    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthBiasEnable = VK_TRUE;
    rs.depthBiasConstantFactor = 1.5f; // tweak by config
    rs.depthBiasSlopeFactor = 1.2f;

    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_TRUE; ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};

    // Dynamic rendering info
    VkPipelineRenderingCreateInfo rinfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rinfo.depthAttachmentFormat = depthFormat;

    // Push constant for cascade index
    VkPushConstantRange pcr{}; pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; pcr.offset = 0; pcr.size = sizeof(int);

    // Pipeline layout
    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = 1; lci.pSetLayouts = &uboSetLayout;
    lci.pushConstantRangeCount = 1; lci.pPushConstantRanges = &pcr;
    if(vkCreatePipelineLayout(device, &lci, nullptr, &pipelineLayout) != VK_SUCCESS) return false;

    VkDynamicState dynStatesArr[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS };
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = (uint32_t)(sizeof(dynStatesArr)/sizeof(dynStatesArr[0]));
    dyn.pDynamicStates = dynStatesArr;

    VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pci.pNext = &rinfo;
    pci.stageCount = 2; pci.pStages = stages;
    pci.pVertexInputState = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState = &ms;
    pci.pDepthStencilState = &ds;
    pci.pColorBlendState = &cb;
    pci.pDynamicState = &dyn;
    pci.layout = pipelineLayout;
    pci.renderPass = VK_NULL_HANDLE;
    pci.subpass = 0;

    if(g_csm_pipeline_cache==VK_NULL_HANDLE) g_csm_pipeline_cache = voxelvk::util::create_pipeline_cache_from_env(device);
    if (vkCreateGraphicsPipelines(device, g_csm_pipeline_cache, 1, &pci, nullptr, &pipeline) != VK_SUCCESS) return false;

    vkDestroyShaderModule(device, vs, nullptr);
    vkDestroyShaderModule(device, fs, nullptr);
    return true;
}

void CSMShadowPass::updateUBO(const CSMGpuUBO& data){
    // host-visible vma buffer
    void* p = nullptr;
    vmaMapMemory(allocator, uboAlloc, &p);
    std::memcpy(p, &data, sizeof(data));
    vmaUnmapMemory(allocator, uboAlloc);
}

void CSMShadowPass::record(VkCommandBuffer cmd, const RecordDepthDrawFn& drawScene){
    // transition to depth attachment
    // (Assume your engine has a barrier helper; else leave to caller.)
    VkViewport vp{0,0,(float)mapSize,(float)mapSize,0.0f,1.0f};
    VkRect2D sc{{0,0},{mapSize,mapSize}};

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &uboSet, 0, nullptr);
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);
    vkCmdSetDepthBias(cmd, 1.5f, 0.0f, 1.2f);

    for(uint32_t i=0;i<cascades;i++){
        // begin dynamic rendering for depth layer i
        VkRenderingAttachmentInfo depthAtt{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        depthAtt.imageView = layerViews[i];
        depthAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkClearValue cv{}; cv.depthStencil = {1.0f, 0};
        depthAtt.clearValue = cv;

        VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
        ri.renderArea = sc;
        ri.layerCount = 1;
        ri.colorAttachmentCount = 0;
        ri.pDepthAttachment = &depthAtt;

        vkCmdBeginRendering(cmd, &ri);
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int), &i);

        // client-provided draw
        drawScene(cmd, (int)i);

        vkCmdEndRendering(cmd);
    }
}

// --- simple file loader for SPV (engine likely has its own) ---
#include <cstdio>
static std::vector<char> readFile(const char* path){
    std::vector<char> data;
    FILE* f = std::fopen(path, "rb");
    if(!f) return data;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    data.resize(n);
    size_t bytesRead = std::fread(data.data(), 1, n, f);
    if(bytesRead != static_cast<size_t>(n)) {
        data.clear(); // Handle partial read
    }
    std::fclose(f);
    return data;
}
static VkShaderModule loadShader(VkDevice dev, const char* path){
    auto bytes = readFile(path);
    if(bytes.empty()) return VK_NULL_HANDLE;
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = bytes.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(bytes.data());
    VkShaderModule m; 
    if(vkCreateShaderModule(dev, &ci, nullptr, &m) != VK_SUCCESS) return VK_NULL_HANDLE;
    return m;
}

} // namespace voxelvk
