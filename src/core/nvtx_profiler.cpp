#include "nvtx_profiler.hpp"
#include "logger.hpp"

namespace voxelvk {

bool NVTXProfiler::s_initialized = false;
void* NVTXProfiler::s_default_domain = nullptr;

void NVTXProfiler::Initialize() {
    if (s_initialized) {
        return;
    }
    
#if VXL_HAS_NVTX
    // Create default domain
    s_default_domain = CreateDomain("VoxelRL");
    s_initialized = true;
    
    // Initialize common domains
    NVTXDomains::InitializeDomains();
    
    VXL_INFO("NVTX profiling initialized");
#else
    VXL_DEBUG("NVTX profiling not available (compiled without NVTX support)");
#endif
}

void NVTXProfiler::Shutdown() {
    if (!s_initialized) {
        return;
    }
    
#if VXL_HAS_NVTX
    // Shutdown common domains
    NVTXDomains::ShutdownDomains();
    
    // Destroy default domain
    if (s_default_domain) {
        DestroyDomain(s_default_domain);
        s_default_domain = nullptr;
    }
    
    s_initialized = false;
    VXL_INFO("NVTX profiling shutdown");
#endif
}

void NVTXProfiler::Mark(const char* message, Color color) {
#if VXL_HAS_NVTX
    nvtxEventAttributes_t eventAttrib = {0};
    eventAttrib.version = NVTX_VERSION;
    eventAttrib.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
    eventAttrib.colorType = NVTX_COLOR_ARGB;
    eventAttrib.color = static_cast<uint32_t>(color);
    eventAttrib.messageType = NVTX_MESSAGE_TYPE_ASCII;
    eventAttrib.message.ascii = message;
    nvtxMarkEx(&eventAttrib);
#endif
}

void NVTXProfiler::Mark(const std::string& message, Color color) {
    Mark(message.c_str(), color);
}

uint64_t NVTXProfiler::RangeStart(const char* message, Color color) {
#if VXL_HAS_NVTX
    nvtxEventAttributes_t eventAttrib = {0};
    eventAttrib.version = NVTX_VERSION;
    eventAttrib.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
    eventAttrib.colorType = NVTX_COLOR_ARGB;
    eventAttrib.color = static_cast<uint32_t>(color);
    eventAttrib.messageType = NVTX_MESSAGE_TYPE_ASCII;
    eventAttrib.message.ascii = message;
    return nvtxRangeStartEx(&eventAttrib);
#else
    return 0;
#endif
}

uint64_t NVTXProfiler::RangeStart(const std::string& message, Color color) {
    return RangeStart(message.c_str(), color);
}

void NVTXProfiler::RangeEnd(uint64_t range_id) {
#if VXL_HAS_NVTX
    nvtxRangeEnd(range_id);
#endif
}

void NVTXProfiler::RangePush(const char* message, Color color) {
#if VXL_HAS_NVTX
    nvtxEventAttributes_t eventAttrib = {0};
    eventAttrib.version = NVTX_VERSION;
    eventAttrib.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
    eventAttrib.colorType = NVTX_COLOR_ARGB;
    eventAttrib.color = static_cast<uint32_t>(color);
    eventAttrib.messageType = NVTX_MESSAGE_TYPE_ASCII;
    eventAttrib.message.ascii = message;
    nvtxRangePushEx(&eventAttrib);
#endif
}

void NVTXProfiler::RangePush(const std::string& message, Color color) {
    RangePush(message.c_str(), color);
}

void NVTXProfiler::RangePop() {
#if VXL_HAS_NVTX
    nvtxRangePop();
#endif
}

void* NVTXProfiler::CreateDomain(const char* name) {
#if VXL_HAS_NVTX
    return nvtxDomainCreateA(name);
#else
    return nullptr;
#endif
}

void NVTXProfiler::DestroyDomain(void* domain) {
#if VXL_HAS_NVTX
    if (domain) {
        nvtxDomainDestroy(static_cast<nvtxDomainHandle_t>(domain));
    }
#endif
}

void NVTXProfiler::DomainMark(void* domain, const char* message, Color color) {
#if VXL_HAS_NVTX
    nvtxEventAttributes_t eventAttrib = {0};
    eventAttrib.version = NVTX_VERSION;
    eventAttrib.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
    eventAttrib.colorType = NVTX_COLOR_ARGB;
    eventAttrib.color = static_cast<uint32_t>(color);
    eventAttrib.messageType = NVTX_MESSAGE_TYPE_ASCII;
    eventAttrib.message.ascii = message;
    nvtxDomainMarkEx(static_cast<nvtxDomainHandle_t>(domain), &eventAttrib);
#endif
}

uint64_t NVTXProfiler::DomainRangeStart(void* domain, const char* message, Color color) {
#if VXL_HAS_NVTX
    nvtxEventAttributes_t eventAttrib = {0};
    eventAttrib.version = NVTX_VERSION;
    eventAttrib.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
    eventAttrib.colorType = NVTX_COLOR_ARGB;
    eventAttrib.color = static_cast<uint32_t>(color);
    eventAttrib.messageType = NVTX_MESSAGE_TYPE_ASCII;
    eventAttrib.message.ascii = message;
    return nvtxDomainRangeStartEx(static_cast<nvtxDomainHandle_t>(domain), &eventAttrib);
#else
    return 0;
#endif
}

void NVTXProfiler::DomainRangeEnd(void* domain, uint64_t range_id) {
#if VXL_HAS_NVTX
    nvtxDomainRangeEnd(static_cast<nvtxDomainHandle_t>(domain), range_id);
#endif
}

void NVTXProfiler::DomainRangePush(void* domain, const char* message, Color color) {
#if VXL_HAS_NVTX
    nvtxEventAttributes_t eventAttrib = {0};
    eventAttrib.version = NVTX_VERSION;
    eventAttrib.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
    eventAttrib.colorType = NVTX_COLOR_ARGB;
    eventAttrib.color = static_cast<uint32_t>(color);
    eventAttrib.messageType = NVTX_MESSAGE_TYPE_ASCII;
    eventAttrib.message.ascii = message;
    nvtxDomainRangePushEx(static_cast<nvtxDomainHandle_t>(domain), &eventAttrib);
#endif
}

void NVTXProfiler::DomainRangePop(void* domain) {
#if VXL_HAS_NVTX
    nvtxDomainRangePop(static_cast<nvtxDomainHandle_t>(domain));
#endif
}

// NVTX Domains implementation
namespace NVTXDomains {
    void* Rendering = nullptr;
    void* Physics = nullptr;
    void* AI = nullptr;
    void* WorldGen = nullptr;
    void* Training = nullptr;
    void* IO = nullptr;
    
    void InitializeDomains() {
#if VXL_HAS_NVTX
        Rendering = NVTXProfiler::CreateDomain("Rendering");
        Physics = NVTXProfiler::CreateDomain("Physics");
        AI = NVTXProfiler::CreateDomain("AI");
        WorldGen = NVTXProfiler::CreateDomain("WorldGen");
        Training = NVTXProfiler::CreateDomain("Training");
        IO = NVTXProfiler::CreateDomain("IO");
#endif
    }
    
    void ShutdownDomains() {
#if VXL_HAS_NVTX
        NVTXProfiler::DestroyDomain(Rendering);
        NVTXProfiler::DestroyDomain(Physics);
        NVTXProfiler::DestroyDomain(AI);
        NVTXProfiler::DestroyDomain(WorldGen);
        NVTXProfiler::DestroyDomain(Training);
        NVTXProfiler::DestroyDomain(IO);
        
        Rendering = nullptr;
        Physics = nullptr;
        AI = nullptr;
        WorldGen = nullptr;
        Training = nullptr;
        IO = nullptr;
#endif
    }
}

} // namespace voxelvk