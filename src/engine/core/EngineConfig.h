#pragma once
#include <string>
#include <cstdint>

namespace vulken::core {

struct EngineConfig {
    // Chunk dimensions (in voxels)
    uint32_t chunkSizeX = 32;
    uint32_t chunkSizeY = 32;
    uint32_t chunkSizeZ = 32;

    // Cache sizes
    size_t maxResidentChunks = 256; // LRU limit
    size_t ioThreadCount = 2;

    // Storage locations
    std::string assetRoot = "assets";
    std::string voxelDataDir = "assets/voxels";
    std::string cacheDir = ".cache/vulken";

    // Future: sqlite index path, redis settings, etc.
    // std::string sqliteIndexPath;
    // std::string redisUri;
};

} // namespace vulken::core
