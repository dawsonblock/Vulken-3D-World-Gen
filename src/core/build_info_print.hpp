
#pragma once
#include <vulkan/vulkan.h>

namespace voxelvk {
void PrintBuildInfoOnce();             // prints to stdout and logs to file once
void PrintBuildInfoAlways();           // prints every call
void LogLine(const char* line);        // append to debug_vk_runtime.log
// Call on app start:
void InitBuildInfoLogging();
// Call on shutdown:
void ShutdownBuildInfoLogging();
} // namespace voxelvk
