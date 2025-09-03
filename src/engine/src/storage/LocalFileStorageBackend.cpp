#include <fstream>
#include <filesystem>
#include "engine/Storage/LocalFileStorageBackend.h"

namespace engine {

LocalFileStorageBackend::LocalFileStorageBackend(std::filesystem::path rootDir)
    : root_(std::move(rootDir)) {
    std::filesystem::create_directories(root_);
}

std::filesystem::path LocalFileStorageBackend::chunkPath(const ChunkCoord& coord) const {
    // Simple path scheme: root/x_y_z.mvox (placeholder format)
    return root_ / (std::to_string(coord.x) + "_" +
                    std::to_string(coord.y) + "_" +
                    std::to_string(coord.z) + ".mvox");
}

bool LocalFileStorageBackend::hasChunk(const ChunkCoord& coord) const {
    return std::filesystem::exists(chunkPath(coord));
}

Result<VoxelChunk> LocalFileStorageBackend::loadChunk(const ChunkCoord& coord) {
    auto p = chunkPath(coord);
    if (!std::filesystem::exists(p)) {
        return Result<VoxelChunk>::Err("Chunk file not found: " + p.string());
    }
    // Placeholder binary format: [magic:4][dim:4][voxels...]
    std::ifstream in(p, std::ios::binary);
    if (!in) return Result<VoxelChunk>::Err("Failed to open: " + p.string());

    uint32_t magic = 0, dim = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    in.read(reinterpret_cast<char*>(&dim), sizeof(dim));
    if (!in || magic != 0x78564F58 /* 'xVOX' */ || dim != kChunkDim) {
        return Result<VoxelChunk>::Err("Invalid chunk header: " + p.string());
    }

    VoxelChunk chunk{coord};
    in.read(reinterpret_cast<char*>(chunk.voxels.data()),
            chunk.voxels.size() * sizeof(Voxel));
    if (!in) return Result<VoxelChunk>::Err("Corrupt chunk data: " + p.string());
    return Result<VoxelChunk>::Ok(std::move(chunk));
}

Result<void> LocalFileStorageBackend::saveChunk(const VoxelChunk& chunk) {
    auto p = chunkPath(chunk.coord);
    std::filesystem::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) return Result<void>::Err("Failed to open for write: " + p.string());

    uint32_t magic = 0x78564F58; // 'xVOX'
    uint32_t dim = kChunkDim;
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&dim), sizeof(dim));
    out.write(reinterpret_cast<const char*>(chunk.voxels.data()),
              chunk.voxels.size() * sizeof(Voxel));
    if (!out) return Result<void>::Err("Failed to write: " + p.string());
    return Result<void>::Ok();
}

} // namespace engine
