#pragma once
#include <vulkan/vulkan.h>
#ifdef HAS_GLFW
#include <GLFW/glfw3.h>
#else
// Forward declare GLFWwindow to avoid including GLFW when not available
struct GLFWwindow;
#endif
#include <vector>
#include <functional>
#include "device_caps.hpp"
#include "error_handling.hpp"

namespace voxelvk {

/**
 * Production-grade swapchain manager with automatic recovery
 */
struct SwapchainSpec {
    VkSurfaceKHR surface;
    uint32_t width, height;
    VkPresentModeKHR preferredPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkFormat preferredFormat = VK_FORMAT_B8G8R8A8_UNORM;
    uint32_t imageCount = 3; // Triple buffering by default
    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    bool enableVSync = true;
};

struct SwapchainResources {
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    VkFormat format;
    VkExtent2D extent;
    uint32_t imageCount;
    
    // Synchronization objects
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    
    // Current frame state
    uint32_t currentFrame = 0;
    uint32_t imageIndex = 0;
    bool isValid = false;
};

class SwapchainManager {
public:
    SwapchainManager(VkDevice device, VkPhysicalDevice physicalDevice, const DeviceCaps& caps);
    ~SwapchainManager();
    
    // Primary interface
    bool create(const SwapchainSpec& spec);
    void destroy();
    
    // Frame operations with automatic recovery
    enum class AcquireResult {
        SUCCESS,
        OUT_OF_DATE,    // Swapchain needs recreation
        SUBOPTIMAL,     // Should recreate but can continue
        ERROR           // Fatal error
    };
    
    enum class PresentResult {
        SUCCESS,
        OUT_OF_DATE,    // Swapchain needs recreation
        DEVICE_LOST,    // Device lost - need full recovery
        ERROR           // Other error
    };
    
    AcquireResult acquireNextImage(uint64_t timeout = UINT64_MAX);
    PresentResult present(VkQueue presentQueue, const std::vector<VkSemaphore>& waitSemaphores = {});
    
    // Recovery operations
    bool recreate(uint32_t newWidth, uint32_t newHeight);
    bool recreate(); // Use current dimensions
    
    // State queries
    const SwapchainResources& getResources() const { return resources_; }
    bool isValid() const { return resources_.isValid; }
    VkExtent2D getExtent() const { return resources_.extent; }
    VkFormat getFormat() const { return resources_.format; }
    uint32_t getCurrentFrame() const { return resources_.currentFrame; }
    uint32_t getImageIndex() const { return resources_.imageIndex; }
    
    // Callbacks for application integration
    void setRecreationCallback(std::function<void(uint32_t, uint32_t)> callback) {
        onRecreation_ = callback;
    }
    
    void setDeviceLostCallback(std::function<void()> callback) {
        onDeviceLost_ = callback;
    }
    
private:
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    const DeviceCaps& caps_;
    SwapchainSpec spec_;
    SwapchainResources resources_;
    DeviceLostRecovery recovery_;
    
    // Callbacks
    std::function<void(uint32_t, uint32_t)> onRecreation_;
    std::function<void()> onDeviceLost_;
    
    // Internal methods
    bool createSwapchain();
    bool createImageViews();
    bool createSyncObjects();
    void destroyResources();
    
    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);
};

/**
 * GLFW integration helper for window resizing
 */
class GLFWSwapchainIntegration {
public:
    static void setupWindowCallbacks(GLFWwindow* window, SwapchainManager* swapchainManager);
    static void handleFramebufferResize(GLFWwindow* window, int width, int height);
    
private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
};

} // namespace voxelvk