#include <GLFW/glfw3.h>
#define GLFW_INCLUDE_VULKAN
#include <cstdio>
#include <cstdlib>
#include "../src/util/vk_pipeline_cache_utils.hpp"
#include <vector>
#include <optional>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <string>
#include "../src/core/fullscreen_toggle.hpp"
#include "redis_asset_store.h"
#include <iostream>

static VkPipelineCache g_pipelineCache = VK_NULL_HANDLE;

#include "../src/ai/ai_palette_config_io.hpp"
#include "../src/ai/rag_runtime_bridge.hpp"

static bool g_resize_requested = false;
static bool g_fullscreen_state = false;

struct QueueFamilyIndices { std::optional<uint32_t> graphics; std::optional<uint32_t> present; bool complete() const { return graphics.has_value() && present.has_value(); } };
struct SwapchainSupport { VkSurfaceCapabilitiesKHR caps{}; std::vector<VkSurfaceFormatKHR> formats; std::vector<VkPresentModeKHR> modes; };

static VKAPI_ATTR VkBool32 VKAPI_CALL dbg_cb(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user){ (void)type; (void)user; if(severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT){ std::fprintf(stderr, "[Vulkan] %s\n", data->pMessage); } return VK_FALSE; }

struct VulkanApp {
    GLFWwindow* window{}; VkInstance instance{}; VkDebugUtilsMessengerEXT debug{}; VkSurfaceKHR surface{}; VkPhysicalDevice phys{}; VkDevice device{}; VkQueue q_graphics{}; VkQueue q_present{}; uint32_t qf_graphics=0, qf_present=0;
    VkSwapchainKHR swapchain{}; VkFormat swap_format{}; VkExtent2D swap_extent{}; std::vector<VkImage> swap_images; std::vector<VkImageView> swap_views; VkRenderPass renderpass{}; std::vector<VkFramebuffer> framebuffers; VkCommandPool cmd_pool{}; std::vector<VkCommandBuffer> cmd_bufs; VkSemaphore sem_image_available{}; VkSemaphore sem_render_finished{}; VkFence in_flight{};

