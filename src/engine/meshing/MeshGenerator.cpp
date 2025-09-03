#include "engine/meshing/MeshGenerator.h"

namespace vulken::meshing {

std::optional<core::Mesh> MeshGenerator::generate(const core::VoxelChunk& chunk) {
    core::Mesh mesh;
    // Placeholder: if center voxel is solid, emit a tiny cube for visualization
    const uint32_t cx = chunk.sizeX / 2;
    const uint32_t cy = chunk.sizeY / 2;
    const uint32_t cz = chunk.sizeZ / 2;
    const auto idx = vulken::core::linearIndex(cx, cy, cz, chunk.sizeX, chunk.sizeY, chunk.sizeZ);
    if (idx < chunk.voxels.size() && chunk.voxels[idx].density > isoValue_) {
        // Very small cube at origin
        mesh.vertices.resize(8);
        // ...fill positions/normals/uvs minimally...
        // ...indices for 12 triangles...
        // Keep empty to avoid bloat; actual MC/DC will populate mesh here.
    }
    return mesh;
}

} // namespace vulken::meshing
