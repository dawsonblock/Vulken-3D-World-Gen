#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <functional>

namespace voxelvk {

/**
 * Device capabilities and feature probing for production reliability.
 * Guards all advanced features with runtime checks + fallbacks.
 */
struct DeviceCaps {
    // Core device info
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    
    // Extension availability
    bool hasDebugUtils = false;
    bool hasValidationLayers = false;
    bool hasPortabilitySubset = false;
    
    // Vulkan 1.2/1.3 features with fallbacks
    bool hasDescriptorIndexing = false;
    bool hasDynamicRendering = false;
    bool hasTimelineSemaphores = false;
    bool hasSubgroupOps = false;
    bool hasSynchronization2 = false;
    bool hasBufferDeviceAddress = false;
    
    // Memory and performance info
    size_t maxUniformBufferRange = 0;
    size_t maxStorageBufferRange = 0; 
    size_t deviceLocalHeapSize = 0;
    size_t hostVisibleHeapSize = 0;
    
    // Compute capabilities
    uint32_t maxComputeWorkGroupInvocations = 0;
    uint32_t maxComputeWorkGroupSize[3] = {0, 0, 0};
    
    // Graphics capabilities  
    uint32_t maxFramebufferWidth = 0;
    uint32_t maxFramebufferHeight = 0;
    uint32_t maxColorAttachments = 0;
    VkSampleCountFlags supportedSampleCounts = 0;
    
    // Weather system specific
    bool canDoComputeShaders = false;
    bool canDoStorageImages = false;
    bool canDoSamplerFiltering = false;
    
    static DeviceCaps probe(VkInstance instance, VkPhysicalDevice physicalDevice);
    bool isFeatureSafe(const char* featureName) const;
    void logCapabilities() const;
    
    // Fallback decision helpers
    bool shouldUseDescriptorIndexing() const { return hasDescriptorIndexing; }
    bool shouldUseDynamicRendering() const { return hasDynamicRendering; }
    bool shouldUseTimelineSemaphores() const { return hasTimelineSemaphores; }
    bool shouldUseSynchronization2() const { return hasSynchronization2; }
    
private:
    std::vector<VkExtensionProperties> availableExtensions;
    std::vector<VkLayerProperties> availableLayers;
};

/**
 * Device-lost detection and recovery system
 */
enum class DeviceStatus {
    OK,
    DEVICE_LOST,  
    OUT_OF_DATE_KHR,
    SUBOPTIMAL_KHR,
    SURFACE_LOST_KHR,
    UNKNOWN_ERROR
};

struct DeviceLostRecovery {
    DeviceStatus lastStatus = DeviceStatus::OK;
    uint32_t recoveryCount = 0;
    double lastRecoveryTime = 0.0;
    
    DeviceStatus checkDeviceStatus(VkDevice device, VkResult result);
    bool shouldAttemptRecovery() const;
    void recordRecovery();
    
    // Recovery callbacks
    std::function<void()> onDeviceLost;
    std::function<bool()> onAttemptRecovery;  // returns true if successful
    std::function<void()> onRecoverySuccess;
    std::function<void()> onRecoveryFailed;
};

} // namespace voxelvk