#include <tracy/TracyVulkan.hpp>

// ...existing code...

void CommandBuffer::beginRenderPass(VkRenderPass renderPass, VkFramebuffer framebuffer) {
    ZoneScopedN("BeginRenderPass");
    TracyVkZone(g_tracyVk.ctx, handle, "Begin Render Pass", true);
    // ...existing code...
}

void CommandBuffer::endRenderPass() {
    ZoneScopedN("EndRenderPass");
    // ...existing code...
    TracyVkCollect(g_tracyVk.ctx, handle);
}
