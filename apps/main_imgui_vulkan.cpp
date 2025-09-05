#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#if defined(__linux__)
    #define GLFW_EXPOSE_NATIVE_X11
    #include <GLFW/glfw3native.h>
    #include <X11/Xlib.h>
    // stb image write for screenshots (header-only implementation here)
    #define STB_IMAGE_WRITE_IMPLEMENTATION
    #include <stb_image_write.h>
#endif
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <chrono>
#include <vector>
#include <thread>
#include <cmath>

// VoxelVK systems
#include "../src/core/fullscreen_toggle.hpp"
#include "../src/core/performance_monitor.hpp"
#include "../src/ai/ai_palette_config_io.hpp"
#include "../src/ai/ai_imgui_palette_panel.hpp"
#include "../src/ai/rag_runtime_bridge.hpp"
#include "../src/core/logger.hpp"
#include "../src/env/weather/weather_system.hpp"
// Utils
#include "../src/util/vk_pipeline_cache_utils.hpp"

// ImGui with Vulkan backend (support both vcpkg and upstream header layouts)
#if __has_include(<imgui.h>)
    #include <imgui.h>
        #if __has_include(<imgui/backends/imgui_impl_glfw.h>) && __has_include(<imgui/backends/imgui_impl_vulkan.h>)
        #include <imgui/backends/imgui_impl_glfw.h>
        #include <imgui/backends/imgui_impl_vulkan.h>
        #define MAIN_HAS_IMGUI_VULKAN 1
    #elif __has_include(<backends/imgui_impl_glfw.h>) && __has_include(<backends/imgui_impl_vulkan.h>)
        #include <backends/imgui_impl_glfw.h>
        #include <backends/imgui_impl_vulkan.h>
        #define MAIN_HAS_IMGUI_VULKAN 1
        #elif __has_include(<imgui_impl_glfw.h>) && __has_include(<imgui_impl_vulkan.h>)
            // vcpkg installs backends at top-level include directory
            #include <imgui_impl_glfw.h>
            #include <imgui_impl_vulkan.h>
            #define MAIN_HAS_IMGUI_VULKAN 1
    #else
        // Fallback to minimal ImGui without backend
        #define MAIN_HAS_IMGUI_MINIMAL 1
    #endif
#endif

using namespace voxelvk;

static Logger g_logger("MainImGuiApp");

// Global Vulkan objects
static VkInstance g_Instance = VK_NULL_HANDLE;
static VkSurfaceKHR g_Surface = VK_NULL_HANDLE;
static VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice g_Device = VK_NULL_HANDLE;
static uint32_t g_GraphicsQueueFamily = UINT32_MAX;
static uint32_t g_PresentQueueFamily = UINT32_MAX;
static VkQueue g_GraphicsQueue = VK_NULL_HANDLE;
static VkQueue g_PresentQueue = VK_NULL_HANDLE;
static VkSwapchainKHR g_Swapchain = VK_NULL_HANDLE;
static std::vector<VkImage> g_SwapchainImages;
static std::vector<VkImageView> g_SwapchainImageViews;
static VkFormat g_SwapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
static VkExtent2D g_SwapchainExtent { 0u, 0u };
static VkRenderPass g_RenderPass = VK_NULL_HANDLE;
static std::vector<VkFramebuffer> g_Framebuffers;
static VkCommandPool g_CommandPool = VK_NULL_HANDLE;
static std::vector<VkCommandBuffer> g_CommandBuffers;
static VkSemaphore g_ImageAvailableSemaphore = VK_NULL_HANDLE;
static VkSemaphore g_RenderFinishedSemaphore = VK_NULL_HANDLE;
static std::vector<VkFence> g_InFlightFences;
static bool g_FramebufferResized = false;
static VkPipelineCache g_PipelineCache = VK_NULL_HANDLE;

// ImGui descriptor pool (if used)
static VkDescriptorPool g_ImGuiDescriptorPool = VK_NULL_HANDLE;

static void error_callback(int code, const char* desc) {
    g_logger.Error("GLFW error {}: {}", code, desc);
}

static void framebuffer_size_callback(GLFWwindow* /*window*/, int /*w*/, int /*h*/){
    g_FramebufferResized = true;
}

static bool createSurface(GLFWwindow* window){
    if(glfwCreateWindowSurface(g_Instance, window, nullptr, &g_Surface) != VK_SUCCESS){
        g_logger.Error("Failed to create Vulkan surface");
        return false;
    }
    return true;
}

static bool initializeVulkan(GLFWwindow* window) {
    g_logger.Info("Initializing Vulkan for ImGui integration...");
    
    // Create Vulkan instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VoxelVK Production App";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VoxelVK";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;
    
    // Get required extensions
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    // Add debug utils only if available (avoid instance creation failure on minimal drivers)
    {
        uint32_t instExtCount = 0; vkEnumerateInstanceExtensionProperties(nullptr, &instExtCount, nullptr);
        std::vector<VkExtensionProperties> instExts(instExtCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &instExtCount, instExts.data());
        bool hasDebugUtils = false;
        for (const auto& e : instExts) {
            if (std::string(e.extensionName) == VK_EXT_DEBUG_UTILS_EXTENSION_NAME) { hasDebugUtils = true; break; }
        }
        if (hasDebugUtils) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    std::vector<const char*> layers;
#ifndef NDEBUG
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif
    
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();
    
    if (vkCreateInstance(&createInfo, nullptr, &g_Instance) != VK_SUCCESS) {
        g_logger.Error("Failed to create Vulkan instance");
        return false;
    }
    // Create surface for presentation
    if(!createSurface(window)) return false;
    
    // Select physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(g_Instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        g_logger.Error("No Vulkan devices found");
        return false;
    }
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(g_Instance, &deviceCount, devices.data());
    g_PhysicalDevice = devices[0]; // Use first device
    
    // Find graphics queue family
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &queueFamilyCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &queueFamilyCount, queueFamilies.data());
    
    // Find graphics and present queue families
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && g_GraphicsQueueFamily == UINT32_MAX) {
            g_GraphicsQueueFamily = i;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, i, g_Surface, &presentSupport);
        if (presentSupport && g_PresentQueueFamily == UINT32_MAX) {
            g_PresentQueueFamily = i;
        }
    }

    if (g_GraphicsQueueFamily == UINT32_MAX || g_PresentQueueFamily == UINT32_MAX) {
        g_logger.Error("No suitable graphics/present queue family found");
        return false;
    }
    
    // Create logical device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = g_GraphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    
    // Enable swapchain extension
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    VkDeviceQueueCreateInfo queueInfos[2]{};
    uint32_t uniqueCount = 0;
    if (g_GraphicsQueueFamily == g_PresentQueueFamily) {
        queueInfos[0] = queueCreateInfo; uniqueCount = 1;
    } else {
        queueInfos[0] = queueCreateInfo;
        queueInfos[1] = queueCreateInfo; queueInfos[1].queueFamilyIndex = g_PresentQueueFamily; uniqueCount = 2;
    }
    deviceCreateInfo.queueCreateInfoCount = uniqueCount;
    deviceCreateInfo.pQueueCreateInfos = queueInfos;
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
    
    if (vkCreateDevice(g_PhysicalDevice, &deviceCreateInfo, nullptr, &g_Device) != VK_SUCCESS) {
        g_logger.Error("Failed to create Vulkan device");
        return false;
    }
    
    // Create/load pipeline cache (best-effort)
    g_PipelineCache = voxelvk::util::create_pipeline_cache_from_env(g_Device);

    // Get queues
    vkGetDeviceQueue(g_Device, g_GraphicsQueueFamily, 0, &g_GraphicsQueue);
    vkGetDeviceQueue(g_Device, g_PresentQueueFamily, 0, &g_PresentQueue);
    
    g_logger.Info("Vulkan initialized successfully for ImGui");
    return true;
}

