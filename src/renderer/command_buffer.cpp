#include <tracy/Tracy.hpp>

// ...existing code...

void CommandBuffer::beginRenderPass(VkRenderPass renderPass, VkFramebuffer framebuffer) {
    ZoneScopedN("BeginRenderPass");
    // ...existing code...
}

void CommandBuffer::endRenderPass() {
    ZoneScopedN("EndRenderPass");
    // ...existing code...
}
