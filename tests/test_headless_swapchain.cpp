#include <gtest/gtest.h>
#include <vulkan/vulkan.h>
#include <vector>

// Test headless swapchain creation (if display available)
class HeadlessSwapchainTest : public ::testing::Test {
protected:
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = UINT32_MAX;
    
    void SetUp() override {
        // Create minimal Vulkan instance
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "HeadlessTest";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;
        
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            GTEST_SKIP() << "Failed to create Vulkan instance";
        }
        
        // Get physical device
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            GTEST_SKIP() << "No Vulkan devices available";
        }
        
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        physicalDevice = devices[0];
        
        // Find graphics queue family
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
        
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsQueueFamily = i;
                break;
            }
        }
        
        if (graphicsQueueFamily == UINT32_MAX) {
            GTEST_SKIP() << "No graphics queue family found";
        }
        
        // Create logical device
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        
        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        
        if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
            GTEST_SKIP() << "Failed to create logical device";
        }
    }
    
    void TearDown() override {
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
        }
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
        }
    }
};

TEST_F(HeadlessSwapchainTest, DeviceCapabilities) {
    // Test that we can query device properties and features
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    
    EXPECT_GT(properties.limits.maxImageDimension2D, 0);
    EXPECT_GT(properties.limits.maxFramebufferWidth, 0);
    EXPECT_GT(properties.limits.maxFramebufferHeight, 0);
    
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);
    
    // Device should have basic rendering capabilities
    EXPECT_TRUE(properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_OTHER || 
                features.geometryShader || features.tessellationShader);
}

TEST_F(HeadlessSwapchainTest, MemoryProperties) {
    // Test memory type properties
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    
    EXPECT_GT(memProperties.memoryTypeCount, 0);
    EXPECT_LE(memProperties.memoryTypeCount, VK_MAX_MEMORY_TYPES);
    
    // Should have at least one device-local memory type
    bool hasDeviceLocal = false;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            hasDeviceLocal = true;
            break;
        }
    }
    EXPECT_TRUE(hasDeviceLocal);
}

TEST_F(HeadlessSwapchainTest, CommandPoolCreation) {
    // Test command pool and buffer creation
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsQueueFamily;
    
    VkCommandPool commandPool;
    ASSERT_EQ(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), VK_SUCCESS);
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer;
    ASSERT_EQ(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer), VK_SUCCESS);
    
    // Test command buffer recording
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    EXPECT_EQ(vkBeginCommandBuffer(commandBuffer, &beginInfo), VK_SUCCESS);
    EXPECT_EQ(vkEndCommandBuffer(commandBuffer), VK_SUCCESS);
    
    vkDestroyCommandPool(device, commandPool, nullptr);
}