static bool createImGuiDescriptorPool() {
    // Create descriptor pool for ImGui
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * (sizeof(pool_sizes) / sizeof(pool_sizes[0]));
    pool_info.poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]);
    pool_info.pPoolSizes = pool_sizes;
    
    if (vkCreateDescriptorPool(g_Device, &pool_info, nullptr, &g_ImGuiDescriptorPool) != VK_SUCCESS) {
        g_logger.Error("Failed to create ImGui descriptor pool");
        return false;
    }
    
    return true;
}

static void shutdownVulkan() {
    if (g_Device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_Device);
    }

    // Persist pipeline cache if configured
    if (g_PipelineCache != VK_NULL_HANDLE) {
        voxelvk::util::save_pipeline_cache_to_env(g_Device, g_PipelineCache);
        vkDestroyPipelineCache(g_Device, g_PipelineCache, nullptr);
        g_PipelineCache = VK_NULL_HANDLE;
    }

    if (g_RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(g_Device, g_RenderPass, nullptr);
        g_RenderPass = VK_NULL_HANDLE;
    }
    for(auto fb : g_Framebuffers){ if(fb) vkDestroyFramebuffer(g_Device, fb, nullptr); }
    g_Framebuffers.clear();
    for(auto iv : g_SwapchainImageViews){ if(iv) vkDestroyImageView(g_Device, iv, nullptr); }
    g_SwapchainImageViews.clear();
    if (g_Swapchain != VK_NULL_HANDLE) { vkDestroySwapchainKHR(g_Device, g_Swapchain, nullptr); g_Swapchain = VK_NULL_HANDLE; }
    if (g_CommandPool != VK_NULL_HANDLE) { vkDestroyCommandPool(g_Device, g_CommandPool, nullptr); g_CommandPool = VK_NULL_HANDLE; }
    if (g_ImageAvailableSemaphore) { vkDestroySemaphore(g_Device, g_ImageAvailableSemaphore, nullptr); g_ImageAvailableSemaphore = VK_NULL_HANDLE; }
    if (g_RenderFinishedSemaphore) { vkDestroySemaphore(g_Device, g_RenderFinishedSemaphore, nullptr); g_RenderFinishedSemaphore = VK_NULL_HANDLE; }
    for(auto f : g_InFlightFences){ if(f) vkDestroyFence(g_Device, f, nullptr); }
    g_InFlightFences.clear();
    
    if (g_ImGuiDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_Device, g_ImGuiDescriptorPool, nullptr);
        g_ImGuiDescriptorPool = VK_NULL_HANDLE;
    }
    
    if (g_Device != VK_NULL_HANDLE) {
        vkDestroyDevice(g_Device, nullptr);
        g_Device = VK_NULL_HANDLE;
    }
    
    if (g_Surface != VK_NULL_HANDLE) { vkDestroySurfaceKHR(g_Instance, g_Surface, nullptr); g_Surface = VK_NULL_HANDLE; }
    if (g_Instance != VK_NULL_HANDLE) { vkDestroyInstance(g_Instance, nullptr); g_Instance = VK_NULL_HANDLE; }
}

// Swapchain helpers
static VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats){
    for(const auto& f : formats){
        if((f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_B8G8R8A8_SRGB) && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){
            return f;
        }
    }
    return formats[0];
}

