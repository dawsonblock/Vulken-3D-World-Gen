#pragma once
#include <cstdint>
#include <vector>
#include "ChunkCoord.h"

namespace engine {

constexpr int kChunkDim = 32; // 32x32x32
constexpr int kVoxelsPerChunk = kChunkDim * kChunkDim * kChunkDim;

struct Voxel {
    uint16_t material{0};
    uint8_t density{0}; // 0..255, threshold ~128
};

struct VoxelChunk {
    ChunkCoord coord{};
    std::vector<Voxel> voxels; // size == kVoxelsPerChunk

    VoxelChunk() : voxels(kVoxelsPerChunk) {}
    explicit VoxelChunk(ChunkCoord c) : coord(c), voxels(kVoxelsPerChunk) {}

    static inline int index(int x, int y, int z) {
        return (z * kChunkDim + y) * kChunkDim + x;
    }
};

struct Mesh {
    std::vector<float> positions; // xyz triplets
    std::vector<float> normals;   // xyz triplets
    std::vector<float> uvs;       // uv pairs (optional)
    std::vector<uint32_t> indices;
};

} // namespace engine
