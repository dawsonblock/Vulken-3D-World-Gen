#include "swapchain_manager.hpp"
#include "../core/logger.hpp"
#include <algorithm>
#include <limits>

#ifdef VK_USE_PLATFORM_GLFW
#include <GLFW/glfw3.h>
#endif

namespace voxelvk {

static Logger g_swapchainLogger("SwapchainManager");

SwapchainManager::SwapchainManager(VkDevice device, VkPhysicalDevice physicalDevice, const DeviceCaps& caps)
    : device_(device), physicalDevice_(physicalDevice), caps_(caps) {
    
    g_swapchainLogger.Info("SwapchainManager initialized");
    
    // Set up device recovery callbacks
    recovery_.onDeviceLost = [this]() {
        g_swapchainLogger.Error("Device lost detected in swapchain");
        if (onDeviceLost_) onDeviceLost_();
    };
    
    recovery_.onAttemptRecovery = [this]() -> bool {
        g_swapchainLogger.Info("Attempting swapchain recovery");
        return recreate();
    };
    
    recovery_.onRecoverySuccess = [this]() {
        g_swapchainLogger.Info("Swapchain recovery successful");
    };
    
    recovery_.onRecoveryFailed = [this]() {
        g_swapchainLogger.Error("Swapchain recovery failed");
    };
}

SwapchainManager::~SwapchainManager() {
    destroy();
}

bool SwapchainManager::create(const SwapchainSpec& spec) {
    g_swapchainLogger.Info("Creating swapchain: {}x{}", spec.width, spec.height);
    
    spec_ = spec;
    
    if (!createSwapchain()) {
        g_swapchainLogger.Error("Failed to create swapchain");
        return false;
    }
    
    if (!createImageViews()) {
        g_swapchainLogger.Error("Failed to create image views");
        destroyResources();
        return false;
    }
    
    if (!createSyncObjects()) {
        g_swapchainLogger.Error("Failed to create synchronization objects");
        destroyResources();
        return false;
    }
    
    resources_.isValid = true;
    g_swapchainLogger.Info("Swapchain created successfully: {} images, {}x{}", 
        resources_.imageCount, resources_.extent.width, resources_.extent.height);
    
    return true;
}

void SwapchainManager::destroy() {
    if (device_ == VK_NULL_HANDLE) return;
    
    g_swapchainLogger.Info("Destroying swapchain");
    
    // Wait for device idle before cleanup
    CHECK_VK_CATEGORY(vkDeviceWaitIdle(device_), VkErrorCategory::SYNCHRONIZATION);
    
    destroyResources();
}

SwapchainManager::AcquireResult SwapchainManager::acquireNextImage(uint64_t timeout) {
    if (!resources_.isValid) {
        return AcquireResult::ERROR;
    }
    
    // Wait for previous frame
    CHECK_VK_CATEGORY(
        vkWaitForFences(device_, 1, &resources_.inFlightFences[resources_.currentFrame], VK_TRUE, timeout),
        VkErrorCategory::SYNCHRONIZATION
    );
    
    VkResult result = vkAcquireNextImageKHR(
        device_, 
        resources_.swapchain,
        timeout,
        resources_.imageAvailableSemaphores[resources_.currentFrame],
        VK_NULL_HANDLE,
        &resources_.imageIndex
    );
    
    DeviceStatus status = recovery_.checkDeviceStatus(device_, result);
    
    switch (status) {
        case DeviceStatus::OK:
            CHECK_VK_CATEGORY(
                vkResetFences(device_, 1, &resources_.inFlightFences[resources_.currentFrame]),
                VkErrorCategory::SYNCHRONIZATION
            );
            return AcquireResult::SUCCESS;
            
        case DeviceStatus::OUT_OF_DATE_KHR:
            g_swapchainLogger.Info("Swapchain out of date, needs recreation");
            return AcquireResult::OUT_OF_DATE;
            
        case DeviceStatus::SUBOPTIMAL_KHR:
            g_swapchainLogger.Debug("Swapchain suboptimal, should recreate");
            CHECK_VK_CATEGORY(
                vkResetFences(device_, 1, &resources_.inFlightFences[resources_.currentFrame]),
                VkErrorCategory::SYNCHRONIZATION
            );
            return AcquireResult::SUBOPTIMAL;
            
        case DeviceStatus::DEVICE_LOST:
            if (recovery_.shouldAttemptRecovery()) {
                recovery_.recordRecovery();
                if (recovery_.onAttemptRecovery && recovery_.onAttemptRecovery()) {
                    recovery_.onRecoverySuccess();
                    return AcquireResult::SUCCESS;
                } else {
                    recovery_.onRecoveryFailed();
                }
            }
            return AcquireResult::ERROR;
            
        default:
            g_swapchainLogger.Error("Failed to acquire next image");
            return AcquireResult::ERROR;
    }
}

SwapchainManager::PresentResult SwapchainManager::present(VkQueue presentQueue, const std::vector<VkSemaphore>& waitSemaphores) {
    if (!resources_.isValid) {
        return PresentResult::ERROR;
    }
    
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    
    // Wait semaphores
    std::vector<VkSemaphore> allWaitSemaphores = waitSemaphores;
    allWaitSemaphores.push_back(resources_.renderFinishedSemaphores[resources_.currentFrame]);
    
    presentInfo.waitSemaphoreCount = static_cast<uint32_t>(allWaitSemaphores.size());
    presentInfo.pWaitSemaphores = allWaitSemaphores.data();
    
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &resources_.swapchain;
    presentInfo.pImageIndices = &resources_.imageIndex;
    presentInfo.pResults = nullptr; // Optional
    
    VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);
    
