#include <vulkan/vulkan.h>
#include <cstdio>
#include "../src/util/vk_pipeline_cache_utils.hpp"
#include <vector>
#include <cstring>
#include <cstdint>
#include <limits>

static bool has_layer(const char* name){
    uint32_t n=0; vkEnumerateInstanceLayerProperties(&n,nullptr);
    std::vector<VkLayerProperties> L(n); vkEnumerateInstanceLayerProperties(&n,L.data());
    for(auto& p:L){ if(std::strcmp(p.layerName,name)==0) return true; } return false;
}
static uint32_t find_memory_type(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags req){
    VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(phys,&mp);
    for(uint32_t i=0;i<mp.memoryTypeCount;i++){
        if((typeBits & (1u<<i)) && (mp.memoryTypes[i].propertyFlags & req) == req) return i;
    }
    return UINT32_MAX;
}

int main(){
    // Instance
    std::vector<const char*> layers;
#ifndef NDEBUG
    if(has_layer("VK_LAYER_KHRONOS_validation")) layers.push_back("VK_LAYER_KHRONOS_validation");
#endif
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "smoke_graphics_headless";
    app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    ici.enabledLayerCount = (uint32_t)layers.size();
    ici.ppEnabledLayerNames = layers.empty()? nullptr : layers.data();
#ifdef VK_KHR_portability_enumeration
    ici.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
    VkInstance instance; if(vkCreateInstance(&ici,nullptr,&instance)!=VK_SUCCESS){ std::puts("vkCreateInstance failed"); return 2; }

    // Device
    uint32_t pdn=0; vkEnumeratePhysicalDevices(instance,&pdn,nullptr);
    if(!pdn){ std::puts("no device"); return 3; }
    std::vector<VkPhysicalDevice> pds(pdn); vkEnumeratePhysicalDevices(instance,&pdn,pds.data());
    VkPhysicalDevice phys = pds[0];
    uint32_t qn=0; vkGetPhysicalDeviceQueueFamilyProperties(phys,&qn,nullptr);
    std::vector<VkQueueFamilyProperties> qprops(qn); vkGetPhysicalDeviceQueueFamilyProperties(phys,&qn,qprops.data());
    uint32_t qg = UINT32_MAX;
    for(uint32_t i=0;i<qn;i++){ if(qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT){ qg=i; break; } }
    if(qg==UINT32_MAX){ std::puts("no graphics q"); return 4; }
    float prio=1.f; VkDeviceQueueCreateInfo dq{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; dq.queueFamilyIndex=qg; dq.queueCount=1; dq.pQueuePriorities=&prio;
    const char* devExts[] = {
#ifdef VK_KHR_portability_subset
        VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif
    };
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; dci.queueCreateInfoCount=1; dci.pQueueCreateInfos=&dq;
    dci.enabledExtensionCount = (uint32_t)(sizeof(devExts)/sizeof(devExts[0])); dci.ppEnabledExtensionNames = devExts;
    VkDevice device; if(vkCreateDevice(phys,&dci,nullptr,&device)!=VK_SUCCESS){ std::puts("vkCreateDevice failed"); return 5; }
    VkPipelineCache pipelineCache = voxelvk::util::create_pipeline_cache_from_env(device);

    VkQueue q; vkGetDeviceQueue(device,qg,0,&q);

    // Command buffer
    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; cpci.queueFamilyIndex=qg;
    VkCommandPool pool; vkCreateCommandPool(device,&cpci,nullptr,&pool);
    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; cbai.commandPool=pool; cbai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount=1;
    VkCommandBuffer cmd; vkAllocateCommandBuffers(device,&cbai,&cmd);

    // Create 1x1 color attachment image + memory
    VkImageCreateInfo ici2{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici2.imageType = VK_IMAGE_TYPE_2D;
    ici2.extent = {1,1,1};
    ici2.mipLevels = 1; ici2.arrayLayers = 1;
    ici2.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici2.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici2.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici2.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ici2.samples = VK_SAMPLE_COUNT_1_BIT;
    VkImage img; if(vkCreateImage(device,&ici2,nullptr,&img)!=VK_SUCCESS){ std::puts("vkCreateImage failed"); return 6; }
    VkMemoryRequirements mr; vkGetImageMemoryRequirements(device,img,&mr);
    uint32_t memIdx = find_memory_type(phys, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(memIdx==UINT32_MAX){ std::puts("no mem type"); return 7; }
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; mai.allocationSize = mr.size; mai.memoryTypeIndex = memIdx;
    VkDeviceMemory mem; if(vkAllocateMemory(device,&mai,nullptr,&mem)!=VK_SUCCESS){ std::puts("alloc fail"); return 8; }
    vkBindImageMemory(device,img,mem,0);

    // View
    VkImageViewCreateInfo ivci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ivci.image = img; ivci.viewType = VK_IMAGE_VIEW_TYPE_2D; ivci.format = ici2.format;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; ivci.subresourceRange.levelCount=1; ivci.subresourceRange.layerCount=1;
    VkImageView view; vkCreateImageView(device,&ivci,nullptr,&view);

    // Render pass (clear-only)
    VkAttachmentDescription att{};
    att.format = ici2.format;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{}; sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; sub.colorAttachmentCount=1; sub.pColorAttachments=&ref;
    VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; rpci.attachmentCount=1; rpci.pAttachments=&att; rpci.subpassCount=1; rpci.pSubpasses=&sub;
    VkRenderPass rp; vkCreateRenderPass(device,&rpci,nullptr,&rp);

    // Framebuffer
    VkFramebufferCreateInfo fbci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbci.renderPass = rp; fbci.attachmentCount = 1; fbci.pAttachments = &view; fbci.width = 1; fbci.height = 1; fbci.layers = 1;
    VkFramebuffer fb; vkCreateFramebuffer(device,&fbci,nullptr,&fb);

    // Record: transition to COLOR_ATTACHMENT_OPTIMAL, begin render pass (clear), end
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; vkBeginCommandBuffer(cmd,&bi);
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = 0; barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = img;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; barrier.subresourceRange.levelCount=1; barrier.subresourceRange.layerCount=1;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkClearValue clear; clear.color = { {0.1f, 0.2f, 0.3f, 1.0f} };
    VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rbi.renderPass = rp; rbi.framebuffer = fb; rbi.renderArea.offset = {0,0}; rbi.renderArea.extent = {1,1}; rbi.clearValueCount = 1; rbi.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount=1; si.pCommandBuffers=&cmd;
    vkQueueSubmit(q,1,&si,VK_NULL_HANDLE); vkQueueWaitIdle(q);

    // Cleanup
    vkDestroyFramebuffer(device, fb, nullptr);
    vkDestroyRenderPass(device, rp, nullptr);
    vkDestroyImageView(device, view, nullptr);
    vkFreeMemory(device, mem, nullptr);
    vkDestroyImage(device, img, nullptr);
    vkDestroyCommandPool(device, pool, nullptr);
    voxelvk::util::save_pipeline_cache_to_env(device, pipelineCache);
    if(pipelineCache) vkDestroyPipelineCache(device, pipelineCache, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    std::puts("smoke_graphics_headless: OK");
    return 0;
}
