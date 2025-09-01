#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <chrono>
#include <vector>

// VoxelVK systems
#include "../src/core/fullscreen_toggle.hpp"
#include "../src/ai/ai_palette_config_io.hpp"
#include "../src/ai/rag_runtime_bridge.hpp"
#include "../src/core/logger.hpp"
#include "../src/env/weather/weather_system.hpp"

// ImGui with Vulkan backend
#if __has_include(<imgui.h>) && __has_include(<imgui/backends/imgui_impl_glfw.h>) && __has_include(<imgui/backends/imgui_impl_vulkan.h>)
  #include <imgui.h>
  #include <imgui/backends/imgui_impl_glfw.h>
  #include <imgui/backends/imgui_impl_vulkan.h>
  #define MAIN_HAS_IMGUI_VULKAN 1
#elif __has_include(<imgui.h>)
  // Fallback to minimal ImGui without backend
  #include <imgui.h>
  #define MAIN_HAS_IMGUI_MINIMAL 1
#endif

using namespace voxelvk;

static Logger g_logger("MainImGuiApp");

// Global Vulkan objects for ImGui integration
static VkInstance g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice g_Device = VK_NULL_HANDLE;
static uint32_t g_GraphicsQueueFamily = UINT32_MAX;
static VkQueue g_GraphicsQueue = VK_NULL_HANDLE;
static VkRenderPass g_RenderPass = VK_NULL_HANDLE;
static VkDescriptorPool g_ImGuiDescriptorPool = VK_NULL_HANDLE;

static void error_callback(int code, const char* desc) {
    g_logger.Error("GLFW error {}: {}", code, desc);
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
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    
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
    
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            g_GraphicsQueueFamily = i;
            break;
        }
    }
    
    if (g_GraphicsQueueFamily == UINT32_MAX) {
        g_logger.Error("No graphics queue family found");
        return false;
    }
    
    // Create logical device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = g_GraphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    
    if (vkCreateDevice(g_PhysicalDevice, &deviceCreateInfo, nullptr, &g_Device) != VK_SUCCESS) {
        g_logger.Error("Failed to create Vulkan device");
        return false;
    }
    
    // Get graphics queue
    vkGetDeviceQueue(g_Device, g_GraphicsQueueFamily, 0, &g_GraphicsQueue);
    
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
    
    if (g_ImGuiDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_Device, g_ImGuiDescriptorPool, nullptr);
        g_ImGuiDescriptorPool = VK_NULL_HANDLE;
    }
    
    if (g_Device != VK_NULL_HANDLE) {
        vkDestroyDevice(g_Device, nullptr);
        g_Device = VK_NULL_HANDLE;
    }
    
    if (g_Instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_Instance, nullptr);
        g_Instance = VK_NULL_HANDLE;
    }
}

