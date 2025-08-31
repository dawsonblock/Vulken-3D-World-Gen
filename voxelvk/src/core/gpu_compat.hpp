
#pragma once
#include <vulkan/vulkan.h>

namespace voxelvk {

struct CompatOptions {
    // toggles your engine can read; initialize with conservative defaults
    bool robustPipelineCreation = true;
    bool strictLayoutTransitions = true;
    bool conservativeDriverSync = true;
    bool avoidZeroSizedWrites = true;
    bool preferDedicatedAllocForLarge = true;
    // expose as needed
};

// Detects vendor and logs recommended knobs; returns options to apply in your renderer/device setup.
CompatOptions ComputeCompatFor(const VkPhysicalDeviceProperties& props);

} // namespace voxelvk
