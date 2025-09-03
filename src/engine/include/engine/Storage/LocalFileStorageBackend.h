#pragma once
#include <filesystem>
#include "IStorageBackend.h"

namespace engine {

class ENGINE_API LocalFileStorageBackend : public IStorageBackend {
public:
    explicit LocalFileStorageBackend(std::filesystem::path rootDir);

    bool hasChunk(const ChunkCoord& coord) const override;
    Result<VoxelChunk> loadChunk(const ChunkCoord& coord) override;
    Result<void> saveChunk(const VoxelChunk& chunk) override;

private:
    std::filesystem::path root_;
    std::filesystem::path chunkPath(const ChunkCoord& coord) const;
};

} // namespace engine
