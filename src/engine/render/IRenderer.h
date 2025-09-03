#pragma once
#include <memory>
#include "engine/core/VoxelTypes.h"

namespace vulken::render {

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    // Upload mesh and get a handle/id for later drawing
    virtual uint64_t uploadMesh(const core::Mesh& mesh) = 0;
    virtual void destroyMesh(uint64_t meshHandle) = 0;

    // Draw uploaded meshes (ImGui HUD integration happens outside)
    virtual void drawMesh(uint64_t meshHandle) = 0;
};

} // namespace vulken::render
