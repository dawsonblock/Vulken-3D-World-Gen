#pragma once
#include <memory>
#include "../ChunkCoord.h"
#include "../VoxelTypes.h"
#include "../engine_export.h"

namespace engine {

using VoxelChunkPtr = std::shared_ptr<VoxelChunk>;

class ENGINE_API IChunkCache {
public:
    virtual ~IChunkCache() = default;
    virtual VoxelChunkPtr get(const ChunkCoord& c) = 0;
    virtual void put(const ChunkCoord& c, VoxelChunkPtr chunk) = 0;
    virtual void erase(const ChunkCoord& c) = 0;
    virtual void clear() = 0;
    virtual size_t size() const = 0;
    virtual size_t capacity() const = 0;
};

using ChunkCachePtr = std::shared_ptr<IChunkCache>;

} // namespace engine
