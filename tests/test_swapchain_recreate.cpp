#include <gtest/gtest.h>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <memory>

class SwapchainRecreateTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize GLFW
        if (!glfwInit()) {
            GTEST_SKIP() << "Failed to initialize GLFW";
        }

        // Check if we can create a window
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(800, 600, "SwapchainTest", nullptr, nullptr);
        if (!window) {
            GTEST_SKIP() << "Failed to create GLFW window - skipping swapchain tests";
        }

        // Initialize Vulkan
        if (!initializeVulkan()) {
            GTEST_SKIP() << "Failed to initialize Vulkan";
        }
    }

    void TearDown() override {
        cleanupVulkan();
        if (window) {
            glfwDestroyWindow(window);
        }
        glfwTerminate();
    }

    bool initializeVulkan() {
        // Create instance
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "SwapchainTest";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // Enable required extensions
        const char* extensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef VK_USE_PLATFORM_WIN32_KHR
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
            VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
            VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#endif
        };
        createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
        createInfo.ppEnabledExtensionNames = extensions;

        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        if (result != VK_SUCCESS) {
            return false;
        }

        // Get physical device
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            return false;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &devices, nullptr);
        physicalDevice = devices[0];

        // Create surface
        VkResult surfaceResult = glfwCreateWindowSurface(instance, window, nullptr, &surface);
        if (surfaceResult != VK_SUCCESS) {
            return false;
        }

        // Create logical device
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = 0;
        queueCreateInfo.queueCount = 1;
        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        const char* deviceExtensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = 1;
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

        result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
        if (result != VK_SUCCESS) {
            return false;
        }

        // Get queue
        vkGetDeviceQueue(device, 0, 0, &queue);

        return true;
    }

    void cleanupVulkan() {
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapchain, nullptr);
        }
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        }
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
        }
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
        }
    }

    bool createSwapchain() {
        // Get surface capabilities
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

        // Get surface formats
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        if (formatCount == 0) {
            return false;
        }

        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

        // Get present modes
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
        if (presentModeCount == 0) {
            return false;
        }

        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

        // Create swapchain
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = capabilities.minImageCount;
        createInfo.imageFormat = formats[0].format;
        createInfo.imageColorSpace = formats[0].colorSpace;
        createInfo.imageExtent = capabilities.currentExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentModes[0];
        createInfo.clipped = VK_TRUE;

        VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain);
        return result == VK_SUCCESS;
    }

    bool recreateSwapchain() {
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }
        return createSwapchain();
    }

    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
};

TEST_F(SwapchainRecreateTest, CreateSwapchain) {
    EXPECT_TRUE(createSwapchain());
    EXPECT_NE(swapchain, VK_NULL_HANDLE);
}

TEST_F(SwapchainRecreateTest, RecreateSwapchain) {
    // Create initial swapchain
    ASSERT_TRUE(createSwapchain());

    // Recreate swapchain
    EXPECT_TRUE(recreateSwapchain());
    EXPECT_NE(swapchain, VK_NULL_HANDLE);
}

TEST_F(SwapchainRecreateTest, MultipleRecreates) {
    // Create initial swapchain
    ASSERT_TRUE(createSwapchain());

    // Recreate multiple times
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(recreateSwapchain());
        EXPECT_NE(swapchain, VK_NULL_HANDLE);
    }
}

TEST_F(SwapchainRecreateTest, GetSwapchainImages) {
    ASSERT_TRUE(createSwapchain());

    // Get swapchain images
    uint32_t imageCount;
    VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    EXPECT_EQ(result, VK_SUCCESS);
    EXPECT_GT(imageCount, 0);

    std::vector<VkImage> images(imageCount);
    result = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());
    EXPECT_EQ(result, VK_SUCCESS);

    for (const auto& image : images) {
        EXPECT_NE(image, VK_NULL_HANDLE);
    }
}

TEST_F(SwapchainRecreateTest, AcquireNextImage) {
    ASSERT_TRUE(createSwapchain());

    // Create semaphore
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore imageAvailableSemaphore;
    VkResult result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore);
    ASSERT_EQ(result, VK_SUCCESS);

    // Try to acquire next image
    uint32_t imageIndex;
    result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    // This might fail if no images are available, which is expected
    // The important thing is that the call doesn't crash
    EXPECT_TRUE(result == VK_SUCCESS || result == VK_NOT_READY || result == VK_TIMEOUT);

    vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
}
