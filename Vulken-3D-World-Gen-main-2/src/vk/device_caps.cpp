#include "device_caps.hpp"
#include "../core/logger.hpp"
#include <algorithm>
#include <cstring>
#include <chrono>

namespace voxelvk {

static Logger g_capsLogger("DeviceCaps");

DeviceCaps DeviceCaps::probe(VkInstance instance, VkPhysicalDevice physicalDevice) {
    DeviceCaps caps{};
    
    // Basic device properties
    vkGetPhysicalDeviceProperties(physicalDevice, &caps.properties);
    vkGetPhysicalDeviceFeatures(physicalDevice, &caps.features);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &caps.memoryProperties);
    
    g_capsLogger.Info("Probing device: {}", caps.properties.deviceName);
    g_capsLogger.Info("API Version: {}.{}.{}", 
        VK_VERSION_MAJOR(caps.properties.apiVersion),
        VK_VERSION_MINOR(caps.properties.apiVersion), 
        VK_VERSION_PATCH(caps.properties.apiVersion));
    
    // Query available extensions
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    caps.availableExtensions.resize(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, caps.availableExtensions.data());
    
    // Query available layers
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    caps.availableLayers.resize(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, caps.availableLayers.data());
    
    // Check for key extensions
    auto hasExtension = [&](const char* name) {
        return std::any_of(caps.availableExtensions.begin(), caps.availableExtensions.end(),
            [name](const VkExtensionProperties& ext) {
                return std::strcmp(ext.extensionName, name) == 0;
            });
    };
    
    auto hasLayer = [&](const char* name) {
        return std::any_of(caps.availableLayers.begin(), caps.availableLayers.end(),
            [name](const VkLayerProperties& layer) {
                return std::strcmp(layer.layerName, name) == 0;
            });
    };
    
    // Extension availability checks
    caps.hasDebugUtils = hasExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    caps.hasValidationLayers = hasLayer("VK_LAYER_KHRONOS_validation");
    caps.hasPortabilitySubset = false; // Disable for compatibility
    
    // Advanced Vulkan 1.2/1.3 feature detection
    if (caps.properties.apiVersion >= VK_API_VERSION_1_2) {
        // Query Vulkan 1.2 features
        VkPhysicalDeviceVulkan12Features vk12Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceFeatures2 features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        features2.pNext = &vk12Features;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
        
        caps.hasDescriptorIndexing = vk12Features.descriptorIndexing;
        caps.hasTimelineSemaphores = vk12Features.timelineSemaphore;
        caps.hasBufferDeviceAddress = vk12Features.bufferDeviceAddress;
        caps.hasSubgroupOps = true; // Available in Vulkan 1.1+
    }
    
    if (caps.properties.apiVersion >= VK_API_VERSION_1_3) {
        // Query Vulkan 1.3 features
        VkPhysicalDeviceVulkan13Features vk13Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceFeatures2 features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        features2.pNext = &vk13Features;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
        
        caps.hasDynamicRendering = vk13Features.dynamicRendering;
        caps.hasSynchronization2 = vk13Features.synchronization2;
    }
    
    // Extract key limits
    const auto& limits = caps.properties.limits;
    caps.maxUniformBufferRange = limits.maxUniformBufferRange;
    caps.maxStorageBufferRange = limits.maxStorageBufferRange;
    caps.maxComputeWorkGroupInvocations = limits.maxComputeWorkGroupInvocations;
    caps.maxComputeWorkGroupSize[0] = limits.maxComputeWorkGroupSize[0];
    caps.maxComputeWorkGroupSize[1] = limits.maxComputeWorkGroupSize[1];
    caps.maxComputeWorkGroupSize[2] = limits.maxComputeWorkGroupSize[2];
    caps.maxFramebufferWidth = limits.maxFramebufferWidth;
    caps.maxFramebufferHeight = limits.maxFramebufferHeight;
    caps.maxColorAttachments = limits.maxColorAttachments;
    caps.supportedSampleCounts = limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
    
    // Calculate memory heap sizes
    for (uint32_t i = 0; i < caps.memoryProperties.memoryHeapCount; i++) {
        const auto& heap = caps.memoryProperties.memoryHeaps[i];
        if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            caps.deviceLocalHeapSize = std::max(caps.deviceLocalHeapSize, (size_t)heap.size);
        } else {
            caps.hostVisibleHeapSize = std::max(caps.hostVisibleHeapSize, (size_t)heap.size);
        }
    }
    
    // Weather system capability checks
    caps.canDoComputeShaders = caps.features.shaderStorageImageExtendedFormats &&
                               caps.features.shaderStorageImageWriteWithoutFormat;
    caps.canDoStorageImages = caps.features.shaderStorageImageExtendedFormats;
    caps.canDoSamplerFiltering = true; // Basic requirement, always available
    
    return caps;
}

