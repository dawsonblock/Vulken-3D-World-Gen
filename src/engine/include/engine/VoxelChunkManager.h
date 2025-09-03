#pragma once
#include <memory>
#include "engine_export.h"
#include "ChunkCoord.h"
#include "VoxelTypes.h"
#include "Result.h"
#include "Storage/IStorageBackend.h"
#include "Cache/IChunkCache.h"

namespace engine {

class ENGINE_API VoxelChunkManager {
public:
    VoxelChunkManager(StorageBackendPtr storage, ChunkCachePtr cache)
        : storage_(std::move(storage)), cache_(std::move(cache)) {}

    VoxelChunkPtr getOrLoad(const ChunkCoord& c) {
        if (auto in = cache_->get(c)) return in;
        if (!storage_) return nullptr;
        auto res = storage_->loadChunk(c);
        if (!res.is_ok()) return nullptr;
        auto ptr = std::make_shared<VoxelChunk>(std::move(res.value));
        cache_->put(c, ptr);
        return ptr;
    }

    Result<void> save(const VoxelChunk& chunk) {
        if (storage_) return storage_->saveChunk(chunk);
        return Result<void>::Err("No storage backend");
    }

    void put(const VoxelChunkPtr& chunk) {
        if (!chunk) return;
        cache_->put(chunk->coord, chunk);
    }

    void evict(const ChunkCoord& c) { cache_->erase(c); }
    void flushCache() { cache_->clear(); }

private:
    StorageBackendPtr storage_;
    ChunkCachePtr cache_;
};

} // namespace engine
