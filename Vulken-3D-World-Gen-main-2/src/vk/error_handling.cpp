#include "error_handling.hpp"
#include "../core/logger.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <atomic>
#include <unordered_map>

namespace voxelvk {

static Logger g_errorLogger("VkErrorHandler");

// VkErrorHandler implementation
bool VkErrorHandler::s_initialized = false;
std::string VkErrorHandler::s_logFilePath;
int VkErrorHandler::s_logLevel = 1; // WARN level by default
int VkErrorHandler::s_maxErrorsPerSecond = 10;
VkErrorHandler::RecoveryFunction VkErrorHandler::s_recoveryFunctions[static_cast<int>(VkErrorCategory::UNKNOWN) + 1];

void VkErrorHandler::initialize() {
    if (s_initialized) return;
    
    g_errorLogger.Info("Initializing VK error handling system");
    
    // Set default recovery functions
    setRecoveryFunction(VkErrorCategory::SWAPCHAIN, [](const VkErrorContext& ctx) -> bool {
        g_errorLogger.Info("Attempting swapchain recovery for error: {}", resultToString(ctx.result));
        // Swapchain recreation would be implemented here
        return false; // Not implemented yet
    });
    
    setRecoveryFunction(VkErrorCategory::DEVICE_CREATION, [](const VkErrorContext& ctx) -> bool {
        g_errorLogger.Error("Device creation failed - no recovery possible");
        return false;
    });
    
    s_initialized = true;
}

void VkErrorHandler::shutdown() {
    if (!s_initialized) return;
    
    g_errorLogger.Info("Shutting down VK error handling system");
    s_initialized = false;
}

void VkErrorHandler::handleError(const VkErrorContext& ctx) {
    if (!s_initialized) initialize();
    
    if (!checkRateLimit()) {
        return; // Rate limited
    }
    
    // Build error message
    std::ostringstream msg;
    msg << "Vulkan Error: " << resultToString(ctx.result)
        << " in " << ctx.expression
        << " at " << ctx.file << ":" << ctx.line
        << " (" << ctx.function << ")";
    
    if (ctx.objectName) {
        msg << " [object: " << ctx.objectName << "]";
    }
    
    msg << " [category: " << categoryToString(ctx.category) << "]";
    
    std::string errorMsg = msg.str();
    
    // Log based on severity 
    if (ctx.result == VK_ERROR_DEVICE_LOST) {
        g_errorLogger.Fatal(errorMsg);
    } else if (ctx.result == VK_ERROR_OUT_OF_HOST_MEMORY || ctx.result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
        g_errorLogger.Error(errorMsg);
    } else if (ctx.result == VK_ERROR_OUT_OF_DATE_KHR || ctx.result == VK_SUBOPTIMAL_KHR) {
        g_errorLogger.Warn(errorMsg);
    } else {
        g_errorLogger.Error(errorMsg);
    }
    
    // Write structured log if enabled
    if (!s_logFilePath.empty()) {
        writeStructuredLog(ctx, errorMsg);
    }
    
    // Attempt recovery if handler exists
    int categoryIndex = static_cast<int>(ctx.category);
    if (categoryIndex <= static_cast<int>(VkErrorCategory::UNKNOWN) && 
        s_recoveryFunctions[categoryIndex]) {
        
        g_errorLogger.Info("Attempting recovery for category: {}", categoryToString(ctx.category));
        bool recovered = s_recoveryFunctions[categoryIndex](ctx);
        
        if (recovered) {
            g_errorLogger.Info("Recovery successful");
        } else {
            g_errorLogger.Error("Recovery failed");
        }
    }
}

void VkErrorHandler::setRecoveryFunction(VkErrorCategory category, RecoveryFunction recovery) {
    int index = static_cast<int>(category);
    if (index <= static_cast<int>(VkErrorCategory::UNKNOWN)) {
        s_recoveryFunctions[index] = recovery;
    }
}

void VkErrorHandler::enableStructuredLogging(const std::string& logFilePath) {
    s_logFilePath = logFilePath;
    g_errorLogger.Info("Structured logging enabled: {}", logFilePath);
}

void VkErrorHandler::setLogLevel(int level) {
    s_logLevel = level;
}

void VkErrorHandler::setErrorRateLimit(int maxErrorsPerSecond) {
    s_maxErrorsPerSecond = maxErrorsPerSecond;
}

void VkErrorHandler::writeStructuredLog(const VkErrorContext& ctx, const std::string& message) {
    std::ofstream logFile(s_logFilePath, std::ios::app);
    if (!logFile.is_open()) return;
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    // JSON structured log
    logFile << "{"
            << "\"timestamp\":\"" << std::put_time(std::localtime(&time_t), "%Y-%m-%dT%H:%M:%S") << "\","
            << "\"level\":\"ERROR\","
            << "\"category\":\"" << categoryToString(ctx.category) << "\","
            << "\"result\":\"" << resultToString(ctx.result) << "\","
            << "\"expression\":\"" << ctx.expression << "\","
            << "\"file\":\"" << ctx.file << "\","
            << "\"line\":" << ctx.line << ","
            << "\"function\":\"" << ctx.function << "\","
            << "\"message\":\"" << message << "\"";
    
    if (ctx.objectName) {
        logFile << ",\"object\":\"" << ctx.objectName << "\"";
    }
    
    logFile << "}" << std::endl;
}

bool VkErrorHandler::checkRateLimit() {
    static std::atomic<int> errorCount{0};
    static std::chrono::steady_clock::time_point lastReset = std::chrono::steady_clock::now();
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastReset).count();
    
