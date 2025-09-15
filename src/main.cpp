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

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    bool isComplete() const { return graphicsFamily.has_value(); }
};

static void glfwErrorCallback(int code, const char* desc) {
    std::cerr << "GLFW error " << code << ": " << (desc ? desc : "") << std::endl;
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
    bool m_validationEnabled = false; // effective state

    void initWindow() {
        glfwSetErrorCallback(glfwErrorCallback);
        if (!glfwInit()) throw std::runtime_error("Failed to init GLFW");
        if (!glfwVulkanSupported()) throw std::runtime_error("Vulkan not supported by GLFW");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulken Viewer (GLFW + Vulkan)", nullptr, nullptr);
        if (!window) throw std::runtime_error("Failed to create GLFW window");
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

    static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev) {
        QueueFamilyIndices indices;
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
        std::vector<VkQueueFamilyProperties> props(count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props.data());

        for (uint32_t i = 0; i < count; ++i) {
            if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
                break;
            }
        }
        return indices;
    }

    bool isDeviceSuitable(VkPhysicalDevice dev) {
        QueueFamilyIndices indices = findQueueFamilies(dev);
        return indices.isComplete();
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
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        std::set<uint32_t> uniqueFamilies = { indices.graphicsFamily.value() };

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
        VkDeviceCreateInfo dci{};
        dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dci.queueCreateInfoCount = static_cast<uint32_t>(queues.size());
        dci.pQueueCreateInfos = queues.data();
        dci.pEnabledFeatures = &features;

        // No device extensions needed for now (swapchain is not created yet).
        dci.enabledExtensionCount = 0;
        dci.ppEnabledExtensionNames = nullptr;

        if (vkCreateDevice(physicalDevice, &dci, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device");
        }
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            ZoneScopedN("MainLoopPoll"); // optional CPU zone
            glfwPollEvents();
        }
        vkDeviceWaitIdle(device);
    }

    void cleanup() {
        if (device) vkDeviceWaitIdle(device);

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