static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes){
    for(const auto& m : modes){ if(m == VK_PRESENT_MODE_MAILBOX_KHR) return m; }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* window){
    if(caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
    int w=0,h=0; glfwGetFramebufferSize(window, &w, &h);
    VkExtent2D e{ (uint32_t)w, (uint32_t)h };
    if(e.width < caps.minImageExtent.width) e.width = caps.minImageExtent.width;
    if(e.height < caps.minImageExtent.height) e.height = caps.minImageExtent.height;
    if(e.width > caps.maxImageExtent.width) e.width = caps.maxImageExtent.width;
    if(e.height > caps.maxImageExtent.height) e.height = caps.maxImageExtent.height;
    return e;
}

static bool createSwapchainAndViews(GLFWwindow* window){
    VkSurfaceCapabilitiesKHR caps{}; vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice, g_Surface, &caps);
    uint32_t fmtCount=0; vkGetPhysicalDeviceSurfaceFormatsKHR(g_PhysicalDevice, g_Surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount); vkGetPhysicalDeviceSurfaceFormatsKHR(g_PhysicalDevice, g_Surface, &fmtCount, formats.data());
    uint32_t pmCount=0; vkGetPhysicalDeviceSurfacePresentModesKHR(g_PhysicalDevice, g_Surface, &pmCount, nullptr);
    std::vector<VkPresentModeKHR> modes(pmCount); vkGetPhysicalDeviceSurfacePresentModesKHR(g_PhysicalDevice, g_Surface, &pmCount, modes.data());

    auto surfaceFormat = chooseSurfaceFormat(formats);
    auto presentMode = choosePresentMode(modes);
    auto extent = chooseExtent(caps, window);

    uint32_t imageCount = caps.minImageCount + 1; if(caps.maxImageCount>0 && imageCount>caps.maxImageCount) imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR sci{}; sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = g_Surface;
    sci.minImageCount = imageCount;
    sci.imageFormat = surfaceFormat.format;
    sci.imageColorSpace = surfaceFormat.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    uint32_t qfs[2] = { g_GraphicsQueueFamily, g_PresentQueueFamily };
    if (g_GraphicsQueueFamily != g_PresentQueueFamily){
        sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2; sci.pQueueFamilyIndices = qfs;
    } else {
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = presentMode;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = g_Swapchain;

    if (vkCreateSwapchainKHR(g_Device, &sci, nullptr, &g_Swapchain) != VK_SUCCESS){
        g_logger.Error("Failed to create swapchain");
        return false;
    }
    if (sci.oldSwapchain) { vkDestroySwapchainKHR(g_Device, sci.oldSwapchain, nullptr); }

    // Get images and create image views
    uint32_t imgCount=0; vkGetSwapchainImagesKHR(g_Device, g_Swapchain, &imgCount, nullptr);
    g_SwapchainImages.resize(imgCount);
    vkGetSwapchainImagesKHR(g_Device, g_Swapchain, &imgCount, g_SwapchainImages.data());
    g_SwapchainImageFormat = surfaceFormat.format;
    g_SwapchainExtent = extent;

    g_SwapchainImageViews.resize(imgCount);
    for(size_t i=0;i<imgCount;++i){
    VkImageViewCreateInfo ivci{}; ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = g_SwapchainImages[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = g_SwapchainImageFormat;
        ivci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount = 1;
        if (vkCreateImageView(g_Device, &ivci, nullptr, &g_SwapchainImageViews[i]) != VK_SUCCESS){
            g_logger.Error("Failed to create image view");
            return false;
        }
    }
    return true;
}

static bool createRenderPassAndFramebuffers(){
    // Render pass with single color attachment clear->present
    VkAttachmentDescription color{};
    color.format = g_SwapchainImageFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL; dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0; dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{}; rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1; rpci.pAttachments = &color;
    rpci.subpassCount = 1; rpci.pSubpasses = &sub;
    rpci.dependencyCount = 1; rpci.pDependencies = &dep;

    if (vkCreateRenderPass(g_Device, &rpci, nullptr, &g_RenderPass) != VK_SUCCESS){ g_logger.Error("Failed to create render pass"); return false; }

    // Framebuffers
    g_Framebuffers.resize(g_SwapchainImageViews.size());
    for(size_t i=0;i<g_SwapchainImageViews.size();++i){
        VkImageView attachments[] = { g_SwapchainImageViews[i] };
    VkFramebufferCreateInfo fbci{}; fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass = g_RenderPass;
        fbci.attachmentCount = 1; fbci.pAttachments = attachments;
        fbci.width = g_SwapchainExtent.width; fbci.height = g_SwapchainExtent.height; fbci.layers = 1;
        if (vkCreateFramebuffer(g_Device, &fbci, nullptr, &g_Framebuffers[i]) != VK_SUCCESS){ g_logger.Error("Failed to create framebuffer"); return false; }
    }
    return true;
}

static bool createCommandPoolAndBuffers(){
    VkCommandPoolCreateInfo cpci{}; cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = g_GraphicsQueueFamily;
    if (vkCreateCommandPool(g_Device, &cpci, nullptr, &g_CommandPool) != VK_SUCCESS){ g_logger.Error("Failed to create command pool"); return false; }
    g_CommandBuffers.resize(g_Framebuffers.size());
    VkCommandBufferAllocateInfo cbai{}; cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = g_CommandPool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = (uint32_t)g_CommandBuffers.size();
    if (vkAllocateCommandBuffers(g_Device, &cbai, g_CommandBuffers.data()) != VK_SUCCESS){ g_logger.Error("Failed to alloc command buffers"); return false; }
    return true;
}

static bool createFramebuffersOnly(){
    g_Framebuffers.resize(g_SwapchainImageViews.size());
    for(size_t i=0;i<g_SwapchainImageViews.size();++i){
        VkImageView attachments[] = { g_SwapchainImageViews[i] };
    VkFramebufferCreateInfo fbci{}; fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass = g_RenderPass;
        fbci.attachmentCount = 1; fbci.pAttachments = attachments;
        fbci.width = g_SwapchainExtent.width; fbci.height = g_SwapchainExtent.height; fbci.layers = 1;
        if (vkCreateFramebuffer(g_Device, &fbci, nullptr, &g_Framebuffers[i]) != VK_SUCCESS){ g_logger.Error("Failed to create framebuffer"); return false; }
    }
    return true;
}

static void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex){
    VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &bi);
    // GPU timestamp: whole render pass region
    auto& pm_top = voxelvk::PerformanceMonitor::instance();
    uint32_t tsRenderBegin = pm_top.getGPUTimer().beginTimestamp(cmd, "RenderPass");
    VkClearValue clear{}; clear.color = { { 0.10f, 0.12f, 0.16f, 1.0f } };
    VkRenderPassBeginInfo rpbi{}; rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass = g_RenderPass; rpbi.framebuffer = g_Framebuffers[imageIndex];
    rpbi.renderArea.offset = {0,0}; rpbi.renderArea.extent = g_SwapchainExtent;
    rpbi.clearValueCount = 1; rpbi.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    // Optional: render ImGui within the pass
#ifdef MAIN_HAS_IMGUI_VULKAN
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data) {
        auto& pm = voxelvk::PerformanceMonitor::instance();
        uint32_t ts = pm.getGPUTimer().beginTimestamp(cmd, "ImGui");
        ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
        pm.getGPUTimer().endTimestamp(cmd, ts);
    }
#endif
    vkCmdEndRenderPass(cmd);
    pm_top.getGPUTimer().endTimestamp(cmd, tsRenderBegin);
    vkEndCommandBuffer(cmd);
}

static bool createSyncObjects(){
    VkSemaphoreCreateInfo sci{}; sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fci{}; fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateSemaphore(g_Device, &sci, nullptr, &g_ImageAvailableSemaphore) != VK_SUCCESS) return false;
    if (vkCreateSemaphore(g_Device, &sci, nullptr, &g_RenderFinishedSemaphore) != VK_SUCCESS) return false;
    g_InFlightFences.resize(2);
    for(auto& f : g_InFlightFences){ if (vkCreateFence(g_Device, &fci, nullptr, &f) != VK_SUCCESS) return false; }
    return true;
}

static bool recreateSwapchain(GLFWwindow* window){
    int w=0,h=0; do { glfwGetFramebufferSize(window, &w, &h); glfwWaitEventsTimeout(0.01); } while(w==0 || h==0);
    vkDeviceWaitIdle(g_Device);
    for(auto fb : g_Framebuffers){ if(fb) vkDestroyFramebuffer(g_Device, fb, nullptr); } g_Framebuffers.clear();
    for(auto iv : g_SwapchainImageViews){ if(iv) vkDestroyImageView(g_Device, iv, nullptr); } g_SwapchainImageViews.clear();
    if(!createSwapchainAndViews(window)) return false;
    if (g_RenderPass != VK_NULL_HANDLE){ if(!createFramebuffersOnly()) return false; } else { if(!createRenderPassAndFramebuffers()) return false; }
    // Recreate command buffers to match new framebuffer count
    if (g_CommandPool == VK_NULL_HANDLE) { if(!createCommandPoolAndBuffers()) return false; }
    else {
        if(!g_CommandBuffers.empty()){
            vkFreeCommandBuffers(g_Device, g_CommandPool, (uint32_t)g_CommandBuffers.size(), g_CommandBuffers.data());
        }
        g_CommandBuffers.clear();
        g_CommandBuffers.resize(g_Framebuffers.size());
        VkCommandBufferAllocateInfo cbai{}; cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbai.commandPool = g_CommandPool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = (uint32_t)g_CommandBuffers.size();
        if (vkAllocateCommandBuffers(g_Device, &cbai, g_CommandBuffers.data()) != VK_SUCCESS){ g_logger.Error("Failed to alloc command buffers (recreate)"); return false; }
    }
    return true;
}

// Simple fly camera state (for UI + future 3D usage)
struct FlyCamera {
    float x{0}, y{1.6f}, z{5};
    float yaw{0}, pitch{0};
    float moveSpeed{5.0f};
    float lookSpeed{0.15f};
};

static void updateCameraInput(FlyCamera& cam, GLFWwindow* window, float dt) {
    // WASD + QE, hold right mouse to look around
    double mx, my; static double pmx=0, pmy=0; static bool first = true;
    int rmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
    if (rmb == GLFW_PRESS) {
        glfwGetCursorPos(window, &mx, &my);
        if (first) { pmx = mx; pmy = my; first = false; }
        double dx = mx - pmx, dy = my - pmy; pmx = mx; pmy = my;
        cam.yaw   -= float(dx) * cam.lookSpeed * 0.01f;
        cam.pitch -= float(dy) * cam.lookSpeed * 0.01f;
        if (cam.pitch > 1.5f) {
            cam.pitch = 1.5f;
        }
        if (cam.pitch < -1.5f) {
            cam.pitch = -1.5f;
        }
    } else {
        first = true;
    }
    auto forward = [&](){ return std::array<float,3>{ std::cos(cam.yaw)*std::cos(cam.pitch), std::sin(cam.pitch), std::sin(cam.yaw)*std::cos(cam.pitch) }; };
    auto rightv  = [&](){ return std::array<float,3>{ std::sin(cam.yaw-3.1415926f/2.0f), 0.0f, std::cos(cam.yaw-3.1415926f/2.0f) }; };
    float speedMul = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) ? 2.5f : 1.0f;
    float s = cam.moveSpeed * speedMul * dt;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { auto f=forward(); cam.x += f[0]*s; cam.y += f[1]*s; cam.z += f[2]*s; }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { auto f=forward(); cam.x -= f[0]*s; cam.y -= f[1]*s; cam.z -= f[2]*s; }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { auto r=rightv();  cam.x -= r[0]*s;                 cam.z -= r[2]*s; }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { auto r=rightv();  cam.x += r[0]*s;                 cam.z += r[2]*s; }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) { cam.y -= s; }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) { cam.y += s; }
}