    void create(GLFWwindow* win){ window = win; create_instance(); create_debug(); create_surface(); pick_device(); create_device(); create_swapchain_and_dependents(); create_sync(); }
    void destroy(){ vkDeviceWaitIdle(device); destroy_swapchain_dependents(); if(in_flight) vkDestroyFence(device, in_flight, nullptr); if(sem_render_finished) vkDestroySemaphore(device, sem_render_finished, nullptr); if(sem_image_available) vkDestroySemaphore(device, sem_image_available, nullptr); if(device) voxelvk::util::save_pipeline_cache_to_env(device, g_pipelineCache);
        if(g_pipelineCache) vkDestroyPipelineCache(device, g_pipelineCache, nullptr);
        vkDestroyDevice(device, nullptr); if(surface) vkDestroySurfaceKHR(instance, surface, nullptr); if(debug){ auto p=(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT"); if(p) p(instance, debug, nullptr); } if(instance) vkDestroyInstance(instance, nullptr); }
    void recreate_swapchain(){ int w=0,h=0; glfwGetFramebufferSize(window,&w,&h); while(w==0||h==0){ glfwGetFramebufferSize(window,&w,&h); glfwWaitEvents(); } vkDeviceWaitIdle(device); destroy_swapchain_dependents(); create_swapchain_and_dependents(); }

    void frame(){
        uint32_t imgIndex=0; VkResult acq = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, sem_image_available, VK_NULL_HANDLE, &imgIndex);
        if(acq == VK_ERROR_OUT_OF_DATE_KHR){ recreate_swapchain(); return; }
        if(acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) throw std::runtime_error("Failed to acquire swapchain image");
        VkPipelineStageFlags waitStage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.waitSemaphoreCount=1; si.pWaitSemaphores=&sem_image_available; si.pWaitDstStageMask=&waitStage; si.commandBufferCount=1; VkCommandBuffer cb = cmd_bufs[imgIndex]; si.pCommandBuffers=&cb; si.signalSemaphoreCount=1; si.pSignalSemaphores=&sem_render_finished; vkResetFences(device,1,&in_flight); if(vkQueueSubmit(q_graphics,1,&si,in_flight)!=VK_SUCCESS) throw std::runtime_error("queue submit failed");
        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR}; pi.waitSemaphoreCount=1; pi.pWaitSemaphores=&sem_render_finished; pi.swapchainCount=1; pi.pSwapchains=&swapchain; pi.pImageIndices=&imgIndex; VkResult pres = vkQueuePresentKHR(q_present,&pi);
        if(pres==VK_ERROR_OUT_OF_DATE_KHR || pres==VK_SUBOPTIMAL_KHR){ recreate_swapchain(); }
        else if(pres!=VK_SUCCESS) throw std::runtime_error("present failed");
        vkWaitForFences(device,1,&in_flight,VK_TRUE,UINT64_MAX);
    }

    // ... [Other methods unchanged from previous version for brevity] ...
private:
    void create_instance(){ VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.pApplicationName = "vulkan_fullscreen_demo"; app.apiVersion = VK_API_VERSION_1_2; uint32_t ec=0; const char** exts = glfwGetRequiredInstanceExtensions(&ec); std::vector<const char*> extensions(exts, exts+ec); #ifndef NDEBUG extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); #endif VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo=&app; ici.enabledExtensionCount=(uint32_t)extensions.size(); ici.ppEnabledExtensionNames=extensions.data(); #ifndef NDEBUG const char* layers[] = {"VK_LAYER_KHRONOS_validation"}; ici.enabledLayerCount = 1; ici.ppEnabledLayerNames = layers; #endif if(vkCreateInstance(&ici,nullptr,&instance)!=VK_SUCCESS) throw std::runtime_error("vkCreateInstance failed"); }
    void create_debug(){ #ifndef NDEBUG auto p=(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT"); if(!p) return; VkDebugUtilsMessengerCreateInfoEXT ci{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT}; ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT; ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT; ci.pfnUserCallback = dbg_cb; p(instance,&ci,nullptr,&debug); #endif }
    void create_surface(){ if(glfwCreateWindowSurface(instance, window, nullptr, &surface)!=VK_SUCCESS) throw std::runtime_error("create surface failed"); }
    static QueueFamilyIndices find_queues(VkPhysicalDevice pd, VkSurfaceKHR surf){ QueueFamilyIndices idx{}; uint32_t n=0; vkGetPhysicalDeviceQueueFamilyProperties(pd,&n,nullptr); std::vector<VkQueueFamilyProperties> props(n); vkGetPhysicalDeviceQueueFamilyProperties(pd,&n,props.data()); for(uint32_t i=0;i<n;i++){ if(props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) idx.graphics = i; VkBool32 present=false; vkGetPhysicalDeviceSurfaceSupportKHR(pd,i,surf,&present); if(present) idx.present = i; if(idx.complete()) break; } return idx; }
    static SwapchainSupport query_swap(VkPhysicalDevice pd, VkSurfaceKHR surf){ SwapchainSupport s{}; uint32_t n=0; vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surf, &s.caps); vkGetPhysicalDeviceSurfaceFormatsKHR(pd,surf,&n,nullptr); s.formats.resize(n); if(n) vkGetPhysicalDeviceSurfaceFormatsKHR(pd,surf,&n,s.formats.data()); vkGetPhysicalDevicePresentModesKHR(pd,surf,&n,nullptr); s.modes.resize(n); if(n) vkGetPhysicalDevicePresentModesKHR(pd,surf,&n,s.modes.data()); return s; }
    void pick_device(){ uint32_t n=0; vkEnumeratePhysicalDevices(instance,&n,nullptr); if(!n) throw std::runtime_error("no device"); std::vector<VkPhysicalDevice> pds(n); vkEnumeratePhysicalDevices(instance,&n,pds.data()); for(auto pd: pds){ auto idx=find_queues(pd,surface); if(!idx.complete()) continue; auto sup=query_swap(pd,surface); if(sup.formats.empty()||sup.modes.empty()) continue; phys=pd; break; } if(!phys) throw std::runtime_error("no suitable device"); auto idx=find_queues(phys,surface); qf_graphics=*idx.graphics; qf_present=*idx.present; }
    void create_device(){ float prio=1.f; std::vector<VkDeviceQueueCreateInfo> qs; std::vector<uint32_t> unique = {qf_graphics}; if(qf_present!=qf_graphics) unique.push_back(qf_present); for(uint32_t qf: unique){ VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; q.queueFamilyIndex=qf; q.queueCount=1; q.pQueuePriorities=&prio; qs.push_back(q);} const char* exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME }; VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; dci.queueCreateInfoCount=(uint32_t)qs.size(); dci.pQueueCreateInfos=qs.data(); dci.enabledExtensionCount=1; dci.ppEnabledExtensionNames=exts; if(vkCreateDevice(phys,&dci,nullptr,&device)!=VK_SUCCESS) throw std::runtime_error("vkCreateDevice failed"); g_pipelineCache = voxelvk::util::create_pipeline_cache_from_env(device);
    vkGetDeviceQueue(device,qf_graphics,0,&q_graphics); vkGetDeviceQueue(device,qf_present,0,&q_present); }
    static VkSurfaceFormatKHR choose_format(const std::vector<VkSurfaceFormatKHR>& fmts){ for(auto& f: fmts){ if(f.format==VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f; } return fmts[0]; }
    static VkPresentModeKHR choose_mode(const std::vector<VkPresentModeKHR>& modes){ for(auto m: modes){ if(m==VK_PRESENT_MODE_MAILBOX_KHR) return m; } return VK_PRESENT_MODE_FIFO_KHR; }
    static VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* win){ if(caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) return caps.currentExtent; int w,h; glfwGetFramebufferSize(win,&w,&h); VkExtent2D e{(uint32_t)w,(uint32_t)h}; e.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, e.width)); e.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, e.height)); return e; }
    void create_swapchain_and_dependents(){ auto sup = query_swap(phys, surface); auto fmt = choose_format(sup.formats); auto mode = choose_mode(sup.modes); auto extent = choose_extent(sup.caps, window); uint32_t imageCount = sup.caps.minImageCount + 1; if(sup.caps.maxImageCount>0 && imageCount>sup.caps.maxImageCount) imageCount=sup.caps.maxImageCount; VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR}; ci.surface = surface; ci.minImageCount=imageCount; ci.imageFormat=fmt.format; ci.imageColorSpace=fmt.colorSpace; ci.imageExtent=extent; ci.imageArrayLayers=1; ci.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; uint32_t indices[] = {qf_graphics, qf_present}; if(qf_graphics!=qf_present){ ci.imageSharingMode=VK_SHARING_MODE_CONCURRENT; ci.queueFamilyIndexCount=2; ci.pQueueFamilyIndices=indices; } else { ci.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE; } ci.preTransform = sup.caps.currentTransform; ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; ci.presentMode = mode; ci.clipped=VK_TRUE; ci.oldSwapchain = VK_NULL_HANDLE; if(vkCreateSwapchainKHR(device,&ci,nullptr,&swapchain)!=VK_SUCCESS) throw std::runtime_error("create swapchain failed"); uint32_t n=0; vkGetSwapchainImagesKHR(device, swapchain, &n, nullptr); swap_images.resize(n); vkGetSwapchainImagesKHR(device, swapchain, &n, swap_images.data()); swap_format = fmt.format; swap_extent = extent; swap_views.resize(swap_images.size()); for(size_t i=0;i<swap_images.size();++i){ VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; vi.image=swap_images[i]; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=swap_format; vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT; vi.subresourceRange.levelCount=1; vi.subresourceRange.layerCount=1; if(vkCreateImageView(device,&vi,nullptr,&swap_views[i])!=VK_SUCCESS) throw std::runtime_error("image view failed"); } VkAttachmentDescription att{}; att.format=swap_format; att.samples=VK_SAMPLE_COUNT_1_BIT; att.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; att.storeOp=VK_ATTACHMENT_STORE_OP_STORE; att.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; att.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; VkAttachmentReference ref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}; VkSubpassDescription sub{}; sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS; sub.colorAttachmentCount=1; sub.pColorAttachments=&ref; VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; rpci.attachmentCount=1; rpci.pAttachments=&att; rpci.subpassCount=1; rpci.pSubpasses=&sub; if(vkCreateRenderPass(device,&rpci,nullptr,&renderpass)!=VK_SUCCESS) throw std::runtime_error("render pass failed"); framebuffers.resize(swap_views.size()); for(size_t i=0;i<swap_views.size();++i){ VkFramebufferCreateInfo fbi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO}; fbi.renderPass=renderpass; fbi.attachmentCount=1; fbi.pAttachments=&swap_views[i]; fbi.width=swap_extent.width; fbi.height=swap_extent.height; fbi.layers=1; if(vkCreateFramebuffer(device,&fbi,nullptr,&framebuffers[i])!=VK_SUCCESS) throw std::runtime_error("framebuffer failed"); } if(!cmd_pool){ VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; cp.queueFamilyIndex=qf_graphics; cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; if(vkCreateCommandPool(device,&cp,nullptr,&cmd_pool)!=VK_SUCCESS) throw std::runtime_error("cmd pool failed"); } cmd_bufs.resize(framebuffers.size()); VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; ai.commandPool=cmd_pool; ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount=(uint32_t)cmd_bufs.size(); if(vkAllocateCommandBuffers(device,&ai,cmd_bufs.data())!=VK_SUCCESS) throw std::runtime_error("alloc cmd failed"); for(size_t i=0;i<cmd_bufs.size();++i){ VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; vkBeginCommandBuffer(cmd_bufs[i],&bi); VkClearValue clear; clear.color = { {0.07f, 0.1f, 0.14f, 1.0f} }; VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; rbi.renderPass=renderpass; rbi.framebuffer=framebuffers[i]; rbi.renderArea.offset={0,0}; rbi.renderArea.extent=swap_extent; rbi.clearValueCount=1; rbi.pClearValues=&clear; vkCmdBeginRenderPass(cmd_bufs[i],&rbi,VK_SUBPASS_CONTENTS_INLINE); vkCmdEndRenderPass(cmd_bufs[i]); if(vkEndCommandBuffer(cmd_bufs[i])!=VK_SUCCESS) throw std::runtime_error("end cmd failed"); }
    }
    void destroy_swapchain_dependents(){ for(auto fb: framebuffers) if(fb) vkDestroyFramebuffer(device, fb, nullptr); framebuffers.clear(); if(renderpass) vkDestroyRenderPass(device, renderpass, nullptr); renderpass=nullptr; for(auto v: swap_views) if(v) vkDestroyImageView(device, v, nullptr); swap_views.clear(); if(swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr); swapchain=nullptr; if(!cmd_bufs.empty()) { vkFreeCommandBuffers(device, cmd_pool, (uint32_t)cmd_bufs.size(), cmd_bufs.data()); cmd_bufs.clear(); } }
    void create_sync(){ VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; fi.flags=VK_FENCE_CREATE_SIGNALED_BIT; if(vkCreateSemaphore(device,&si,nullptr,&sem_image_available)!=VK_SUCCESS) throw std::runtime_error("sem1 failed"); if(vkCreateSemaphore(device,&si,nullptr,&sem_render_finished)!=VK_SUCCESS) throw std::runtime_error("sem2 failed"); if(vkCreateFence(device,&fi,nullptr,&in_flight)!=VK_SUCCESS) throw std::runtime_error("fence failed"); }
};

