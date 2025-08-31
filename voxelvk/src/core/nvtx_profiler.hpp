#pragma once
#include <cstdint>
#include <string>

// NVTX profiling support for NVIDIA Nsight
namespace voxelvk {

#ifdef VXL_ENABLE_NVTX
#include <nvtx3/nvToolsExt.h>
#define VXL_HAS_NVTX 1
#else
#define VXL_HAS_NVTX 0
#endif

class NVTXProfiler {
public:
    // Color constants for NVTX markers
    enum class Color : uint32_t {
        Green       = 0xFF00FF00,
        Blue        = 0xFF0000FF,
        Yellow      = 0xFFFFFF00,
        Magenta     = 0xFFFF00FF,
        Cyan        = 0xFF00FFFF,
        Red         = 0xFFFF0000,
        Orange      = 0xFFFF8000,
        Purple      = 0xFF8000FF,
        Pink        = 0xFFFF0080,
        LightBlue   = 0xFF8080FF,
        LightGreen  = 0xFF80FF80,
        White       = 0xFFFFFFFF,
        Gray        = 0xFF808080,
        DarkGray    = 0xFF404040,
        Black       = 0xFF000000
    };
    
    // Initialize NVTX (call once at startup)
    static void Initialize();
    
    // Shutdown NVTX (call once at shutdown)
    static void Shutdown();
    
    // Mark a point in time with a message
    static void Mark(const char* message, Color color = Color::White);
    static void Mark(const std::string& message, Color color = Color::White);
    
    // Start a range (returns range ID for ending)
    static uint64_t RangeStart(const char* message, Color color = Color::Green);
    static uint64_t RangeStart(const std::string& message, Color color = Color::Green);
    
    // End a range
    static void RangeEnd(uint64_t range_id);
    
    // Push/pop ranges (LIFO stack)
    static void RangePush(const char* message, Color color = Color::Blue);
    static void RangePush(const std::string& message, Color color = Color::Blue);
    static void RangePop();
    
    // Domain support for grouping
    static void* CreateDomain(const char* name);
    static void DestroyDomain(void* domain);
    
    static void DomainMark(void* domain, const char* message, Color color = Color::White);
    static uint64_t DomainRangeStart(void* domain, const char* message, Color color = Color::Green);
    static void DomainRangeEnd(void* domain, uint64_t range_id);
    static void DomainRangePush(void* domain, const char* message, Color color = Color::Blue);
    static void DomainRangePop(void* domain);
    
    // Check if NVTX is available
    static bool IsAvailable() { return VXL_HAS_NVTX; }
    
private:
    static bool s_initialized;
    static void* s_default_domain;
};

// RAII wrapper for NVTX ranges
class NVTXScope {
public:
    explicit NVTXScope(const char* name, NVTXProfiler::Color color = NVTXProfiler::Color::Green)
        : m_domain(nullptr) {
        if (NVTXProfiler::IsAvailable()) {
            NVTXProfiler::RangePush(name, color);
        }
    }
    
    explicit NVTXScope(const std::string& name, NVTXProfiler::Color color = NVTXProfiler::Color::Green)
        : m_domain(nullptr) {
        if (NVTXProfiler::IsAvailable()) {
            NVTXProfiler::RangePush(name, color);
        }
    }
    
    NVTXScope(void* domain, const char* name, NVTXProfiler::Color color = NVTXProfiler::Color::Green)
        : m_domain(domain) {
        if (NVTXProfiler::IsAvailable()) {
            NVTXProfiler::DomainRangePush(domain, name, color);
        }
    }
    
    ~NVTXScope() {
        if (NVTXProfiler::IsAvailable()) {
            if (m_domain) {
                NVTXProfiler::DomainRangePop(m_domain);
            } else {
                NVTXProfiler::RangePop();
            }
        }
    }
    
    // Non-copyable, non-movable
    NVTXScope(const NVTXScope&) = delete;
    NVTXScope& operator=(const NVTXScope&) = delete;
    NVTXScope(NVTXScope&&) = delete;
    NVTXScope& operator=(NVTXScope&&) = delete;
    
private:
    void* m_domain;
};

// Convenience macros
#if VXL_HAS_NVTX
    #define VXL_NVTX_MARK(msg) ::voxelvk::NVTXProfiler::Mark(msg)
    #define VXL_NVTX_MARK_COLOR(msg, color) ::voxelvk::NVTXProfiler::Mark(msg, color)
    #define VXL_NVTX_RANGE(name) ::voxelvk::NVTXScope nvtx_scope_##__LINE__(name)
    #define VXL_NVTX_RANGE_COLOR(name, color) ::voxelvk::NVTXScope nvtx_scope_##__LINE__(name, color)
    #define VXL_NVTX_FUNCTION() VXL_NVTX_RANGE(__FUNCTION__)
    #define VXL_NVTX_FUNCTION_COLOR(color) VXL_NVTX_RANGE_COLOR(__FUNCTION__, color)
#else
    #define VXL_NVTX_MARK(msg) ((void)0)
    #define VXL_NVTX_MARK_COLOR(msg, color) ((void)0)
    #define VXL_NVTX_RANGE(name) ((void)0)
    #define VXL_NVTX_RANGE_COLOR(name, color) ((void)0)
    #define VXL_NVTX_FUNCTION() ((void)0)
    #define VXL_NVTX_FUNCTION_COLOR(color) ((void)0)
#endif

// Common NVTX domains for VoxelRL
namespace NVTXDomains {
    extern void* Rendering;
    extern void* Physics;
    extern void* AI;
    extern void* WorldGen;
    extern void* Training;
    extern void* IO;
    
    void InitializeDomains();
    void ShutdownDomains();
}

} // namespace voxelvk