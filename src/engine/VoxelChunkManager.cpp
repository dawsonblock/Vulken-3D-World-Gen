#include "engine/VoxelChunkManager.h"
#include <mutex>
#include <optional>

namespace vulken {

VoxelChunkManager::VoxelChunkManager(core::EngineConfig cfg,
                                     std::shared_ptr<storage::IStorageBackend> storage,
                                     std::shared_ptr<meshing::IMeshGenerator> mesher)
: cfg_(std::move(cfg))
, storage_(std::move(storage))
, mesher_(std::move(mesher))
, cache_(cfg_.maxResidentChunks) {}

void VoxelChunkManager::setCacheSize(size_t chunks) {
    std::scoped_lock lk(mtx_);
    cache_.setCapacity(chunks);
}

std::optional<core::VoxelChunk> VoxelChunkManager::loadLocked_(const core::ChunkCoord& coord) {
    core::VoxelChunk chunk;
    if (cache_.get(coord, chunk)) return chunk;
    auto loaded = storage_->loadChunk(coord, cfg_.chunkSizeX, cfg_.chunkSizeY, cfg_.chunkSizeZ);
    if (!loaded) return std::nullopt;
    cache_.put(coord, *loaded);
    return loaded;
}

std::optional<core::VoxelChunk> VoxelChunkManager::getChunk(const core::ChunkCoord& coord) {
    std::scoped_lock lk(mtx_);
    return loadLocked_(coord);
}

bool VoxelChunkManager::saveChunk(const core::VoxelChunk& chunk) {
    std::scoped_lock lk(mtx_);
    cache_.put(chunk.coord, chunk);
    return storage_->saveChunk(chunk);
}

std::optional<core::Mesh> VoxelChunkManager::getChunkMesh(const core::ChunkCoord& coord) {
    std::scoped_lock lk(mtx_);
    auto chunk = loadLocked_(coord);
    if (!chunk) return std::nullopt;
    return mesher_->generate(*chunk);
}

void VoxelChunkManager::evict(const core::ChunkCoord& coord) {
    std::scoped_lock lk(mtx_);
    cache_.erase(coord);
}

} // namespace vulken