bool DeviceCaps::isFeatureSafe(const char* featureName) const {
    // Conservative feature safety check
    if (std::strcmp(featureName, "descriptorIndexing") == 0) return hasDescriptorIndexing;
    if (std::strcmp(featureName, "dynamicRendering") == 0) return hasDynamicRendering;
    if (std::strcmp(featureName, "timelineSemaphores") == 0) return hasTimelineSemaphores;
    if (std::strcmp(featureName, "synchronization2") == 0) return hasSynchronization2;
    if (std::strcmp(featureName, "subgroupOps") == 0) return hasSubgroupOps;
    if (std::strcmp(featureName, "bufferDeviceAddress") == 0) return hasBufferDeviceAddress;
    if (std::strcmp(featureName, "computeShaders") == 0) return canDoComputeShaders;
    if (std::strcmp(featureName, "storageImages") == 0) return canDoStorageImages;
    return false;
}

void DeviceCaps::logCapabilities() const {
    g_capsLogger.Info("=== Device Capabilities Report ===");
    g_capsLogger.Info("Device: {}", properties.deviceName);
    g_capsLogger.Info("Vendor: 0x{:X}, Device: 0x{:X}", properties.vendorID, properties.deviceID);
    g_capsLogger.Info("API Version: {}.{}.{}", 
        VK_VERSION_MAJOR(properties.apiVersion),
        VK_VERSION_MINOR(properties.apiVersion),
        VK_VERSION_PATCH(properties.apiVersion));
    
    g_capsLogger.Info("Extensions:");
    g_capsLogger.Info("  Debug Utils: {}", hasDebugUtils ? "YES" : "NO");
    g_capsLogger.Info("  Validation Layers: {}", hasValidationLayers ? "YES" : "NO");
    g_capsLogger.Info("  Descriptor Indexing: {}", hasDescriptorIndexing ? "YES" : "NO");
    g_capsLogger.Info("  Dynamic Rendering: {}", hasDynamicRendering ? "YES" : "NO");
    g_capsLogger.Info("  Timeline Semaphores: {}", hasTimelineSemaphores ? "YES" : "NO");
    g_capsLogger.Info("  Synchronization2: {}", hasSynchronization2 ? "YES" : "NO");
    
    g_capsLogger.Info("Memory Heaps:");
    g_capsLogger.Info("  Device Local: {:.1f} MB", deviceLocalHeapSize / (1024.0 * 1024.0));
    g_capsLogger.Info("  Host Visible: {:.1f} MB", hostVisibleHeapSize / (1024.0 * 1024.0));
    
    g_capsLogger.Info("Weather System:");
    g_capsLogger.Info("  Compute Shaders: {}", canDoComputeShaders ? "YES" : "NO");  
    g_capsLogger.Info("  Storage Images: {}", canDoStorageImages ? "YES" : "NO");
    g_capsLogger.Info("  Max Compute Workgroup: {}x{}x{}", 
        maxComputeWorkGroupSize[0], maxComputeWorkGroupSize[1], maxComputeWorkGroupSize[2]);
    
    g_capsLogger.Info("================================");
}

// Device Lost Recovery Implementation
DeviceStatus DeviceLostRecovery::checkDeviceStatus(VkDevice device, VkResult result) {
    switch (result) {
        case VK_SUCCESS:
            return DeviceStatus::OK;
            
        case VK_ERROR_DEVICE_LOST:
            if (lastStatus != DeviceStatus::DEVICE_LOST) {
                g_capsLogger.Error("Device lost detected!");
                if (onDeviceLost) onDeviceLost();
            }
            return DeviceStatus::DEVICE_LOST;
            
        case VK_ERROR_OUT_OF_DATE_KHR:
            g_capsLogger.Warn("Swapchain out of date, needs recreation");
            return DeviceStatus::OUT_OF_DATE_KHR;
            
        case VK_SUBOPTIMAL_KHR:
            g_capsLogger.Debug("Swapchain suboptimal, should recreate");  
            return DeviceStatus::SUBOPTIMAL_KHR;
            
        case VK_ERROR_SURFACE_LOST_KHR:
            g_capsLogger.Error("Surface lost!");
            return DeviceStatus::SURFACE_LOST_KHR;
            
        default:
            g_capsLogger.Error("Unknown Vulkan error: {}", static_cast<int32_t>(result));
            return DeviceStatus::UNKNOWN_ERROR;
    }
}

bool DeviceLostRecovery::shouldAttemptRecovery() const {
    if (recoveryCount >= 3) {
        g_capsLogger.Error("Maximum recovery attempts reached ({}), giving up", recoveryCount);
        return false;
    }
    
    // Rate limiting: don't attempt recovery more than once every 5 seconds
    auto now = std::chrono::steady_clock::now().time_since_epoch().count() / 1e9;
    if (now - lastRecoveryTime < 5.0) {
        g_capsLogger.Warn("Recovery rate limited (last attempt {:.1f}s ago)", now - lastRecoveryTime);
        return false;
    }
    
    return true;
}

void DeviceLostRecovery::recordRecovery() {
    recoveryCount++;
    lastRecoveryTime = std::chrono::steady_clock::now().time_since_epoch().count() / 1e9;
    g_capsLogger.Info("Recording device recovery attempt #{}", recoveryCount);
}

} // namespace voxelvk