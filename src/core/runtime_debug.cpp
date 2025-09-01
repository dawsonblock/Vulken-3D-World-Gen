
#include "runtime_debug.hpp"
#include "build_info_print.hpp"
#include <cstring>
#include <iostream>
#include <fstream>

namespace voxelvk {

static VKAPI_ATTR VkBool32 VKAPI_CALL cb_debug(
    VkDebugUtilsMessageSeverityFlagBitsEXT       severity,
    VkDebugUtilsMessageTypeFlagsEXT              types,
    const VkDebugUtilsMessengerCallbackDataEXT*  data,
    void*                                        user)
{
    (void)types; (void)user;
    // Print build info once on first message
    PrintBuildInfoOnce();
    const char* sev =
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR" :
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "WARN " :
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) ? "INFO " : "VERB ";
    char buf[4096];
    std::snprintf(buf, sizeof(buf), "[VK][%s] %s", sev, data && data->pMessage ? data->pMessage : "(null)");
    LogLine(buf);
    return VK_FALSE;
}

static PFN_vkCreateDebugUtilsMessengerEXT pfnCreateMessenger = nullptr;
static PFN_vkDestroyDebugUtilsMessengerEXT pfnDestroyMessenger = nullptr;

void DebugRuntime_AttachIfAvailable(DebugRuntime& dr){
    if(!dr.instance) return;
    if(!pfnCreateMessenger){
        pfnCreateMessenger  = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(dr.instance, "vkCreateDebugUtilsMessengerEXT"));
        pfnDestroyMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(dr.instance, "vkDestroyDebugUtilsMessengerEXT"));
    }
}

void DebugRuntime_Init(DebugRuntime& dr, VkInstance instance){
    dr.instance = instance;
    DebugRuntime_AttachIfAvailable(dr);
    if(!pfnCreateMessenger) return; // extension not available
    if(dr.messenger) return;
    VkDebugUtilsMessengerCreateInfoEXT ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = cb_debug;
    ci.pUserData = nullptr;
    if(pfnCreateMessenger(dr.instance, &ci, nullptr, &dr.messenger) == VK_SUCCESS){
        LogLine("[VK] Debug messenger attached (VK_EXT_debug_utils). Press F9 to toggle.");
        PrintBuildInfoOnce();
    }
}

void DebugRuntime_Shutdown(DebugRuntime& dr){
    if(dr.messenger && pfnDestroyMessenger){
        pfnDestroyMessenger(dr.instance, dr.messenger, nullptr);
        dr.messenger = VK_NULL_HANDLE;
    }
}

void DebugRuntime_SetEnabled(DebugRuntime& dr, bool on){
    dr.enabled.store(on);
    char msg[128];
    std::snprintf(msg, sizeof(msg), "[VK] Runtime debug %s", on ? "ENABLED" : "DISABLED");
    LogLine(msg);
}

void DebugRuntime_Toggle(DebugRuntime& dr){
    bool v = dr.enabled.load();
    DebugRuntime_SetEnabled(dr, !v);
}

} // namespace voxelvk