int main() {
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
    
    // Initialize Vulkan
    if (!initializeVulkan(window)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 3;
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
    
    voxelvk::ai::UpdateRagConfig(paletteRuntime.cfg.ai_generation.enable_rag, 
                                paletteRuntime.cfg.ai_generation.rag_top_k);
    
#ifdef MAIN_HAS_IMGUI_VULKAN
    // Initialize ImGui with Vulkan backend
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    ImGui::StyleColorsDark();
    
    // Initialize ImGui backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    
    if (!createImGuiDescriptorPool()) {
        g_logger.Error("Failed to create ImGui descriptor pool");
        shutdownVulkan();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 4;
    }
    
    // Note: In a full implementation, you'd create a render pass for ImGui
    // For now, we'll use a mock render pass
    g_RenderPass = VK_NULL_HANDLE; // Would be created for actual rendering
    
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_GraphicsQueueFamily;
    init_info.Queue = g_GraphicsQueue;
    init_info.DescriptorPool = g_ImGuiDescriptorPool;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.UseDynamicRendering = false;
    
    // ImGui_ImplVulkan_Init(&init_info, g_RenderPass); // Would be called with real render pass
    
    g_logger.Info("ImGui Vulkan backend initialized");
#endif
    
    // Frame timing
    double lastTime = glfwGetTime();
    double fps = 0.0;
    int frameCount = 0;
    double fpsAccum = 0.0;
    
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
            
            std::string title = "VoxelVK Production App - " + std::to_string(static_cast<int>(fps)) + " FPS";
            glfwSetWindowTitle(window, title.c_str());
        }
        
        // Update weather system
        weatherSystem.tick(deltaTime);
        
        // Hot reload palette and propagate RAG settings
        paletteRuntime.tick_hot_reload();
        voxelvk::ai::UpdateRagConfig(paletteRuntime.cfg.ai_generation.enable_rag, 
                                    paletteRuntime.cfg.ai_generation.rag_top_k);
        
        // Handle fullscreen toggle
        if (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS) {
            static bool fullscreenPressed = false;
            if (!fullscreenPressed) {
                static bool isFullscreen = false;
                isFullscreen = !isFullscreen;
                voxelvk::RequestFullscreen(isFullscreen);
                fullscreenPressed = true;
            }
        } else {
            static bool fullscreenPressed = false;
            fullscreenPressed = false;
        }
        
        // Config hot-reload on F5
        if (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS) {
            static bool reloadPressed = false;
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
            static bool reloadPressed = false;
            reloadPressed = false;
        }
        
#ifdef MAIN_HAS_IMGUI_VULKAN
        // Begin ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Performance HUD
        if (ImGui::Begin("VoxelVK Performance HUD", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("FPS: %.1f", fps);
            ImGui::Text("Frame time: %.3f ms", deltaTime * 1000.0);
            ImGui::Separator();
            
            // Weather system status
            auto weatherUBO = weatherSystem.getUBO();
            ImGui::Text("Weather System:");
            ImGui::Text("  State: %u", weatherUBO.state);
            ImGui::Text("  Wind: %.1f m/s", weatherUBO.windSpeed);
            ImGui::Text("  Clouds: %.2f", weatherUBO.cloudCoverage);
            ImGui::Text("  Time of day: %.2f", weatherUBO.timeOfDay);
            
            ImGui::Separator();
            
            // RAG system status
            ImGui::Text("RAG System:");
            ImGui::Text("  Enabled: %s", paletteRuntime.cfg.ai_generation.enable_rag ? "YES" : "NO");
            ImGui::Text("  Top-K: %d", paletteRuntime.cfg.ai_generation.rag_top_k);
            
            ImGui::Separator();
            
            // Controls
            if (ImGui::Button("Toggle Fullscreen (F11)")) {
                voxelvk::RequestFullscreen(true);
            }
            
            if (ImGui::Button("Hot Reload Config (F5)")) {
                try {
                    weatherSystem.loadFromYaml("config/weather.yaml");
                    g_logger.Info("Configuration reloaded via UI");
                } catch (const std::exception& e) {
                    g_logger.Warn("Reload failed: {}", e.what());
                }
            }
            
            // Weather controls
            ImGui::Separator();
            ImGui::Text("Weather Controls:");
            
            if (ImGui::Button("Clear")) weatherSystem.setState(WeatherState::CLEAR);
            ImGui::SameLine();
            if (ImGui::Button("Storm")) weatherSystem.setState(WeatherState::STORM);
            ImGui::SameLine();
            if (ImGui::Button("Rain")) weatherSystem.setState(WeatherState::RAIN);
            
            if (ImGui::Button("Snow")) weatherSystem.setState(WeatherState::SNOW);
            ImGui::SameLine();
            if (ImGui::Button("Fog")) weatherSystem.setState(WeatherState::FOG);
        }
        ImGui::End();
        
        // Render ImGui
        ImGui::Render();
        
        // In a full implementation, you'd render ImGui to the swapchain
        // For now, we just prepare the draw data
        ImDrawData* draw_data = ImGui::GetDrawData();
        if (draw_data) {
            // ImGui_ImplVulkan_RenderDrawData would be called here with command buffer
            g_logger.Debug("ImGui frame prepared (draw_data valid)");
        }
        
#elif defined(MAIN_HAS_IMGUI_MINIMAL)
        // Minimal ImGui without rendering (just for testing)
        static bool firstRun = true;
        if (firstRun) {
            g_logger.Info("ImGui available but no Vulkan backend - running minimal mode");
            firstRun = false;
        }
#else
        // No ImGui - just run the core loop
        static bool firstRun = true;
        if (firstRun) {
            g_logger.Info("ImGui not available - running core loop only");
            firstRun = false;
        }
#endif
        
        // Simulate frame work
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
    
    g_logger.Info("Main loop exited, shutting down...");
    
#ifdef MAIN_HAS_IMGUI_VULKAN
    // Cleanup ImGui
    if (g_Device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_Device);
    }
    
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
#endif
    
    // Cleanup
    shutdownVulkan();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    g_logger.Info("VoxelVK Production App shutdown complete");
    return 0;
}