    // Advance to next frame
    resources_.currentFrame = (resources_.currentFrame + 1) % resources_.imageCount;
    
    DeviceStatus status = recovery_.checkDeviceStatus(device_, result);
    
    switch (status) {
        case DeviceStatus::OK:
            return PresentResult::SUCCESS;
            
        case DeviceStatus::OUT_OF_DATE_KHR:
        case DeviceStatus::SUBOPTIMAL_KHR:
            return PresentResult::OUT_OF_DATE;
            
        case DeviceStatus::DEVICE_LOST:
            return PresentResult::DEVICE_LOST;
            
        default:
            return PresentResult::ERROR;
    }
}

bool SwapchainManager::recreate(uint32_t newWidth, uint32_t newHeight) {
    g_swapchainLogger.Info("Recreating swapchain: {}x{} -> {}x{}", 
        spec_.width, spec_.height, newWidth, newHeight);
    
    // Wait for device idle
    CHECK_VK_CATEGORY(vkDeviceWaitIdle(device_), VkErrorCategory::SYNCHRONIZATION);
    
    // Update spec
    spec_.width = newWidth;
    spec_.height = newHeight;
    
    // Clean up old resources
    destroyResources();
    
    // Create new swapchain
    bool success = createSwapchain() && createImageViews() && createSyncObjects();
    
    if (success) {
        resources_.isValid = true;
        resources_.currentFrame = 0;
        
        g_swapchainLogger.Info("Swapchain recreation successful");
        
        if (onRecreation_) {
            onRecreation_(newWidth, newHeight);
        }
    } else {
        g_swapchainLogger.Error("Swapchain recreation failed");
        resources_.isValid = false;
    }
    
    return success;
}

bool SwapchainManager::recreate() {
    return recreate(spec_.width, spec_.height);
}

bool SwapchainManager::createSwapchain() {
    // Query surface capabilities
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    CHECK_VK_CATEGORY(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, spec_.surface, &surfaceCapabilities),
        VkErrorCategory::SWAPCHAIN
    );
    
    // Query surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, spec_.surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, spec_.surface, &formatCount, surfaceFormats.data());
    
    // Query present modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, spec_.surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, spec_.surface, &presentModeCount, presentModes.data());
    
    // Choose optimal settings
    VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(surfaceFormats);
    VkPresentModeKHR presentMode = choosePresentMode(presentModes);
    VkExtent2D extent = chooseSwapExtent(surfaceCapabilities, spec_.width, spec_.height);
    
    // Determine image count
    uint32_t imageCount = spec_.imageCount;
    if (surfaceCapabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, surfaceCapabilities.maxImageCount);
    }
    imageCount = std::max(imageCount, surfaceCapabilities.minImageCount);
    
    // Create swapchain
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = spec_.surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = spec_.imageUsage;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = surfaceCapabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = resources_.swapchain; // For recreation
    
    VkSwapchainKHR newSwapchain;
    VkResult result = vkCreateSwapchainKHR(device_, &createInfo, nullptr, &newSwapchain);
    
    if (result != VK_SUCCESS) {
        CHECK_VK_OBJECT(result, VkErrorCategory::SWAPCHAIN, "swapchain_creation");
        return false;
    }
    
    // Clean up old swapchain if it exists
    if (resources_.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, resources_.swapchain, nullptr);
    }
    
    resources_.swapchain = newSwapchain;
    resources_.format = surfaceFormat.format;
    resources_.extent = extent;
    resources_.imageCount = imageCount;
    
    // Get swapchain images
    vkGetSwapchainImagesKHR(device_, resources_.swapchain, &imageCount, nullptr);
    resources_.images.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, resources_.swapchain, &imageCount, resources_.images.data());
    
    VK_OBJECT_NAME(device_, resources_.swapchain, VK_OBJECT_TYPE_SWAPCHAIN_KHR, "MainSwapchain");
    
    return true;
}

