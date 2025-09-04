#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <functional>

// Define missing Vulkan object type constants for compatibility
#ifndef VK_OBJECT_TYPE_COMPUTE_PIPELINE
#define VK_OBJECT_TYPE_COMPUTE_PIPELINE VK_OBJECT_TYPE_PIPELINE
#endif

namespace voxelvk {

// Centralized Vulkan error handling with actionable diagnostics
enum class VkErrorCategory {
    DEVICE_CREATION,
    SWAPCHAIN,
    MEMORY_ALLOCATION,
    PIPELINE_CREATION,
    RENDER_PASS,
    COMMAND_BUFFER,
    SYNCHRONIZATION,
    SHADER_COMPILATION,
    RESOURCE_CREATION,
    WEATHER_SYSTEM,
    UNKNOWN
};

struct VkErrorContext {
    VkResult result;
    const char* expression;
    const char* file;
    int line;
    const char* function;
    VkErrorCategory category;
    const char* objectName;
    uint64_t objectHandle;
};

// Error handler with recovery strategies
class VkErrorHandler {
public:
    using RecoveryFunction = std::function<bool(const VkErrorContext&)>;
    static void initialize();
    static void shutdown();
    static void handleError(const VkErrorContext& ctx);
    static void setRecoveryFunction(VkErrorCategory category, RecoveryFunction recovery);
    static void enableStructuredLogging(const std::string& logFilePath);
    static void setLogLevel(int level); // 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG
    static void setErrorRateLimit(int maxErrorsPerSecond);
private:
    static bool s_initialized;
    static std::string s_logFilePath;
    static int s_logLevel;
    static int s_maxErrorsPerSecond;
    static RecoveryFunction s_recoveryFunctions[static_cast<int>(VkErrorCategory::UNKNOWN) + 1];
    static void writeStructuredLog(const VkErrorContext& ctx, const std::string& message);
    static bool checkRateLimit();
    static const char* resultToString(VkResult result);
    static const char* categoryToString(VkErrorCategory category);
};

// VK_EXT_debug_utils integration for validation layers
class VkDebugUtils {
public:
    static bool initialize(VkInstance instance);
    static void shutdown(VkInstance instance);
    static void setObjectName(VkDevice device, uint64_t object, VkObjectType objectType, const char* name);
    static void beginLabel(VkCommandBuffer commandBuffer, const char* labelName, float color[4] = nullptr);
    static void endLabel(VkCommandBuffer commandBuffer);
    static void insertLabel(VkCommandBuffer commandBuffer, const char* labelName, float color[4] = nullptr);
    static void beginQueueLabel(VkQueue queue, const char* labelName, float color[4] = nullptr);
    static void endQueueLabel(VkQueue queue);
private:
    static VkDebugUtilsMessengerEXT s_debugMessenger;
    static PFN_vkSetDebugUtilsObjectNameEXT s_vkSetDebugUtilsObjectNameEXT;
    static PFN_vkCmdBeginDebugUtilsLabelEXT s_vkCmdBeginDebugUtilsLabelEXT;
    static PFN_vkCmdEndDebugUtilsLabelEXT s_vkCmdEndDebugUtilsLabelEXT;
    static PFN_vkCmdInsertDebugUtilsLabelEXT s_vkCmdInsertDebugUtilsLabelEXT;
    static PFN_vkQueueBeginDebugUtilsLabelEXT s_vkQueueBeginDebugUtilsLabelEXT;
    static PFN_vkQueueEndDebugUtilsLabelEXT s_vkQueueEndDebugUtilsLabelEXT;
    static VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
};

} // namespace voxelvk

// Primary error checking macros - use these everywhere
// Note: use parameter names that don't collide with VkErrorContext fields
#define CHECK_VK_RESULT(expr, err_category, obj_name)                      \
    do {                                                                  \
        VkResult _vk_result = (expr);                                     \
        if (_vk_result != VK_SUCCESS) {                                   \
            voxelvk::VkErrorContext _ctx{};                                \
            _ctx.result = _vk_result;                                      \
            _ctx.expression = #expr;                                       \
            _ctx.file = __FILE__;                                          \
            _ctx.line = __LINE__;                                          \
            _ctx.function = __func__;                                      \
            _ctx.category = (err_category);                                \
            _ctx.objectName = (obj_name);                                  \
            _ctx.objectHandle = 0;                                         \
            voxelvk::VkErrorHandler::handleError(_ctx);                    \
        }                                                                  \
    } while(0)

#define CHECK_VK(expr) CHECK_VK_RESULT(expr, voxelvk::VkErrorCategory::UNKNOWN, nullptr)
#define CHECK_VK_CATEGORY(expr, cat_) CHECK_VK_RESULT(expr, cat_, nullptr)
#define CHECK_VK_OBJECT(expr, cat_, objName_) CHECK_VK_RESULT(expr, cat_, objName_)

// RAII debug label for command buffers
#define VK_DEBUG_LABEL(commandBuffer, name)                                \
    struct VkDebugLabel_##__LINE__ {                                       \
        VkCommandBuffer cmd;                                               \
        VkDebugLabel_##__LINE__(VkCommandBuffer c, const char* n) : cmd(c) \
        { voxelvk::VkDebugUtils::beginLabel(cmd, n); }                     \
        ~VkDebugLabel_##__LINE__() { voxelvk::VkDebugUtils::endLabel(cmd);} \
    } _vk_debug_label_##__LINE__(commandBuffer, name)

#define VK_OBJECT_NAME(device, object, type, name)                         \
    voxelvk::VkDebugUtils::setObjectName(device, (uint64_t)(object), type, name)
