#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <optional>

namespace vulken::core {

struct ChunkCoord {
    int32_t x{};
    int32_t y{};
    int32_t z{};
    auto operator<=>(const ChunkCoord&) const = default;
};

struct Voxel {
    // Density or SDF value; >0 = solid in typical MC flows
    float density = 0.0f;
    // Material index (matches materials.json)
    uint16_t material = 0;
};

struct VoxelChunk {
    ChunkCoord coord;
    uint32_t sizeX{32}, sizeY{32}, sizeZ{32};
    // Interleaved or SoA; keep simple for now
    std::vector<Voxel> voxels; // size = sizeX*sizeY*sizeZ
};

struct Vertex {
    std::array<float,3> pos{};
    std::array<float,3> normal{};
    std::array<float,2> uv{};
    uint16_t material = 0;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    // Future: LODs, adjacency, meshlets, material buckets
};

inline size_t linearIndex(uint32_t x, uint32_t y, uint32_t z,
                          uint32_t sx, uint32_t sy, uint32_t sz) {
    return static_cast<size_t>(z)*sx*sy + static_cast<size_t>(y)*sx + x;
}

} // namespace vulken::core
