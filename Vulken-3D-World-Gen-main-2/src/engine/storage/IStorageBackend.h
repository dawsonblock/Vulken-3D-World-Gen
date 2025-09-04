#pragma once
#include <optional>
#include <string>
#include "engine/core/VoxelTypes.h"
#include "engine/core/EngineConfig.h"

namespace vulken::storage {

class IStorageBackend {
public:
    virtual ~IStorageBackend() = default;

    // Load a voxel chunk based on coordinate and size from persistent storage.
    virtual std::optional<core::VoxelChunk> loadChunk(const core::ChunkCoord& coord,
                                                      uint32_t sx, uint32_t sy, uint32_t sz) = 0;

    // Save a voxel chunk (editor/export).
    virtual bool saveChunk(const core::VoxelChunk& chunk) = 0;

    // Optional: index queries, biome/material catalogs, metadata.
    // virtual ... query(...)=0;
};

} // namespace vulken::storage
