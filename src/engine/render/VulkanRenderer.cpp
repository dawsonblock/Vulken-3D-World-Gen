#include "engine/render/VulkanRenderer.h"

namespace vulken::render {

bool VulkanRenderer::initialize() {
    // Create instance/device/swapchain/pipelines later
    return true;
}

void VulkanRenderer::shutdown() {
    // Destroy pipelines, buffers, device, instance
    meshes_.clear();
}

uint64_t VulkanRenderer::uploadMesh(const core::Mesh& /*mesh*/) {
    // Create GPU buffers, upload via staging, record handle
    auto h = nextHandle_++;
    meshes_[h] = MeshGpu{};
    return h;
}

void VulkanRenderer::destroyMesh(uint64_t meshHandle) {
    meshes_.erase(meshHandle);
}

void VulkanRenderer::drawMesh(uint64_t /*meshHandle*/) {
    // Record draw calls into command buffers
}

} // namespace vulken::render