#if defined(__linux__)
// Simple X11-based screenshot using the root window region for the target window; avoids Vulkan readbacks
static bool SaveWindowScreenshot(GLFWwindow* window, const char* path) {
    Display* dpy = glfwGetX11Display();
    if (!dpy) return false;
    Window xw = glfwGetX11Window(window);

    // Query window geometry and translate to root coords
    XWindowAttributes attr{};
    if (!XGetWindowAttributes(dpy, xw, &attr)) return false;
    Window root = DefaultRootWindow(dpy);
    int rx=0, ry=0; Window child;
    XTranslateCoordinates(dpy, xw, root, 0, 0, &rx, &ry, &child);

    int w = attr.width;
    int h = attr.height;
    if (w<=0 || h<=0) return false;

    // Ensure server processed recent drawing before capture
    XSync(dpy, False);

    // Clamp capture region to root bounds
    Window rr; int rrx, rry; unsigned int rw, rh, rb, rd;
    if (XGetGeometry(dpy, root, &rr, &rrx, &rry, &rw, &rh, &rb, &rd)) {
        if (rx < 0) { w += rx; rx = 0; }
        if (ry < 0) { h += ry; ry = 0; }
        if (rx + w > (int)rw) w = std::max(0, (int)rw - rx);
        if (ry + h > (int)rh) h = std::max(0, (int)rh - ry);
        if (w <= 0 || h <= 0) return false;
    }

    // Temporary X error handler to swallow BadMatch
    auto prevHandler = XSetErrorHandler([](Display*, XErrorEvent* e)->int { (void)e; return 0; });

    // Capture from the root window region covering our app window
    XImage* img = XGetImage(dpy, root, (int)rx, (int)ry, (unsigned)w, (unsigned)h, AllPlanes, ZPixmap);
    // Force error delivery, then restore handler
    XSync(dpy, False);
    XSetErrorHandler(prevHandler);
    if (!img) return false;

    auto popcount = [](unsigned long x){ int c=0; while(x){ c += (x&1ul); x >>= 1; } return c; };
    auto firstbit = [](unsigned long x){ int s=0; if(!x) return 0; while((x & 1ul)==0){ x >>= 1; ++s; } return s; };

    int rbits = popcount(img->red_mask);
    int gbits = popcount(img->green_mask);
    int bbits = popcount(img->blue_mask);
    int rshift = firstbit(img->red_mask);
    int gshift = firstbit(img->green_mask);
    int bshift = firstbit(img->blue_mask);

    std::vector<unsigned char> rgba(static_cast<size_t>(w*h*4));
    for (int y=0; y<h; ++y) {
        for (int x=0; x<w; ++x) {
            unsigned long p = XGetPixel(img, x, h-1-y); // flip vertically
            unsigned long rv = (p & img->red_mask) >> rshift;
            unsigned long gv = (p & img->green_mask) >> gshift;
            unsigned long bv = (p & img->blue_mask) >> bshift;
            auto scale = [](unsigned long v, int bits){ if(bits<=0) return (unsigned char)0; unsigned long maxv = (1ul<<bits) - 1ul; double f = maxv ? (double)v / (double)maxv : 0.0; int u = (int)std::round(f * 255.0); if(u<0) u=0; if(u>255) u=255; return (unsigned char)u; };
            unsigned char R = scale(rv, rbits);
            unsigned char G = scale(gv, gbits);
            unsigned char B = scale(bv, bbits);
            size_t idx = static_cast<size_t>((y*w + x) * 4);
            rgba[idx+0] = R; rgba[idx+1] = G; rgba[idx+2] = B; rgba[idx+3] = 255;
        }
    }
    int ok = stbi_write_png(path, w, h, 4, rgba.data(), w*4);
    XDestroyImage(img);
    return ok != 0;
}
#endif

static bool hasArg(int argc, char** argv, const char* flag) {
    for (int i=0;i<argc;++i) {
        if (std::string(argv[i]) == flag) return true;
    }
    return false;
}

static const char* getArgValue(int argc, char** argv, const char* key) {
    for (int i=0; i<argc-1; ++i) {
        if (std::string(argv[i]) == key) return argv[i+1];
    }
    return nullptr;
}

int main(int argc, char** argv) {
    g_logger.Info("=== VoxelVK Production ImGui Demo ===");
    
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
        g_logger.Error("Failed to initialize GLFW");
        return 1;
    }
    
    // Create window for Vulkan (no OpenGL context)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    
    GLFWwindow* window = glfwCreateWindow(1600, 900, "VoxelVK Production App", nullptr, nullptr);
    if (!window) {
        g_logger.Error("Failed to create GLFW window");
        glfwTerminate();
        return 2;
    }
    
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Initialize Vulkan
    if (!initializeVulkan(window)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 3;
    }

    // Swapchain + draw setup
    if(!createSwapchainAndViews(window)) { shutdownVulkan(); glfwDestroyWindow(window); glfwTerminate(); return 3; }
    if(!createRenderPassAndFramebuffers()) { shutdownVulkan(); glfwDestroyWindow(window); glfwTerminate(); return 3; }
    if(!createCommandPoolAndBuffers()) { shutdownVulkan(); glfwDestroyWindow(window); glfwTerminate(); return 3; }
    if(!createSyncObjects()) { shutdownVulkan(); glfwDestroyWindow(window); glfwTerminate(); return 3; }
    // Initialize performance monitor and GPU timer (best-effort)
    voxelvk::PerformanceMonitor::instance().initialize(voxelvk::PerformanceBudgetTracker::BudgetConfig::Balanced60());
    {
        uint32_t framesInFlight = g_SwapchainImages.empty() ? 2u : (uint32_t)g_SwapchainImages.size();
        voxelvk::PerformanceMonitor::instance().enableGPUTimer(g_Device, g_PhysicalDevice, framesInFlight, 64);
    }
    
    // Initialize weather system for demo
    WeatherSystem weatherSystem;
    try {
        weatherSystem.loadFromYaml("config/weather.yaml");
        g_logger.Info("Weather system loaded successfully");
    } catch (const std::exception& e) {
        g_logger.Warn("Using default weather config: {}", e.what());
    }
    
    // Set up fullscreen toggle handler
    voxelvk::SetFullscreenHandler([window](bool fullscreen) {
        static voxelvk::WindowedState windowedState;
        voxelvk::ToggleFullscreen(window, fullscreen, windowedState);
        const char* title = fullscreen ? "VoxelVK Production App (Fullscreen)" : "VoxelVK Production App (Windowed)";
        glfwSetWindowTitle(window, title);
    });
    
    // Initialize AI palette runtime
    voxelvk::ai::PaletteRuntime paletteRuntime;
    paletteRuntime.load_from_file("ai_palette.cfg");
    
    // Set up RAG config handler
    voxelvk::ai::SetRagConfigHandler([&](bool enabled, int topK) {
        g_logger.Info("RAG config updated: enabled={}, top_k={}", enabled, topK);
    });
    
    // Track last-known RAG config to avoid spamming updates/logs
    bool last_rag_enabled = paletteRuntime.cfg.ai_generation.enable_rag;
    int  last_rag_top_k  = paletteRuntime.cfg.ai_generation.rag_top_k;
    voxelvk::ai::UpdateRagConfig(last_rag_enabled, last_rag_top_k);
    
