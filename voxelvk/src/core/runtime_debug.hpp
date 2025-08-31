
#pragma once
#include <vulkan/vulkan.h>
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