    if (elapsed >= 1) {
        // Reset counter every second
        errorCount.store(0);
        lastReset = now;
    }
    
    int currentCount = errorCount.fetch_add(1);
    if (currentCount >= s_maxErrorsPerSecond) {
        static bool rateLimitWarningLogged = false;
        if (!rateLimitWarningLogged) {
            g_errorLogger.Warn("VK error rate limit exceeded ({}/s), suppressing further errors", s_maxErrorsPerSecond);
            rateLimitWarningLogged = true;
        }
        return false;
    }
    
    return true;
}

const char* VkErrorHandler::resultToString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
        default: return "VK_ERROR_UNKNOWN";
    }
}

const char* VkErrorHandler::categoryToString(VkErrorCategory category) {
    switch (category) {
        case VkErrorCategory::DEVICE_CREATION: return "DEVICE_CREATION";
        case VkErrorCategory::SWAPCHAIN: return "SWAPCHAIN";
        case VkErrorCategory::MEMORY_ALLOCATION: return "MEMORY_ALLOCATION";
        case VkErrorCategory::PIPELINE_CREATION: return "PIPELINE_CREATION";
        case VkErrorCategory::RENDER_PASS: return "RENDER_PASS";
        case VkErrorCategory::COMMAND_BUFFER: return "COMMAND_BUFFER";
        case VkErrorCategory::SYNCHRONIZATION: return "SYNCHRONIZATION";
        case VkErrorCategory::SHADER_COMPILATION: return "SHADER_COMPILATION";
        case VkErrorCategory::RESOURCE_CREATION: return "RESOURCE_CREATION";
        case VkErrorCategory::WEATHER_SYSTEM: return "WEATHER_SYSTEM";
        case VkErrorCategory::UNKNOWN: return "UNKNOWN";
        default: return "INVALID_CATEGORY";
    }
}

// VkDebugUtils implementation
VkDebugUtilsMessengerEXT VkDebugUtils::s_debugMessenger = VK_NULL_HANDLE;
PFN_vkSetDebugUtilsObjectNameEXT VkDebugUtils::s_vkSetDebugUtilsObjectNameEXT = nullptr;
PFN_vkCmdBeginDebugUtilsLabelEXT VkDebugUtils::s_vkCmdBeginDebugUtilsLabelEXT = nullptr;
PFN_vkCmdEndDebugUtilsLabelEXT VkDebugUtils::s_vkCmdEndDebugUtilsLabelEXT = nullptr;
PFN_vkCmdInsertDebugUtilsLabelEXT VkDebugUtils::s_vkCmdInsertDebugUtilsLabelEXT = nullptr;
PFN_vkQueueBeginDebugUtilsLabelEXT VkDebugUtils::s_vkQueueBeginDebugUtilsLabelEXT = nullptr;
PFN_vkQueueEndDebugUtilsLabelEXT VkDebugUtils::s_vkQueueEndDebugUtilsLabelEXT = nullptr;

bool VkDebugUtils::initialize(VkInstance instance) {
    // Load debug utils function pointers
    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    
    if (!vkCreateDebugUtilsMessengerEXT) {
        g_errorLogger.Warn("VK_EXT_debug_utils not available");
        return false;
    }
    
    s_vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)
        vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
    s_vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)
        vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
    s_vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)
        vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");
    s_vkCmdInsertDebugUtilsLabelEXT = (PFN_vkCmdInsertDebugUtilsLabelEXT)
        vkGetInstanceProcAddr(instance, "vkCmdInsertDebugUtilsLabelEXT");
    s_vkQueueBeginDebugUtilsLabelEXT = (PFN_vkQueueBeginDebugUtilsLabelEXT)
        vkGetInstanceProcAddr(instance, "vkQueueBeginDebugUtilsLabelEXT");
    s_vkQueueEndDebugUtilsLabelEXT = (PFN_vkQueueEndDebugUtilsLabelEXT)
        vkGetInstanceProcAddr(instance, "vkQueueEndDebugUtilsLabelEXT");
    
    // Create debug messenger
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
    
    VkResult result = vkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &s_debugMessenger);
    if (result != VK_SUCCESS) {
        g_errorLogger.Error("Failed to create debug messenger");
        return false;
    }
    
    g_errorLogger.Info("VK_EXT_debug_utils initialized successfully");
    return true;
}

