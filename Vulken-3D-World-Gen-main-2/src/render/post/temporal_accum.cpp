#include "temporal_accum.hpp"

namespace voxelvk {

void dispatch_temporal_accum(VkCommandBuffer cmd, const TRPInputs& in) {
    // Pseudo-implementation: bind pipeline, descriptors, push constants, dispatch (w+7)/8,(h+7)/8,1
    // This would need proper Vulkan descriptor/pipeline binding
    // For now, just a stub that demonstrates the interface
    
    // TODO: Implement actual Vulkan dispatch:
    // 1. Bind compute pipeline for temporal_accum.comp
    // 2. Bind descriptor sets with curr/history/out images
    // 3. Push constants with invSize, dt, alpha
    // 4. vkCmdDispatch(cmd, (in.w+7)/8, (in.h+7)/8, 1)
}

} // namespace voxelvk