#include "engine/storage/FileStorageBackend.h"
#include <fstream>

namespace vulken::storage {

FileStorageBackend::FileStorageBackend(const core::EngineConfig& cfg) : cfg_(cfg) {}

std::filesystem::path FileStorageBackend::chunkPath(const core::ChunkCoord& c) const {
    auto root = std::filesystem::path(cfg_.voxelDataDir);
    // Example layout: assets/voxels/x_y_z.mvox
    return root / (std::to_string(c.x) + "_" + std::to_string(c.y) + "_" + std::to_string(c.z) + ".mvox");
}

std::optional<core::VoxelChunk> FileStorageBackend::loadChunk(const core::ChunkCoord& coord,
                                                              uint32_t sx, uint32_t sy, uint32_t sz) {
    core::VoxelChunk chunk;
    chunk.coord = coord;
    chunk.sizeX = sx; chunk.sizeY = sy; chunk.sizeZ = sz;
    chunk.voxels.resize(static_cast<size_t>(sx) * sy * sz);

    auto p = chunkPath(coord);
    if (!std::filesystem::exists(p)) {
        // Placeholder: generate empty chunk
        return chunk;
    }
    if (!readMVOX(p, chunk)) {
        return std::nullopt;
    }
    return chunk;
}

bool FileStorageBackend::saveChunk(const core::VoxelChunk& chunk) {
    auto p = chunkPath(chunk.coord);
    std::filesystem::create_directories(p.parent_path());
    return writeMVOX(p, chunk);
}

bool FileStorageBackend::readMVOX(const std::filesystem::path& p, core::VoxelChunk& out) const {
    // Placeholder simple raw dump: [u32 sx,sy,sz][for N: float density, u16 material]
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    uint32_t sx=0, sy=0, sz=0;
    f.read(reinterpret_cast<char*>(&sx), sizeof(uint32_t));
    f.read(reinterpret_cast<char*>(&sy), sizeof(uint32_t));
    f.read(reinterpret_cast<char*>(&sz), sizeof(uint32_t));
    if (!f || sx!=out.sizeX || sy!=out.sizeY || sz!=out.sizeZ) return false;

    for (size_t i=0; i<out.voxels.size(); ++i) {
        f.read(reinterpret_cast<char*>(&out.voxels[i].density), sizeof(float));
        f.read(reinterpret_cast<char*>(&out.voxels[i].material), sizeof(uint16_t));
        if (!f) return false;
    }
    return true;
}

bool FileStorageBackend::writeMVOX(const std::filesystem::path& p, const core::VoxelChunk& in) const {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(&in.sizeX), sizeof(uint32_t));
    f.write(reinterpret_cast<const char*>(&in.sizeY), sizeof(uint32_t));
    f.write(reinterpret_cast<const char*>(&in.sizeZ), sizeof(uint32_t));
    for (const auto& v : in.voxels) {
        f.write(reinterpret_cast<const char*>(&v.density), sizeof(float));
        f.write(reinterpret_cast<const char*>(&v.material), sizeof(uint16_t));
        if (!f) return false;
    }
    return true;
}

} // namespace vulken::storage
