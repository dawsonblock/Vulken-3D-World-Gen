#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Remove Vulkan part; keep CPU profiling only
#include <tracy/Tracy.hpp>

#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

static const uint32_t WIDTH = 1280;
static const uint32_t HEIGHT = 720;

// Replace previous kEnableValidation/isValidationEnabled mix with a single helper
static bool isValidationEnabled() {
#ifdef NDEBUG
    return false;
#else
    return std::getenv("VULKAN_VALIDATION") != nullptr;
#endif
}

static const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*  pCallbackData,
    void*                                        /*pUserData*/) {
    (void)messageSeverity;
    (void)messageTypes;
    std::cerr << "[VK] " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

static bool checkValidationLayerSupport() {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (const char* name : kValidationLayers) {
        bool found = false;
        for (const auto& lp : layers) {
            if (std::strcmp(lp.layerName, name) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

static void populateDebugCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& ci) {
    ci = {};
    ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = debugCallback;
}

static bool g_framebufferResized = false;
static void framebufferResizeCallback(GLFWwindow*, int, int) {
    g_framebufferResized = true;
}

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

static void glfwErrorCallback(int code, const char* desc) {
    std::cerr << "GLFW error " << code << ": " << (desc ? desc : "") << std::endl;
}

static bool hasDisplay() {
    return std::getenv("DISPLAY") != nullptr || std::getenv("WAYLAND_DISPLAY") != nullptr;
}

class App {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    bool m_validationEnabled = false; // effective state

    // Swapchain and rendering
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapChainExtent{};
    std::vector<VkImageView> swapChainImageViews;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    size_t currentFrame = 0;

    void initWindow() {
        if (!hasDisplay()) {
            throw std::runtime_error(
                "No X11/Wayland display detected. Use 'bash scripts/run.sh --headless' or '--auto'.");
        }
        glfwSetErrorCallback(glfwErrorCallback);
        if (!glfwInit()) throw std::runtime_error("Failed to init GLFW");
        if (!glfwVulkanSupported()) throw std::runtime_error("Vulkan not supported by GLFW");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulken Viewer (GLFW + Vulkan)", nullptr, nullptr);
        if (!window) throw std::runtime_error("Failed to create GLFW window");
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    // Surface-dependent helpers
    static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surf) {
        QueueFamilyIndices indices;
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
        std::vector<VkQueueFamilyProperties> props(count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props.data());
        for (uint32_t i = 0; i < count; ++i) {
            if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surf, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }
            if (indices.isComplete()) break;
        }
        return indices;
    }

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice dev, VkSurfaceKHR surf) {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surf, &details.capabilities);
        uint32_t c = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surf, &c, nullptr);
        details.formats.resize(c);
        if (c) vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surf, &c, details.formats.data());
        vkGetPhysicalDevicePresentModesKHR(dev, surf, &c, nullptr);
        details.presentModes.resize(c);
        if (c) vkGetPhysicalDevicePresentModesKHR(dev, surf, &c, details.presentModes.data());
        return details;
    }

    static bool checkDeviceExtensionSupport(VkPhysicalDevice dev) {
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> exts(count);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, exts.data());
        bool hasSwapchain = false;
        for (const auto& e : exts) {
            if (std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchain = true;
                break;
            }
        }
        return hasSwapchain;
    }

    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return f;
            }
        }
        return formats.empty() ? VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR } : formats[0];
    }

    static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
        for (auto m : modes) {
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
        }
        return VK_PRESENT_MODE_FIFO_KHR; // always available
    }

   static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* win) {
        if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
        int w, h;
        glfwGetFramebufferSize(win, &w, &h);
        VkExtent2D actual{ static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
        actual.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, actual.width));
        actual.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, actual.height));
        return actual;
    }

    std::vector<const char*> getRequiredExtensions(bool enableDebug) {
        uint32_t count = 0;
        const char** exts = glfwGetRequiredInstanceExtensions(&count);
        if (!exts || count == 0) throw std::runtime_error("GLFW did not return required Vulkan instance extensions");

        std::vector<const char*> extensions(exts, exts + count);
        if (enableDebug) {
            // Add debug utils extension for validation messages
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        return extensions;
    }

    void createInstance() {
        bool enableValidation = isValidationEnabled();
#ifndef NDEBUG
        if (enableValidation && !checkValidationLayerSupport()) {
            std::cerr << "Warning: Validation layers requested but not available. Continuing without." << std::endl;
            enableValidation = false;
        }
#endif
        m_validationEnabled = enableValidation;

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Vulken 3D World Gen";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        std::vector<const char*> extensions = getRequiredExtensions(m_validationEnabled);

        VkInstanceCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.pApplicationInfo = &appInfo;
        ci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        ci.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCI{};
        if (m_validationEnabled) {
            ci.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
            ci.ppEnabledLayerNames = kValidationLayers.data();
            populateDebugCreateInfo(debugCI);
            ci.pNext = &debugCI; // get messages during instance creation
        } else {
            ci.enabledLayerCount = 0;
            ci.ppEnabledLayerNames = nullptr;
            ci.pNext = nullptr;
        }

        if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }
    }

    static PFN_vkCreateDebugUtilsMessengerEXT fpCreateDebug;
    static PFN_vkDestroyDebugUtilsMessengerEXT fpDestroyDebug;

    void setupDebugMessenger() {
        if (!m_validationEnabled) return;
        fpCreateDebug = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        fpDestroyDebug = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (!fpCreateDebug || !fpDestroyDebug) return;

        VkDebugUtilsMessengerCreateInfoEXT ci{};
        populateDebugCreateInfo(ci);
        if (fpCreateDebug(instance, &ci, nullptr, &debugMessenger) != VK_SUCCESS) {
            std::cerr << "Warning: Failed to create debug messenger" << std::endl;
        }
    }

    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface");
        }
    }

    bool isDeviceSuitable(VkPhysicalDevice dev) {
        if (!checkDeviceExtensionSupport(dev)) return false;
        auto q = findQueueFamilies(dev, surface);
        if (!q.isComplete()) return false;
        auto support = querySwapChainSupport(dev, surface);
        bool adequate = !support.formats.empty() && !support.presentModes.empty();
        return adequate;
    }

    void pickPhysicalDevice() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        if (count == 0) throw std::runtime_error("No Vulkan-capable GPUs found");
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance, &count, devices.data());

        // Prefer discrete GPU if suitable; fallback to any suitable device.
        int bestScore = -1;
        for (const auto& dev : devices) {
            if (!isDeviceSuitable(dev)) continue;
            VkPhysicalDeviceProperties p{};
            vkGetPhysicalDeviceProperties(dev, &p);
            int score = (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 2 : 1;
            if (score > bestScore) {
                bestScore = score;
                physicalDevice = dev;
            }
        }
        if (physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("Failed to find a suitable GPU");

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        std::cout << "Using GPU: " << props.deviceName << std::endl;
    }

    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
        std::set<uint32_t> uniqueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        float priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queues;
        for (uint32_t family : uniqueFamilies) {
            VkDeviceQueueCreateInfo qci{};
            qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qci.queueFamilyIndex = family;
            qci.queueCount = 1;
            qci.pQueuePriorities = &priority;
            queues.push_back(qci);
        }

        VkPhysicalDeviceFeatures features{};
        const char* deviceExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        VkDeviceCreateInfo dci{};
        dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dci.queueCreateInfoCount = static_cast<uint32_t>(queues.size());
        dci.pQueueCreateInfos = queues.data();
        dci.pEnabledFeatures = &features;
        dci.enabledExtensionCount = 1;
        dci.ppEnabledExtensionNames = deviceExts;

        if (vkCreateDevice(physicalDevice, &dci, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device");
        }
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }

    void createSwapChain() {
        auto support = querySwapChainSupport(physicalDevice, surface);
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.formats);
        VkPresentModeKHR presentMode = choosePresentMode(support.presentModes);
        VkExtent2D extent = chooseSwapExtent(support.capabilities, window);

        uint32_t imageCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
            imageCount = support.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR ci{};
        ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ci.surface = surface;
        ci.minImageCount = imageCount;
        ci.imageFormat = surfaceFormat.format;
        ci.imageColorSpace = surfaceFormat.colorSpace;
        ci.imageExtent = extent;
        ci.imageArrayLayers = 1;
        ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
        if (indices.graphicsFamily != indices.presentFamily) {
            ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            ci.queueFamilyIndexCount = 2;
            ci.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        ci.preTransform = support.capabilities.currentTransform;
        ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode = presentMode;
        ci.clipped = VK_TRUE;
        ci.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &ci, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swap chain");
        }

        uint32_t actualCount = 0;
        vkGetSwapchainImagesKHR(device, swapChain, &actualCount, nullptr);
        swapChainImages.resize(actualCount);
        vkGetSwapchainImagesKHR(device, swapChain, &actualCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); ++i) {
            VkImageViewCreateInfo ci{};
            ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ci.image = swapChainImages[i];
            ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ci.format = swapChainImageFormat;
            ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                              VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
            ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ci.subresourceRange.baseMipLevel = 0;
            ci.subresourceRange.levelCount = 1;
            ci.subresourceRange.baseArrayLayer = 0;
            ci.subresourceRange.layerCount = 1;
            if (vkCreateImageView(device, &ci, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image view");
            }
        }
    }

    void createRenderPass() {
        VkAttachmentDescription color{};
        color.format = swapChainImageFormat;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;

        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = 0;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        ci.attachmentCount = 1;
        ci.pAttachments = &color;
        ci.subpassCount = 1;
        ci.pSubpasses = &subpass;
        ci.dependencyCount = 1;
        ci.pDependencies = &dep;

        if (vkCreateRenderPass(device, &ci, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass");
        }
    }

    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); ++i) {
            VkImageView attachments[] = { swapChainImageViews[i] };
            VkFramebufferCreateInfo ci{};
            ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            ci.renderPass = renderPass;
            ci.attachmentCount = 1;
            ci.pAttachments = attachments;
            ci.width = swapChainExtent.width;
            ci.height = swapChainExtent.height;
            ci.layers = 1;
            if (vkCreateFramebuffer(device, &ci, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer");
            }
        }
    }

    void createCommandPool() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
        VkCommandPoolCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        ci.queueFamilyIndex = indices.graphicsFamily.value();
        ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(device, &ci, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool");
        }
    }

    void createCommandBuffers() {
        commandBuffers.resize(swapChainFramebuffers.size());

        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = commandPool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
        if (vkAllocateCommandBuffers(device, &ai, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers");
        }

        for (size_t i = 0; i < commandBuffers.size(); ++i) {
            VkCommandBufferBeginInfo bi{};
            bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(commandBuffers[i], &bi);

            VkClearValue clear{};
            clear.color = { { 0.05f, 0.1f, 0.15f, 1.0f } };

            VkRenderPassBeginInfo rp{};
            rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp.renderPass = renderPass;
            rp.framebuffer = swapChainFramebuffers[i];
            rp.renderArea.offset = { 0, 0 };
            rp.renderArea.extent = swapChainExtent;
            rp.clearValueCount = 1;
            rp.pClearValues = &clear;

            vkCmdBeginRenderPass(commandBuffers[i], &rp, VK_SUBPASS_CONTENTS_INLINE);
            // No pipeline yet: clear-only pass
            vkCmdEndRenderPass(commandBuffers[i]);

            if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to record command buffer");
            }
        }
    }

    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (vkCreateSemaphore(device, &si, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &si, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fi, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create sync objects");
            }
        }
    }

    void cleanupSwapChain() {
        for (auto fb : swapChainFramebuffers) vkDestroyFramebuffer(device, fb, nullptr);
        for (auto iv : swapChainImageViews) vkDestroyImageView(device, iv, nullptr);
        if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);
        if (swapChain) vkDestroySwapchainKHR(device, swapChain, nullptr);
    }

    void recreateSwapChain() {
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);
        while (w == 0 || h == 0) {
            glfwGetFramebufferSize(window, &w, &h);
            glfwWaitEvents();
        }
        vkDeviceWaitIdle(device);

        cleanupSwapChain();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createFramebuffers();
        // Re-record command buffers to use new framebuffers
        vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
        createCommandBuffers();
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createFramebuffers();
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            ZoneScopedN("MainLoopPoll");
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(device);
    }

    void drawFrame() {
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        uint32_t imageIndex = 0;
        VkResult res = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (res == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        }
        if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Failed to acquire swap chain image");
        }

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };

        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = waitSemaphores;
        si.pWaitDstStageMask = waitStages;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &commandBuffers[imageIndex];
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue, 1, &si, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer");
        }

        VkPresentInfoKHR pi{};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = signalSemaphores;
        pi.swapchainCount = 1;
        pi.pSwapchains = &swapChain;
        pi.pImageIndices = &imageIndex;

        res = vkQueuePresentKHR(presentQueue, &pi);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || g_framebufferResized) {
            g_framebufferResized = false;
            recreateSwapChain();
        } else if (res != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swap chain image");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void cleanup() {
        if (device) vkDeviceWaitIdle(device);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (imageAvailableSemaphores[i]) vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            if (renderFinishedSemaphores[i]) vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            if (inFlightFences[i]) vkDestroyFence(device, inFlightFences[i], nullptr);
        }
        if (commandPool && !commandBuffers.empty()) {
            vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
        }
        if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);

        cleanupSwapChain();

        if (device) vkDestroyDevice(device, nullptr);
        if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
        if (debugMessenger && fpDestroyDebug) {
            fpDestroyDebug(instance, debugMessenger, nullptr);
        }
        if (instance) vkDestroyInstance(instance, nullptr);
        if (window) {
            glfwDestroyWindow(window);
            glfwTerminate();
        }
    }
};

// Define static function pointers once (outside the class)
PFN_vkCreateDebugUtilsMessengerEXT App::fpCreateDebug = nullptr;
PFN_vkDestroyDebugUtilsMessengerEXT App::fpDestroyDebug = nullptr;

int main() {
    try {
        App app;
        app.run();
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
