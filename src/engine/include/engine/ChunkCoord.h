#pragma once
#include <cstddef>
#include <functional>

namespace engine {

struct ChunkCoord {
    int x{0}, y{0}, z{0};

    bool operator==(const ChunkCoord& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& c) const noexcept {
        // Simple 3D hash combine
        std::size_t h1 = std::hash<int>{}(c.x);
        std::size_t h2 = std::hash<int>{}(c.y);
        std::size_t h3 = std::hash<int>{}(c.z);
        std::size_t seed = h1;
        seed ^= h2 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= h3 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }
};

} // namespace engine
