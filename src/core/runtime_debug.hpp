
#pragma once
#ifndef VOXELVK_HEADLESS_ONLY
#include <vulkan/vulkan.h>
#else
// Forward declarations for headless mode
#include <cstdint>

typedef uint32_t VkFlags;
typedef uint32_t VkBool32;
typedef int32_t VkResult;

#ifndef VKAPI_PTR
#define VKAPI_PTR
#endif

typedef struct VkInstance_T* VkInstance;
typedef struct VkAllocationCallbacks VkAllocationCallbacks;
typedef struct VkDebugUtilsMessengerEXT_T* VkDebugUtilsMessengerEXT;
typedef struct VkDebugUtilsMessengerCallbackDataEXT_T* VkDebugUtilsMessengerCallbackDataEXT;
typedef struct VkDebugUtilsMessengerCreateInfoEXT VkDebugUtilsMessengerCreateInfoEXT;

// Proper enum and bitmask typedefs
typedef enum VkDebugUtilsMessageSeverityFlagBitsEXT {
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT = 0x00000001,
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT = 0x00000010,
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT = 0x00000100,
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT = 0x00001000,
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT = 0x7FFFFFFF
} VkDebugUtilsMessageSeverityFlagBitsEXT;

typedef enum VkDebugUtilsMessageTypeFlagBitsEXT {
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT = 0x00000001,
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT = 0x00000002,
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT = 0x00000004,
    VK_DEBUG_UTILS_MESSAGE_TYPE_FLAG_BITS_MAX_ENUM_EXT = 0x7FFFFFFF
} VkDebugUtilsMessageTypeFlagBitsEXT;

typedef VkFlags VkDebugUtilsMessageSeverityFlagsEXT;
typedef VkFlags VkDebugUtilsMessageTypeFlagsEXT;

// Proper function pointer typedefs with VKAPI_PTR
typedef VkBool32 (VKAPI_PTR *PFN_vkDebugUtilsMessengerCallbackEXT)(
    VkDebugUtilsMessageSeverityFlagsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

typedef VkResult (VKAPI_PTR *PFN_vkCreateDebugUtilsMessengerEXT)(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger);

typedef void (VKAPI_PTR *PFN_vkDestroyDebugUtilsMessengerEXT)(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* pAllocator);

typedef VkResult (VKAPI_PTR *PFN_vkSubmitDebugUtilsMessageEXT)(
    VkInstance instance,
    VkDebugUtilsMessageSeverityFlagsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData);
#define VK_NULL_HANDLE nullptr
#endif
#include <atomic>

namespace voxelvk {

struct DebugRuntime {
    std::atomic<bool> enabled{true}; // toggled by hotkey
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
};

// Create messenger if available; safe to call multiple times (no-op if already created)
void DebugRuntime_Init(DebugRuntime& dr, VkInstance instance);
// Destroy messenger if created
void DebugRuntime_Shutdown(DebugRuntime& dr);

// Toggle at runtime (e.g., GLFW key F9)
void DebugRuntime_SetEnabled(DebugRuntime& dr, bool on);
void DebugRuntime_Toggle(DebugRuntime& dr);

// Call to wire VK_EXT_debug_utils without needing validation layers enabled at startup
void DebugRuntime_AttachIfAvailable(DebugRuntime& dr);

} // namespace voxelvk