bool SwapchainManager::createImageViews() {
    resources_.imageViews.resize(resources_.images.size());
    
    for (size_t i = 0; i < resources_.images.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = resources_.images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = resources_.format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        
        VkResult result = vkCreateImageView(device_, &createInfo, nullptr, &resources_.imageViews[i]);
        if (result != VK_SUCCESS) {
            CHECK_VK_OBJECT(result, VkErrorCategory::RESOURCE_CREATION, "swapchain_image_view");
            return false;
        }
        
        // Label for debugging
        std::string name = "SwapchainImageView_" + std::to_string(i);
        VK_OBJECT_NAME(device_, resources_.imageViews[i], VK_OBJECT_TYPE_IMAGE_VIEW, name.c_str());
    }
    
    return true;
}

bool SwapchainManager::createSyncObjects() {
    resources_.imageAvailableSemaphores.resize(resources_.imageCount);
    resources_.renderFinishedSemaphores.resize(resources_.imageCount);
    resources_.inFlightFences.resize(resources_.imageCount);
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled
    
    for (uint32_t i = 0; i < resources_.imageCount; i++) {
        VkResult result;
        
        result = vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &resources_.imageAvailableSemaphores[i]);
        if (result != VK_SUCCESS) {
            CHECK_VK_OBJECT(result, VkErrorCategory::SYNCHRONIZATION, "image_available_semaphore");
            return false;
        }
        
        result = vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &resources_.renderFinishedSemaphores[i]);
        if (result != VK_SUCCESS) {
            CHECK_VK_OBJECT(result, VkErrorCategory::SYNCHRONIZATION, "render_finished_semaphore");
            return false;
        }
        
        result = vkCreateFence(device_, &fenceInfo, nullptr, &resources_.inFlightFences[i]);
        if (result != VK_SUCCESS) {
            CHECK_VK_OBJECT(result, VkErrorCategory::SYNCHRONIZATION, "in_flight_fence");
            return false;
        }
        
        // Label for debugging
        std::string availName = "ImageAvailable_" + std::to_string(i);
        std::string finishedName = "RenderFinished_" + std::to_string(i);
        std::string fenceName = "InFlight_" + std::to_string(i);
        
        VK_OBJECT_NAME(device_, resources_.imageAvailableSemaphores[i], VK_OBJECT_TYPE_SEMAPHORE, availName.c_str());
        VK_OBJECT_NAME(device_, resources_.renderFinishedSemaphores[i], VK_OBJECT_TYPE_SEMAPHORE, finishedName.c_str());
        VK_OBJECT_NAME(device_, resources_.inFlightFences[i], VK_OBJECT_TYPE_FENCE, fenceName.c_str());
    }
    
    return true;
}

void SwapchainManager::destroyResources() {
    // Sync objects
    for (size_t i = 0; i < resources_.imageAvailableSemaphores.size(); i++) {
        if (resources_.imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, resources_.imageAvailableSemaphores[i], nullptr);
        }
    }
    resources_.imageAvailableSemaphores.clear();
    
    for (size_t i = 0; i < resources_.renderFinishedSemaphores.size(); i++) {
        if (resources_.renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, resources_.renderFinishedSemaphores[i], nullptr);
        }
    }
    resources_.renderFinishedSemaphores.clear();
    
    for (size_t i = 0; i < resources_.inFlightFences.size(); i++) {
        if (resources_.inFlightFences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(device_, resources_.inFlightFences[i], nullptr);
        }
    }
    resources_.inFlightFences.clear();
    
    // Image views
    for (auto imageView : resources_.imageViews) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, imageView, nullptr);
        }
    }
    resources_.imageViews.clear();
    
    // Swapchain
    if (resources_.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, resources_.swapchain, nullptr);
        resources_.swapchain = VK_NULL_HANDLE;
    }
    
    resources_.images.clear();
    resources_.isValid = false;
}

VkSurfaceFormatKHR SwapchainManager::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    // Prefer sRGB if available
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == spec_.preferredFormat &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    
    // Fallback to first available
    return availableFormats[0];
}

VkPresentModeKHR SwapchainManager::choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    // If VSync is disabled, prefer mailbox mode for low latency
    if (!spec_.enableVSync) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        
        // Fallback to immediate mode
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                return availablePresentMode;
            }
        }
    }
    
    // Check for preferred mode
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == spec_.preferredPresentMode) {
            return availablePresentMode;
        }
    }
    
    // FIFO is guaranteed to be available
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapchainManager::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    
    VkExtent2D actualExtent = {width, height};
    
    actualExtent.width = std::clamp(actualExtent.width, 
        capabilities.minImageExtent.width, 
        capabilities.maxImageExtent.width);
    
    actualExtent.height = std::clamp(actualExtent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);
    
    return actualExtent;
}

// GLFW Integration
#ifdef VK_USE_PLATFORM_GLFW
void GLFWSwapchainIntegration::setupWindowCallbacks(GLFWwindow* window, SwapchainManager* swapchainManager) {
    glfwSetWindowUserPointer(window, swapchainManager);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void GLFWSwapchainIntegration::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto* swapchainManager = static_cast<SwapchainManager*>(glfwGetWindowUserPointer(window));
    if (swapchainManager && width > 0 && height > 0) {
        swapchainManager->recreate(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    }
}
#endif

} // namespace voxelvk