#pragma once
#include <unordered_map>
#include "engine/render/IRenderer.h"

namespace vulken::render {

class VulkanRenderer final : public IRenderer {
public:
    bool initialize() override;
    void shutdown() override;

    uint64_t uploadMesh(const core::Mesh& mesh) override;
    void destroyMesh(uint64_t meshHandle) override;
    void drawMesh(uint64_t meshHandle) override;

private:
    struct MeshGpu {
        // VkBuffer vertexBuffer;
        // VkBuffer indexBuffer;
        // size_t indexCount;
    };
    uint64_t nextHandle_ = 1;
    std::unordered_map<uint64_t, MeshGpu> meshes_;
    // VkDevice, VkInstance, VMA allocator, pipelines, etc.
};

} // namespace vulken::render