#ifdef MAIN_HAS_IMGUI_VULKAN
    // Initialize ImGui with Vulkan backend
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Enable docking/viewports only if backend has those features
    #ifdef IMGUI_HAS_DOCKING
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    #endif
    // Optional multi-viewport: enabled by flag/env to avoid quirks on some WMs
    #ifdef IMGUI_HAS_VIEWPORT
    const bool enableViewports = hasArg(argc, argv, "--viewports") || std::getenv("VOXELVK_VIEWPORTS");
    if (enableViewports) io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    #endif

    // Ini path & UI scale from arg/env (CLI takes precedence over env)
    if (const char* ini = getArgValue(argc, argv, "--imgui-ini")) {
        ImGui::GetIO().IniFilename = ini;
    } else if (const char* envIni = std::getenv("VOXELVK_IMGUI_INI")) {
        ImGui::GetIO().IniFilename = envIni;
    }
    // UI scale
    float uiScale = 1.0f;
    bool uiScaleParsed = false;
    if (const char* s = getArgValue(argc, argv, "--ui-scale")) {
        try {
            uiScale = std::max(0.5f, std::min(2.0f, std::stof(s)));
            uiScaleParsed = true;
        } catch (const std::exception& ex) {
            g_logger.Error(std::string("Failed to parse --ui-scale argument: ") + ex.what());
        } catch (...) {
            g_logger.Error("Failed to parse --ui-scale argument: unknown error");
        }
    }
    if (!uiScaleParsed) {
        if (const char* e = std::getenv("VOXELVK_UI_SCALE")) {
            try {
                uiScale = std::max(0.5f, std::min(2.0f, std::stof(e)));
            } catch (const std::exception& ex) {
                g_logger.Error(std::string("Failed to parse VOXELVK_UI_SCALE environment variable: ") + ex.what());
            } catch (...) {
                g_logger.Error("Failed to parse VOXELVK_UI_SCALE environment variable: unknown error");
            }
        }
    }
    io.FontGlobalScale = uiScale;

    // Theme switch (runtime)
    enum class ThemeKind { Dark, Light };
    static ThemeKind themeKind = ThemeKind::Dark;
    auto ApplyDarkTheme = [](){
        ImGui::StyleColorsDark();
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 8.0f;
        s.FrameRounding = 6.0f;
        s.GrabRounding = 6.0f;
        s.PopupRounding = 8.0f;
        s.TabRounding = 6.0f;
        s.ScrollbarRounding = 9.0f;
        s.FrameBorderSize = 1.0f;
        s.WindowBorderSize = 1.0f;
        s.ItemSpacing = ImVec2(10, 8);
        s.ItemInnerSpacing = ImVec2(8, 6);
        s.WindowPadding = ImVec2(12, 10);
        s.FramePadding = ImVec2(10, 6);
        ImVec4* c = s.Colors;
        c[ImGuiCol_WindowBg]        = ImVec4(0.10f,0.11f,0.14f,1.00f);
        c[ImGuiCol_TitleBg]         = ImVec4(0.08f,0.09f,0.12f,1.00f);
        c[ImGuiCol_TitleBgActive]   = ImVec4(0.18f,0.19f,0.24f,1.00f);
        c[ImGuiCol_TitleBgCollapsed]= ImVec4(0.08f,0.09f,0.12f,1.00f);
        c[ImGuiCol_Header]          = ImVec4(0.20f,0.45f,0.85f,0.22f);
        c[ImGuiCol_HeaderHovered]   = ImVec4(0.20f,0.45f,0.85f,0.40f);
        c[ImGuiCol_HeaderActive]    = ImVec4(0.20f,0.45f,0.85f,0.55f);
        c[ImGuiCol_Button]          = ImVec4(0.20f,0.45f,0.85f,0.35f);
        c[ImGuiCol_ButtonHovered]   = ImVec4(0.20f,0.45f,0.85f,0.55f);
        c[ImGuiCol_ButtonActive]    = ImVec4(0.20f,0.45f,0.85f,0.75f);
        c[ImGuiCol_CheckMark]       = ImVec4(0.95f,0.95f,0.95f,1.00f);
        c[ImGuiCol_SliderGrab]      = ImVec4(0.30f,0.60f,1.00f,0.80f);
        c[ImGuiCol_SliderGrabActive]= ImVec4(0.30f,0.60f,1.00f,1.00f);
        c[ImGuiCol_Separator]       = ImVec4(0.25f,0.28f,0.32f,1.00f);
        c[ImGuiCol_Tab]             = ImVec4(0.16f,0.18f,0.22f,1.00f);
        c[ImGuiCol_TabHovered]      = ImVec4(0.20f,0.45f,0.85f,0.45f);
        c[ImGuiCol_TabActive]       = ImVec4(0.18f,0.20f,0.25f,1.00f);
        c[ImGuiCol_NavHighlight]    = ImVec4(0.20f,0.45f,0.85f,0.60f);
    #ifdef IMGUI_HAS_VIEWPORT
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) { s.WindowRounding = 8.0f; s.Colors[ImGuiCol_WindowBg].w = 1.0f; }
    #endif
    };
    auto ApplyLightTheme = [](){
        ImGui::StyleColorsLight();
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 8.0f; s.FrameRounding = 6.0f; s.GrabRounding = 6.0f; s.PopupRounding = 8.0f; s.TabRounding = 6.0f; s.ScrollbarRounding = 9.0f;
        s.ItemSpacing = ImVec2(10, 8); s.ItemInnerSpacing = ImVec2(8, 6); s.WindowPadding = ImVec2(12, 10); s.FramePadding = ImVec2(10, 6);
    };
    ApplyDarkTheme();
    
    // Initialize ImGui backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    
    if (!createImGuiDescriptorPool()) {
        g_logger.Error("Failed to create ImGui descriptor pool");
        shutdownVulkan();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 4;
    }

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_GraphicsQueueFamily;
    init_info.Queue = g_GraphicsQueue;
    init_info.DescriptorPool = g_ImGuiDescriptorPool;
    init_info.MinImageCount = (uint32_t)g_SwapchainImages.size();
    init_info.ImageCount = (uint32_t)g_SwapchainImages.size();
    // Note: UseDynamicRendering not available in ImGui 1.86
    // Some backends provide PipelineCache in init info; ignore if not present in this version.
#ifdef IMGUI_IMPL_VULKAN_HAS_PIPELINE_CACHE
    init_info.PipelineCache = g_PipelineCache;
