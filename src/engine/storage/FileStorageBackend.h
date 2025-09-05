#pragma once
#include <filesystem>
#include "IStorageBackend.h"

namespace vulken::storage {

class FileStorageBackend final : public IStorageBackend {
public:
    explicit FileStorageBackend(const core::EngineConfig& cfg);

    std::optional<core::VoxelChunk> loadChunk(const core::ChunkCoord& coord,
                                              uint32_t sx, uint32_t sy, uint32_t sz) override;

    bool saveChunk(const core::VoxelChunk& chunk) override;

private:
    core::EngineConfig cfg_;
    std::filesystem::path chunkPath(const core::ChunkCoord& coord) const;
    bool readMVOX(const std::filesystem::path& p,
                  core::VoxelChunk& out) const; // placeholder
    bool writeMVOX(const std::filesystem::path& p,
                   const core::VoxelChunk& in) const; // placeholder
};

} // namespace vulken::storage