static void framebuffer_size_cb(GLFWwindow* w, int, int){ (void)w; g_resize_requested=true; }

std::unique_ptr<RedisAssetStore> redis_store;

void init_redis() {
    try {
        redis_store = std::make_unique<RedisAssetStore>("config/redis.yaml");
        redis_store->set_reload_callback([](const std::string& asset_id) {
            // This callback will be invoked from a background thread.
            // Flag the asset for reload on the main thread.
            std::cout << "Hot-reload triggered for asset: " << asset_id << std::endl;
            // Example: add asset_id to a concurrent queue to be processed in the main loop
        });
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize RedisAssetStore: " << e.what() << std::endl;
    }
}

int main(){
    if(!glfwInit()){ std::fprintf(stderr, "Failed to init GLFW\n"); return 1; }
    if(!glfwVulkanSupported()){ std::fprintf(stderr, "Vulkan not supported by GLFW\n"); return 2; }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Vulkan Fullscreen Demo (Windowed)", nullptr, nullptr);
    if(!window){ std::fprintf(stderr, "Failed to create window\n"); glfwTerminate(); return 3; }
    glfwSetFramebufferSizeCallback(window, framebuffer_size_cb);

    static voxelvk::WindowedState ws; voxelvk::SetFullscreenHandler([window](bool on){ g_fullscreen_state=on; voxelvk::ToggleFullscreen(window,on,ws); g_resize_requested=true; });

    // RAG runtime wiring + palette hot reload for Vulkan demo
    voxelvk::ai::PaletteRuntime palette_rt; palette_rt.load_from_file("ai_palette.cfg");
    voxelvk::ai::SetRagConfigHandler([&](bool enabled, int topk){ std::printf("[RAG] (vk demo) enable=%d top_k=%d\n", (int)enabled, topk); });
    voxelvk::ai::UpdateRagConfig(palette_rt.cfg.ai_generation.enable_rag, palette_rt.cfg.ai_generation.rag_top_k);

    VulkanApp app;
    try { app.create(window); }
    catch(const std::exception& e){ std::fprintf(stderr, "Init error: %s\n", e.what()); glfwDestroyWindow(window); glfwTerminate(); return 4; }

    // Initialize Redis asset store
    init_redis();

    // Example of fetching an asset on startup
    if (redis_store) {
        MeshBlob rock_mesh;
        if (redis_store->fetch_mesh("rock01", rock_mesh)) {
            std::cout << "Successfully fetched mesh 'rock01' with size " << rock_mesh.data.size() << " bytes." << std::endl;
            // Upload to GPU or process the mesh data
        } else {
            std::cerr << "Failed to fetch mesh 'rock01'." << std::endl;
        }
    }

    double last = glfwGetTime(); double fps=0.0; double acc=0.0; int frames=0;

    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        try { app.frame(); }
        catch(const std::exception& e){ std::fprintf(stderr, "Frame error: %s\n", e.what()); break; }

        // Hot reload RAG config
        if(palette_rt.tick_hot_reload()){
            voxelvk::ai::UpdateRagConfig(palette_rt.cfg.ai_generation.enable_rag, palette_rt.cfg.ai_generation.rag_top_k);
        }

        // FPS calc and title update
        double now = glfwGetTime(); double dt = now-last; last=now; acc+=dt; frames++;
        if(acc>0.5){ fps = frames/acc; frames=0; acc=0.0; }
        std::string title = std::string("Vulkan Fullscreen Demo ") + (g_fullscreen_state?"(Fullscreen)":"(Windowed)") + " | FPS: " + std::to_string((int)fps);
        glfwSetWindowTitle(window, title.c_str());

        if(g_resize_requested){ g_resize_requested=false; app.recreate_swapchain(); }
    }

    app.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}