#endif
    
    // Initialize ImGui Vulkan backend - render pass passed separately in older versions
    ImGui_ImplVulkan_Init(&init_info, g_RenderPass);

    // Upload ImGui fonts
    {
    // Newer backend API creates fonts texture without explicit command buffer
    ImGui_ImplVulkan_CreateFontsTexture();
    vkQueueWaitIdle(g_GraphicsQueue);
    ImGui_ImplVulkan_DestroyFontsTexture();
    }

    g_logger.Info("ImGui Vulkan backend initialized");
#endif
    
    // Frame timing
    double lastTime = glfwGetTime();
    double fps = 0.0;
    int frameCount = 0;
    double fpsAccum = 0.0;

    // Smoke/benchmark options
    double exitAfterSec = 0.0;
    if (hasArg(argc, argv, "--smoke")) exitAfterSec = 2.0;
    if (const char* envExit = std::getenv("VOXELVK_EXIT_AFTER_SEC")) {
        try { exitAfterSec = std::max(exitAfterSec, std::stod(envExit)); } catch(...) {}
    }
    bool benchmarkMode = hasArg(argc, argv, "--benchmark") || std::getenv("VOXELVK_BENCHMARK");
    int maxFrames = -1;
    if (const char* v = getArgValue(argc, argv, "--frames")) {
        try { maxFrames = std::stoi(v); } catch(...) {}
    }
    if (const char* envFrames = std::getenv("VOXELVK_BENCH_FRAMES")) {
        try { maxFrames = std::max(maxFrames, std::stoi(envFrames)); } catch(...) {}
    }
    std::string perfOutDir = ".";
    if (const char* v = getArgValue(argc, argv, "--perf-out")) perfOutDir = v;
    if (const char* envDir = std::getenv("VOXELVK_PERF_OUT")) perfOutDir = envDir;

    FlyCamera cam; // track camera state
    bool autoScreenshotPending = false;
    
    // Performance history for graphs
    std::vector<float> frameTimeHistory_(120, 16.67f);
    std::vector<float> fpsHistory_(120, 60.0f);
    if (hasArg(argc, argv, "--autoscreenshot") || std::getenv("VOXELVK_AUTOSCREENSHOT")) {
        autoScreenshotPending = true;
    }
    
    g_logger.Info("Entering main loop...");
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
    // Update timing
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        
        frameCount++;
        fpsAccum += deltaTime;
        
        if (fpsAccum >= 1.0) {
            fps = frameCount / fpsAccum;
            frameCount = 0;
            fpsAccum = 0.0;
            
            // Update performance history for graphs
            frameTimeHistory_.erase(frameTimeHistory_.begin());
            frameTimeHistory_.push_back(static_cast<float>(1000.0 / fps));
            fpsHistory_.erase(fpsHistory_.begin());
            fpsHistory_.push_back(static_cast<float>(fps));
            
            std::string title = "VoxelVK Production App - " + std::to_string(static_cast<int>(fps)) + " FPS";
            glfwSetWindowTitle(window, title.c_str());
        }
        
    // Update weather system
        {
            PERF_WEATHER_TIMER("WeatherTick");
            weatherSystem.tick(deltaTime);
        }
        
    // Hot reload palette and propagate RAG settings (only on change)
        paletteRuntime.tick_hot_reload();
        bool cur_enabled = paletteRuntime.cfg.ai_generation.enable_rag;
        int  cur_top_k   = paletteRuntime.cfg.ai_generation.rag_top_k;
        if (cur_enabled != last_rag_enabled || cur_top_k != last_rag_top_k) {
            last_rag_enabled = cur_enabled;
            last_rag_top_k = cur_top_k;
            voxelvk::ai::UpdateRagConfig(last_rag_enabled, last_rag_top_k);
        }

    // Camera input
    updateCameraInput(cam, window, static_cast<float>(deltaTime));
        
    // Handle fullscreen toggle with edge detection (shared state)
    static bool g_isFullscreen = false;
    static bool f11Prev = false; bool f11Now = (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS);
    if (f11Now && !f11Prev) { g_isFullscreen = !g_isFullscreen; voxelvk::RequestFullscreen(g_isFullscreen); }
    f11Prev = f11Now;
        
        // Config hot-reload on F5
        {
            static bool reloadPressed = false;
            if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS) {
                if (!reloadPressed) {
                g_logger.Info("F5 pressed - hot reloading configuration...");
                try {
                    weatherSystem.loadFromYaml("config/weather.yaml");
                    paletteRuntime.load_from_file("ai_palette.cfg");
                    g_logger.Info("Configuration hot-reloaded successfully");
                } catch (const std::exception& e) {
                    g_logger.Warn("Hot-reload failed: {}", e.what());
                }
                    reloadPressed = true;
                }
            } else {
                reloadPressed = false;
            }
        }
        
    // Start a new ImGui frame (if available)
