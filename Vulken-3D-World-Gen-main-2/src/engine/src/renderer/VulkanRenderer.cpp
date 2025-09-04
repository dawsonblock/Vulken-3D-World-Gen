#include "engine/Renderer/VulkanRenderer.h"

namespace engine {

bool VulkanRenderer::init(const RendererInit& /*init*/) {
    initialized_ = true;
    return true;
}

void VulkanRenderer::shutdown() {
    // TODO: release resources
    initialized_ = false;
}

uint64_t VulkanRenderer::uploadMesh(const Mesh& /*mesh*/) {
    // TODO: upload to GPU
    static uint64_t nextHandle = 1;
    return nextHandle++;
}

void VulkanRenderer::destroyMesh(uint64_t /*handle*/) {
    // TODO: free GPU resources
}

void VulkanRenderer::renderFrame() {
    if (!initialized_) return;
    // TODO: draw calls, frame management
}

} // namespace engine
