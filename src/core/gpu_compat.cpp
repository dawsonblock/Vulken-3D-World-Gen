
#include "gpu_compat.hpp"
#include "build_info_print.hpp"
#include <cstdio>

namespace voxelvk {

CompatOptions ComputeCompatFor(const VkPhysicalDeviceProperties& props){
    CompatOptions o{};
    const uint32_t NVIDIA = 0x10DE;
    const uint32_t AMD    = 0x1002;
    const uint32_t INTEL  = 0x8086;

    char line[512];  // Increased buffer size to accommodate long device names
    std::snprintf(line, sizeof(line), "[GPU] %s (vendor=0x%04X, device=0x%04X, driver=0x%08X)",
                  props.deviceName, props.vendorID, props.deviceID, props.driverVersion);
    LogLine(line);

    if (props.vendorID == NVIDIA){
        // NVIDIA: layout transitions strict; prefer dedicated alloc for big images
        o.strictLayoutTransitions = true;
        o.avoidZeroSizedWrites = true;
        o.conservativeDriverSync = false; // NVIDIA generally fine
        LogLine("[GPU] NVIDIA profile: strict layout transitions, dedicated alloc for big images.");
    } else if (props.vendorID == AMD){
        // AMD: be careful with zero-sized transfers; robust pipeline creation
        o.avoidZeroSizedWrites = true;
        o.robustPipelineCreation = true;
        o.conservativeDriverSync = true;
        LogLine("[GPU] AMD profile: avoid zero-sized writes, robust pipeline creation, conservative sync.");
    } else if (props.vendorID == INTEL){
        // INTEL: conservative sync, prefer smaller batches
        o.conservativeDriverSync = true;
        o.robustPipelineCreation = true;
        LogLine("[GPU] Intel profile: conservative sync and robust pipeline creation.");
    } else {
        LogLine("[GPU] Generic profile applied.");
    }
    return o;
}

} // namespace voxelvk