#ifdef MAIN_HAS_IMGUI_VULKAN
        {
            PERF_FRAME_TIMER("UI_Build");
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // Dockspace & main menu (only when docking is available)
            #ifdef IMGUI_HAS_DOCKING
            ImGuiViewport* vp = ImGui::GetMainViewport();
            ImGui::DockSpaceOverViewport(vp);
            #endif
        if (ImGui::BeginMainMenuBar()) {
                if (ImGui::BeginMenu("View")) {
                    ImGui::MenuItem("Overview", nullptr, &showOverview);
                    ImGui::MenuItem("Camera", nullptr, &showCamera);
                    ImGui::MenuItem("Systems", nullptr, &showSystems);
                    ImGui::MenuItem("Performance", nullptr, &showPerf);
                    ImGui::Separator();
                    ImGui::MenuItem("AI Configuration", nullptr, &showAiPanel);
                    ImGui::Separator();
                    ImGui::Text("UI Scale"); ImGui::SameLine();
                    static float uiScaleRuntime = io.FontGlobalScale; if (ImGui::SliderFloat("##uiscale", &uiScaleRuntime, 0.75f, 1.75f, "%.2fx")) io.FontGlobalScale = uiScaleRuntime;
                    ImGui::Separator();
                    bool tDark = (themeKind == ThemeKind::Dark);
                    if (ImGui::MenuItem("Dark Theme", nullptr, tDark)) { themeKind = ThemeKind::Dark; ApplyDarkTheme(); }
                    if (ImGui::MenuItem("Light Theme", nullptr, !tDark)) { themeKind = ThemeKind::Light; ApplyLightTheme(); }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Actions")) {
                    if (ImGui::MenuItem("Toggle Fullscreen (F11)")) { g_isFullscreen = !g_isFullscreen; voxelvk::RequestFullscreen(g_isFullscreen);}            
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }

            // Command Palette (Ctrl+K)
            static bool paletteOpen = false; static char paletteQuery[128] = "";
            if (io.KeyCtrl && ImGui::IsKeyPressed(75, false)) paletteOpen = true; // 75 = 'K' key
            struct Cmd { const char* name; std::function<void()> fn; };
            std::vector<Cmd> cmds = {
                {"Toggle Fullscreen", [&](){ g_isFullscreen = !g_isFullscreen; voxelvk::RequestFullscreen(g_isFullscreen); }},
                {"Open AI Configuration", [&](){ showAiPanel = true; }},
                {"Toggle RAG", [&](){ 
                    bool newState = !last_rag_enabled; 
                    voxelvk::ai::UpdateRagConfig(newState, last_rag_top_k); 
                    last_rag_enabled = newState; 
                }},
                {"Save Screenshot", [&](){
#if defined(__linux__)
                    SaveWindowScreenshot(window, "screenshot.png");
#endif
                }},
                {"Export Perf JSON", [&](){ voxelvk::PerformanceMonitor::instance().exportPerformanceData("."); }},
                {"Hot Reload Config", [&](){ 
                    paletteRuntime.load_from_file(paletteRuntime.path);
                    try { weatherSystem.loadFromYaml("config/weather.yaml"); } catch(...) {}
                }},
                #ifdef IMGUI_HAS_VIEWPORT
                {"Toggle Viewports", [&](){ io.ConfigFlags ^= ImGuiConfigFlags_ViewportsEnable; }},
                #endif
            };
            if (paletteOpen) {
                ImGui::SetNextWindowSize(ImVec2(520, 300), ImGuiCond_Always);
                if (ImGui::Begin("Command Palette", &paletteOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
                    ImGui::InputTextWithHint("##cmd", "Type a command...", paletteQuery, sizeof(paletteQuery));
                    auto matches = cmds;
                    if (paletteQuery[0]) {
                        matches.clear();
                        std::string q = paletteQuery; std::transform(q.begin(), q.end(), q.begin(), ::tolower);
                        for (auto& c : cmds) { std::string n=c.name; std::transform(n.begin(), n.end(), n.begin(), ::tolower); if (n.find(q)!=std::string::npos) matches.push_back(c); }
                    }
                    ImGui::Separator();
                    for (auto& m : matches) {
                        if (ImGui::Selectable(m.name)) { m.fn(); paletteOpen=false; paletteQuery[0]='\0'; }
                    }
                }
                ImGui::End();
            }

            // Panel visibility state
            static bool showAiPanel = true, showOverview = true, showCamera = true, showSystems = true, showPerf = true;
            
            // Modern Overview panel with enhanced metrics
            if (showOverview && ImGui::Begin("Overview", &showOverview)) {
                // Header with status indicators
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "ACTIVE");
                ImGui::SameLine(); ImGui::Text("VoxelVK Production Engine");
                ImGui::Separator();
                
                // Performance metrics in a table
                if (ImGui::BeginTable("PerformanceTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();
                    
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("FPS");
                    ImGui::TableNextColumn(); 
                    ImVec4 fpsColor = fps > 55.0f ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : 
                                     fps > 30.0f ? ImVec4(0.9f, 0.9f, 0.3f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
                    ImGui::TextColored(fpsColor, "%.1f", fps);
                    
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Frame Time");
                    ImGui::TableNextColumn(); ImGui::Text("%.2f ms", 1000.0 * (fps > 0.0 ? 1.0 / fps : 0.0));
                    
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Resolution");
                    ImGui::TableNextColumn(); ImGui::Text("%dx%d", (int)g_SwapchainExtent.width, (int)g_SwapchainExtent.height);
                    
                    ImGui::EndTable();
                }
                
                ImGui::Spacing();
                ImGui::TextDisabled("Controls: WASD/QE move, hold RMB to look, Shift to sprint, F11 toggle fullscreen");
#if defined(__linux__)
                static bool screenshot_ok = false; static double screenshot_msg_t = 0.0;
                if (ImGui::Button("Save Screenshot (P)")) { screenshot_ok = SaveWindowScreenshot(window, "screenshot.png"); screenshot_msg_t = glfwGetTime(); }
                {
                    static bool ppressed = false;
                    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
                        if (!ppressed) {
                            screenshot_ok = SaveWindowScreenshot(window, "screenshot.png");
                            screenshot_msg_t = glfwGetTime();
                            ppressed = true;
                        }
                    } else {
                        ppressed = false;
                    }
                }
                if (glfwGetTime() - screenshot_msg_t < 2.0) {
                    ImGui::TextColored(screenshot_ok ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1), screenshot_ok ? "Saved to screenshot.png" : "Screenshot failed");
                }
#endif
            }
            ImGui::End();

            if (ImGui::Begin("Camera")) {
                ImGui::Text("Position");
                ImGui::BulletText("(%.2f, %.2f, %.2f)", static_cast<double>(cam.x), static_cast<double>(cam.y), static_cast<double>(cam.z));
                ImGui::Text("Orientation");
                ImGui::BulletText("Yaw/Pitch: (%.2f, %.2f)", static_cast<double>(cam.yaw), static_cast<double>(cam.pitch));
                ImGui::SliderFloat("Move speed", &cam.moveSpeed, 0.5f, 20.0f);
            }
            ImGui::End();

            // Enhanced Systems panel
            if (showSystems && ImGui::Begin("Systems", &showSystems)) {
                ImGui::Text("Engine Systems Status");
                ImGui::Separator();
                
                // Weather System Status
                ImGui::BulletText("Weather System:");
                ImGui::SameLine(); ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Running");
                
                // AI Systems Status
                ImGui::BulletText("AI Generation:");
                ImGui::SameLine(); 
                bool aiActive = last_rag_enabled || paletteRuntime.cfg.ai_generation.enable_ai_structures;
                ImGui::TextColored(aiActive ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                                 aiActive ? "Active" : "Inactive");
                
                ImGui::Spacing();
                
                // Quick RAG Controls
                ImGui::Text("Quick Controls:");
                static bool rag_enabled = last_rag_enabled; static int rag_top_k = last_rag_top_k;
                if (ImGui::Checkbox("Enable RAG", &rag_enabled)) { 
                    voxelvk::ai::UpdateRagConfig(rag_enabled, rag_top_k); 
                    last_rag_enabled = rag_enabled; 
                }
                if (ImGui::SliderInt("RAG Top-K", &rag_top_k, 1, 16)) { 
                    voxelvk::ai::UpdateRagConfig(rag_enabled, rag_top_k); 
                    last_rag_top_k = rag_top_k; 
                }
                
                ImGui::Spacing();
                if (ImGui::Button("Open AI Configuration", ImVec2(180, 30))) {
                    showAiPanel = true;
                }
            }
            ImGui::End();
            
            // AI Palette Panel (modernized)
            if (showAiPanel) {
                bool aiPanelOpen = showAiPanel;
                if (ImGui::Begin("AI Configuration", &aiPanelOpen, ImGuiWindowFlags_MenuBar)) {
                    voxelvk::ai::DrawAIPalettePanel(paletteRuntime);
                }
                ImGui::End();
                showAiPanel = aiPanelOpen;
            }

            // Enhanced Performance panel with graphs
            if (showPerf && ImGui::Begin("Performance", &showPerf)) {
                auto& pm = voxelvk::PerformanceMonitor::instance();
                auto& tracker = pm.getBudgetTracker();
                
                // Performance Summary Table
                if (ImGui::BeginTable("PerfSummary", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableHeadersRow();
                    
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Frame Time");
                    ImGui::TableNextColumn(); 
                    float frameTime = tracker.getAverageTime(voxelvk::PerformanceBudget::FRAME_TOTAL);
                    ImVec4 frameColor = frameTime < 16.67f ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : 
                                       frameTime < 33.33f ? ImVec4(0.9f, 0.9f, 0.3f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
                    ImGui::TextColored(frameColor, "%.2f ms", frameTime);
                    ImGui::TableNextColumn(); ImGui::Text("16.67 ms");
                    
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("P95 Frame");
                    ImGui::TableNextColumn(); ImGui::Text("%.2f ms", tracker.getP95Time(voxelvk::PerformanceBudget::FRAME_TOTAL));
                    ImGui::TableNextColumn(); ImGui::Text("20.0 ms");
                    
                    ImGui::EndTable();
                }
                
                ImGui::Spacing();
                
                // Frame time history graph
                if (!fpsHistory_.empty()) {
                    ImGui::Text("FPS History (2 min)");
                    ImGui::PlotLines("##FPSGraph", fpsHistory_.data(), (int)fpsHistory_.size(), 0, nullptr, 0.0f, 120.0f, ImVec2(0, 80));
                    
                    ImGui::Text("Frame Time History");
                    ImGui::PlotLines("##FrameTimeGraph", frameTimeHistory_.data(), (int)frameTimeHistory_.size(), 0, nullptr, 0.0f, 50.0f, ImVec2(0, 80));
                }
                
                // GPU Timings
                const auto& timings = pm.getGPUTimer().getAllTimings();
                if (!timings.empty()) {
                    ImGui::Separator();
                    ImGui::Text("GPU Pipeline Timings:");
                    for (const auto& kv : timings) {
                        ImGui::BulletText("%s: %.3f ms", kv.first.c_str(), kv.second);
                        
                        // Add a small progress bar for visual representation
                        float normalized = std::min(static_cast<float>(kv.second / 16.67f), 1.0f); // Normalize to 60 FPS budget
                        ImVec4 barColor = normalized < 0.5f ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : 
                                         normalized < 0.8f ? ImVec4(0.9f, 0.9f, 0.3f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
                        ImGui::ProgressBar(normalized, ImVec2(150, 0), "");
                        ImGui::PopStyleColor();
                    }
                } else {
                    ImGui::TextDisabled("GPU timings not available");
                }
                
                // Performance actions
                ImGui::Spacing();
                ImGui::Separator();
                if (ImGui::Button("Export Performance Data", ImVec2(180, 30))) {
                    pm.exportPerformanceData(".");
                }
            }
            ImGui::End();
            // Status bar
            ImGuiWindowFlags sbFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
            ImGuiViewport* mainVp = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(mainVp->WorkPos.x, mainVp->WorkPos.y + mainVp->WorkSize.y - 24));
            ImGui::SetNextWindowSize(ImVec2(mainVp->WorkSize.x, 24));
            if (ImGui::Begin("##StatusBar", nullptr, sbFlags)) {
                ImGui::Text("FPS: %.0f  (%.2f ms)", fps, 1000.0 * (fps > 0.0 ? 1.0 / fps : 0.0));
                ImGui::SameLine();
                auto& pm = voxelvk::PerformanceMonitor::instance();
                const auto& timings = pm.getGPUTimer().getAllTimings();
                if (!timings.empty()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("|");
                    ImGui::SameLine();
                    int n = 0;
                    for (const auto& kv : timings) {
                        if (n++ > 0) {
                            ImGui::SameLine();
                            ImGui::TextDisabled("|");
                            ImGui::SameLine();
                        }
                        ImGui::Text("%s: %.2f ms", kv.first.c_str(), kv.second);
                    }
                }
            }
            ImGui::End();
        }
        ImGui::Render();
        #ifdef IMGUI_HAS_VIEWPORT
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
        #endif
#elif defined(MAIN_HAS_IMGUI_MINIMAL)
        // Minimal ImGui without backend: just ensure we log once
        static bool firstRun = true;
        if (firstRun) { g_logger.Info("ImGui available but no Vulkan backend - running minimal mode"); firstRun = false; }
#endif

        // Acquire/record/submit/present
    vkWaitForFences(g_Device, 1, &g_InFlightFences[0], VK_TRUE, 1000000000ULL);
        vkResetFences(g_Device, 1, &g_InFlightFences[0]);

    uint32_t imageIndex = 0;
        VkResult aiRes = vkAcquireNextImageKHR(g_Device, g_Swapchain, UINT64_MAX, g_ImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (aiRes == VK_ERROR_OUT_OF_DATE_KHR || g_FramebufferResized) {
            g_FramebufferResized = false;
            if(!recreateSwapchain(window)) { g_logger.Error("Swapchain recreate failed"); break; }
            continue;
        } else if (aiRes != VK_SUCCESS && aiRes != VK_SUBOPTIMAL_KHR) {
            g_logger.Error("Failed to acquire swapchain image");
            break;
        }

    // Begin performance frame with swapchain image index
    voxelvk::PerformanceMonitor::instance().beginFrame(imageIndex);

    // Re-record for this image (with optional ImGui)
    recordCommandBuffer(g_CommandBuffers[imageIndex], imageIndex);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.waitSemaphoreCount = 1; si.pWaitSemaphores = &g_ImageAvailableSemaphore; si.pWaitDstStageMask = &waitStage;
        si.commandBufferCount = 1; si.pCommandBuffers = &g_CommandBuffers[imageIndex];
        si.signalSemaphoreCount = 1; si.pSignalSemaphores = &g_RenderFinishedSemaphore;
        if (vkQueueSubmit(g_GraphicsQueue, 1, &si, g_InFlightFences[0]) != VK_SUCCESS){ g_logger.Error("Queue submit failed"); break; }

    VkPresentInfoKHR pi{}; pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &g_RenderFinishedSemaphore;
        pi.swapchainCount = 1; pi.pSwapchains = &g_Swapchain; pi.pImageIndices = &imageIndex;
        VkResult pr = vkQueuePresentKHR(g_PresentQueue, &pi);
        if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR || g_FramebufferResized) {
            g_FramebufferResized = false;
            if(!recreateSwapchain(window)) { g_logger.Error("Swapchain present-recreate failed"); break; }
        } else if (pr != VK_SUCCESS) {
            g_logger.Error("Failed to present swapchain image");
            break;
        }
        // End performance frame and collect timings
        voxelvk::PerformanceMonitor::instance().endFrame();

        // Auto-screenshot after first frame if requested
#if defined(__linux__)
        if (autoScreenshotPending) {
            if (SaveWindowScreenshot(window, "screenshot.png")) {
                g_logger.Info("Auto-screenshot saved to screenshot.png");
            } else {
                g_logger.Warn("Auto-screenshot failed");
            }
            autoScreenshotPending = false;
        }
#endif
        // Optional auto-exit for smoke testing
        if (exitAfterSec > 0.0) {
            static double accum = 0.0; accum += deltaTime; if (accum >= exitAfterSec) { g_logger.Info("Smoke time reached; exiting."); break; }
        }
        // Benchmark frame budget (deterministic count)
        if (benchmarkMode && maxFrames > 0) {
            static int frames = 0; if (++frames >= maxFrames) { g_logger.Info("Benchmark frames reached ({}); exiting.", maxFrames); break; }
        }
    }
    
    g_logger.Info("Main loop exited, shutting down...");
    
    // Optional ImGui cleanup
    #ifdef MAIN_HAS_IMGUI_VULKAN
    if (g_Device != VK_NULL_HANDLE) { vkDeviceWaitIdle(g_Device); }
    ImGui_ImplVulkan_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    #endif
    
    // Cleanup
    shutdownVulkan();
    glfwDestroyWindow(window);
    glfwTerminate();

    // Export performance data if requested (also when in benchmark or smoke)
    if (benchmarkMode || hasArg(argc, argv, "--export-perf") || std::getenv("VOXELVK_EXPORT_PERF")) {
        voxelvk::PerformanceMonitor::instance().exportPerformanceData(perfOutDir);
    }
    
    g_logger.Info("VoxelVK Production App shutdown complete");
    return 0;
}