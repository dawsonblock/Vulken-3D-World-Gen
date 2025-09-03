#pragma once
#include <memory>
#include <string>
#include "engine_export.h"
#include "../ChunkCoord.h"
#include "../VoxelTypes.h"
#include "../Result.h"

namespace engine {

class ENGINE_API IStorageBackend {
public:
    virtual ~IStorageBackend() = default;

    virtual bool hasChunk(const ChunkCoord& coord) const = 0;
    virtual Result<VoxelChunk> loadChunk(const ChunkCoord& coord) = 0;
    virtual Result<void> saveChunk(const VoxelChunk& chunk) = 0;

    // Optional: bulk ops, indexing, etc. (Phase 2/4)
};

using StorageBackendPtr = std::shared_ptr<IStorageBackend>;

} // namespace engine