void VkDebugUtils::shutdown(VkInstance instance) {
    if (s_debugMessenger != VK_NULL_HANDLE) {
        auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        
        if (vkDestroyDebugUtilsMessengerEXT) {
            vkDestroyDebugUtilsMessengerEXT(instance, s_debugMessenger, nullptr);
        }
        
        s_debugMessenger = VK_NULL_HANDLE;
    }
}

void VkDebugUtils::setObjectName(VkDevice device, uint64_t object, VkObjectType objectType, const char* name) {
    if (!s_vkSetDebugUtilsObjectNameEXT || !name) return;
    
    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = object;
    nameInfo.pObjectName = name;
    
    s_vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
}

void VkDebugUtils::beginLabel(VkCommandBuffer commandBuffer, const char* labelName, float color[4]) {
    if (!s_vkCmdBeginDebugUtilsLabelEXT) return;
    
    VkDebugUtilsLabelEXT labelInfo{};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pLabelName = labelName;
    
    if (color) {
        labelInfo.color[0] = color[0];
        labelInfo.color[1] = color[1]; 
        labelInfo.color[2] = color[2];
        labelInfo.color[3] = color[3];
    } else {
        // Default color: light blue
        labelInfo.color[0] = 0.3f;
        labelInfo.color[1] = 0.7f;
        labelInfo.color[2] = 1.0f;
        labelInfo.color[3] = 1.0f;
    }
    
    s_vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &labelInfo);
}

void VkDebugUtils::endLabel(VkCommandBuffer commandBuffer) {
    if (s_vkCmdEndDebugUtilsLabelEXT) {
        s_vkCmdEndDebugUtilsLabelEXT(commandBuffer);
    }
}

void VkDebugUtils::insertLabel(VkCommandBuffer commandBuffer, const char* labelName, float color[4]) {
    if (!s_vkCmdInsertDebugUtilsLabelEXT) return;
    
    VkDebugUtilsLabelEXT labelInfo{};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pLabelName = labelName;
    
    if (color) {
        labelInfo.color[0] = color[0];
        labelInfo.color[1] = color[1];
        labelInfo.color[2] = color[2]; 
        labelInfo.color[3] = color[3];
    } else {
        labelInfo.color[0] = 1.0f;
        labelInfo.color[1] = 1.0f;
        labelInfo.color[2] = 0.3f;
        labelInfo.color[3] = 1.0f;
    }
    
    s_vkCmdInsertDebugUtilsLabelEXT(commandBuffer, &labelInfo);
}

void VkDebugUtils::beginQueueLabel(VkQueue queue, const char* labelName, float color[4]) {
    if (!s_vkQueueBeginDebugUtilsLabelEXT) return;
    
    VkDebugUtilsLabelEXT labelInfo{};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pLabelName = labelName;
    
    if (color) {
        labelInfo.color[0] = color[0];
        labelInfo.color[1] = color[1];
        labelInfo.color[2] = color[2];
        labelInfo.color[3] = color[3];
    } else {
        labelInfo.color[0] = 0.8f;
        labelInfo.color[1] = 0.3f;
        labelInfo.color[2] = 0.8f;
        labelInfo.color[3] = 1.0f;
    }
    
    s_vkQueueBeginDebugUtilsLabelEXT(queue, &labelInfo);
}

void VkDebugUtils::endQueueLabel(VkQueue queue) {
    if (s_vkQueueEndDebugUtilsLabelEXT) {
        s_vkQueueEndDebugUtilsLabelEXT(queue);
    }
}

VkBool32 VKAPI_CALL VkDebugUtils::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    (void)messageType;
    (void)pUserData;
    
    const char* severityStr;
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severityStr = "ERROR";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severityStr = "WARNING";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        severityStr = "INFO";
    } else {
        severityStr = "VERBOSE";
    }
    
    std::string message = std::string("Validation [") + severityStr + "]: " + pCallbackData->pMessage;
    
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        g_errorLogger.Error(message);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        g_errorLogger.Warn(message);
    } else {
        g_errorLogger.Debug(message);
    }
    
    return VK_FALSE;
}

} // namespace voxelvk