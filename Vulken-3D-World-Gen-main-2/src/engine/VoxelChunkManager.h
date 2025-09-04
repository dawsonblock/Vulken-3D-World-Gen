#pragma once
#include <memory>
#include <mutex>
#include <unordered_map>
#include <optional>
#include "engine/core/EngineConfig.h"
#include "engine/core/VoxelTypes.h"
#include "engine/storage/IStorageBackend.h"
#include "engine/meshing/IMeshGenerator.h"
#include "engine/utils/LRUCache.h"

namespace vulken {

class VoxelChunkManager {
public:
    VoxelChunkManager(core::EngineConfig cfg,
                      std::shared_ptr<storage::IStorageBackend> storage,
                      std::shared_ptr<meshing::IMeshGenerator> mesher);

    void setCacheSize(size_t chunks);
    std::optional<core::VoxelChunk> getChunk(const core::ChunkCoord& coord);
    bool saveChunk(const core::VoxelChunk& chunk);

    // On-demand meshing for a chunk
    std::optional<core::Mesh> getChunkMesh(const core::ChunkCoord& coord);

    // Eviction control
    void evict(const core::ChunkCoord& coord);

private:
    core::EngineConfig cfg_;
    std::shared_ptr<storage::IStorageBackend> storage_;
    std::shared_ptr<meshing::IMeshGenerator> mesher_;

    using CacheT = utils::LRUCache<core::ChunkCoord, core::VoxelChunk>;
    CacheT cache_;
    std::mutex mtx_;

    std::optional<core::VoxelChunk> loadLocked_(const core::ChunkCoord& coord);
};

} // namespace vulken
