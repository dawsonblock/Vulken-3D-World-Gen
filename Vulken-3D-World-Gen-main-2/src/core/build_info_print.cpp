
#include "build_info_print.hpp"
#include "build_info.hpp"
#include <cstdio>
#include <mutex>

namespace voxelvk {
static std::once_flag g_once;
static FILE* g_log = nullptr;

void InitBuildInfoLogging(){
    if(!g_log){
        g_log = std::fopen("debug_vk_runtime.log", "a");
        if(g_log){
            std::setvbuf(g_log, nullptr, _IOLBF, 0); // line-buffered
        }
    }
}

void ShutdownBuildInfoLogging(){
    if(g_log){ std::fclose(g_log); g_log=nullptr; }
}

void LogLine(const char* line){
    if(!g_log) InitBuildInfoLogging();
    if(g_log){ std::fputs(line, g_log); std::fputc('\n', g_log); }
    std::fputs(line, stdout); std::fputc('\n', stdout);
}

static void _print(){
    std::printf("[BUILD INFO]\n");
    std::printf("Branch Sources: %s\n", VOXELVK_BUILD_BRANCH_SOURCES);
    std::printf("Build Time: %s\n", VOXELVK_BUILD_TIME);
    std::printf("Build Hash: %s\n", VOXELVK_BUILD_HASH);
    std::printf("Vulkan SDK: %s\n", VOXELVK_VULKAN_SDK_VERSION);
#if VOXELVK_GPU_COMPAT_LAYER
    std::printf("GPU Compatibility Layer: ENABLED\n");
#else
    std::printf("GPU Compatibility Layer: DISABLED\n");
#endif
#if VOXELVK_RUNTIME_DEBUG_TOGGLE
    std::printf("Runtime Vulkan Debug Toggle: ENABLED (Hotkey: F9)\n");
#else
    std::printf("Runtime Vulkan Debug Toggle: DISABLED\n");
#endif
    if(g_log){
        std::fprintf(g_log, "[BUILD INFO]\n");
        std::fprintf(g_log, "Branch Sources: %s\n", VOXELVK_BUILD_BRANCH_SOURCES);
        std::fprintf(g_log, "Build Time: %s\n", VOXELVK_BUILD_TIME);
        std::fprintf(g_log, "Build Hash: %s\n", VOXELVK_BUILD_HASH);
        std::fprintf(g_log, "Vulkan SDK: %s\n", VOXELVK_VULKAN_SDK_VERSION);
#if VOXELVK_GPU_COMPAT_LAYER
        std::fprintf(g_log, "GPU Compatibility Layer: ENABLED\n");
#else
        std::fprintf(g_log, "GPU Compatibility Layer: DISABLED\n");
#endif
#if VOXELVK_RUNTIME_DEBUG_TOGGLE
        std::fprintf(g_log, "Runtime Vulkan Debug Toggle: ENABLED (Hotkey: F9)\n");
#else
        std::fprintf(g_log, "Runtime Vulkan Debug Toggle: DISABLED\n");
#endif
        std::fflush(g_log);
    }
}

void PrintBuildInfoOnce(){ std::call_once(g_once, [](){ InitBuildInfoLogging(); _print(); }); }
void PrintBuildInfoAlways(){ InitBuildInfoLogging(); _print(); }

} // namespace voxelvk
