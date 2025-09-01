#pragma once
#include "blocks.hpp"
#include "../core/logger.hpp"
#include "../core/timer.hpp"
#include "../core/thread_pool.hpp"
#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <atomic>
#include <queue>

namespace voxelvk {

// Chunk coordinates
struct ChunkCoord {
    int32_t x, y, z;
    
    ChunkCoord() : x(0), y(0), z(0) {}
    ChunkCoord(int32_t x_, int32_t y_, int32_t z_) : x(x_), y(y_), z(z_) {}
    
    bool operator==(const ChunkCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
    
    bool operator!=(const ChunkCoord& other) const {
        return !(*this == other);
    }
    
    bool operator<(const ChunkCoord& other) const {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }
};

// Hash function for ChunkCoord
struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& coord) const {
        // Szudzik pairing function for 3D coordinates
        auto pair2d = [](uint64_t a, uint64_t b) -> uint64_t {
            return a >= b ? a * a + a + b : a + b * b;
        };
        
        uint64_t x = static_cast<uint64_t>(coord.x + 0x7FFFFFFF);
        uint64_t y = static_cast<uint64_t>(coord.y + 0x7FFFFFFF);
        uint64_t z = static_cast<uint64_t>(coord.z + 0x7FFFFFFF);
        
        return pair2d(pair2d(x, y), z);
    }
};

// Block position within world
struct BlockPos {
    int32_t x, y, z;
    
    BlockPos() : x(0), y(0), z(0) {}
    BlockPos(int32_t x_, int32_t y_, int32_t z_) : x(x_), y(y_), z(z_) {}
    
    // Convert to chunk coordinate
    ChunkCoord ToChunkCoord(int32_t chunk_size) const {
        auto div_floor = [](int32_t a, int32_t b) {
            return (a >= 0) ? a / b : (a - b + 1) / b;
        };
        return ChunkCoord(
            div_floor(x, chunk_size),
            div_floor(y, chunk_size),
            div_floor(z, chunk_size)
        );
    }
    
    // Get local position within chunk
    std::array<int32_t, 3> ToLocalPos(int32_t chunk_size) const {
        auto mod_floor = [](int32_t a, int32_t b) {
            int32_t r = a % b;
            return (r >= 0) ? r : r + b;
        };
        return {
            mod_floor(x, chunk_size),
            mod_floor(y, chunk_size),
            mod_floor(z, chunk_size)
        };
    }
};

// Chunk data - Structure of Arrays for better cache performance
class Chunk {
public:
    static constexpr int32_t CHUNK_SIZE = 32;
    static constexpr int32_t CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
    
    Chunk(const ChunkCoord& coord);
    ~Chunk() = default;
    
    // Non-copyable but movable
    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    Chunk(Chunk&&) = default;
    Chunk& operator=(Chunk&&) = default;
    
    // Block access
    BlockType GetBlock(int32_t x, int32_t y, int32_t z) const;
    void SetBlock(int32_t x, int32_t y, int32_t z, BlockType type);
    
    uint8_t GetMetadata(int32_t x, int32_t y, int32_t z) const;
    void SetMetadata(int32_t x, int32_t y, int32_t z, uint8_t metadata);
    
    uint8_t GetLight(int32_t x, int32_t y, int32_t z) const;
    void SetLight(int32_t x, int32_t y, int32_t z, uint8_t light);
    
    // Bulk operations
    void Fill(BlockType type);
    void Clear();
    
    // Compression
    void Compress();
    void Decompress();
    bool IsCompressed() const { return m_compressed; }
    
    // State management
    bool IsDirty() const { return m_dirty; }
    void SetDirty(bool dirty = true) { m_dirty = dirty; }
    
    bool IsGenerated() const { return m_generated; }
    void SetGenerated(bool generated = true) { m_generated = generated; }
    
    bool IsLoaded() const { return m_loaded; }
    void SetLoaded(bool loaded = true) { m_loaded = loaded; }
    
    // Coordinates
    const ChunkCoord& GetCoord() const { return m_coord; }
    
    // Statistics
    size_t GetMemoryUsage() const;
    uint32_t GetBlockCount(BlockType type) const;
    bool IsEmpty() const;
    bool IsUniform() const;
    BlockType GetUniformType() const;
    
    // Access time for LRU
    void UpdateAccessTime();
    uint64_t GetLastAccessTime() const { return m_last_access_time; }
    
    // Neighbors (for lighting and generation)
    void SetNeighbor(int direction, std::shared_ptr<Chunk> neighbor);
    std::shared_ptr<Chunk> GetNeighbor(int direction) const;
    
private:
    ChunkCoord m_coord;
    
    // SoA storage for cache efficiency
    std::vector<uint16_t> m_blocks;     // Block types
    std::vector<uint8_t> m_metadata;   // Block metadata
    std::vector<uint8_t> m_light;      // Light levels
    
    // Compression support
    bool m_compressed = false;
    std::vector<uint8_t> m_compressed_data;
    
    // State flags
    std::atomic<bool> m_dirty{false};
    std::atomic<bool> m_generated{false};
    std::atomic<bool> m_loaded{false};
    
    // Access tracking for LRU
    std::atomic<uint64_t> m_last_access_time{0};
    
    // Neighbors for lighting and generation
    std::array<std::weak_ptr<Chunk>, 6> m_neighbors; // N, S, E, W, U, D
    mutable std::mutex m_neighbors_mutex;
    
    // Helper functions
    size_t GetIndex(int32_t x, int32_t y, int32_t z) const;
    bool IsValidCoord(int32_t x, int32_t y, int32_t z) const;
};

// Chunk edit for tracking modifications
struct ChunkEdit {
    BlockPos position;
    BlockType old_type;
    BlockType new_type;
    uint8_t old_metadata;
    uint8_t new_metadata;
    uint64_t timestamp;
    uint32_t player_id;
    
    ChunkEdit() = default;
    ChunkEdit(const BlockPos& pos, BlockType old_t, BlockType new_t,
              uint8_t old_meta = 0, uint8_t new_meta = 0, uint32_t pid = 0)
        : position(pos), old_type(old_t), new_type(new_t)
        , old_metadata(old_meta), new_metadata(new_meta)
        , timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now().time_since_epoch()).count())
        , player_id(pid) {}
};

// LRU cache for chunks
class ChunkLRUCache {
public:
    explicit ChunkLRUCache(size_t max_size);
    
    void Insert(const ChunkCoord& coord, std::shared_ptr<Chunk> chunk);
    std::shared_ptr<Chunk> Get(const ChunkCoord& coord);
    bool Contains(const ChunkCoord& coord) const;
    void Remove(const ChunkCoord& coord);
    void Clear();
    
    size_t Size() const;
    size_t MaxSize() const { return m_max_size; }
    void SetMaxSize(size_t max_size);
    
    // Get least recently used chunks for eviction
    std::vector<ChunkCoord> GetLRUChunks(size_t count) const;
    
private:
    struct CacheNode {
        ChunkCoord coord;
        std::shared_ptr<Chunk> chunk;
        uint64_t access_time;
    };
    
    mutable std::mutex m_mutex;
    size_t m_max_size;
    std::unordered_map<ChunkCoord, CacheNode, ChunkCoordHash> m_cache;
    
    void EvictLRU();
};

// World manager configuration
struct WorldConfig {
    int32_t chunk_size = 32;
    int32_t world_height = 256;
    int32_t sea_level = 64;
    
    // Chunk management
    int32_t render_distance = 8;
    int32_t simulation_distance = 6;
    int32_t load_distance = 10;
    float unload_delay = 30.0f;
    
    // Memory management
    size_t max_loaded_chunks = 1000;
    size_t chunk_cache_size = 500;
    bool compress_chunks = true;
    
    // Performance
    size_t generation_threads = 4;
    bool async_generation = true;
    bool async_lighting = true;
    
    // Limits
    int32_t world_radius = 512; // chunks
    int32_t max_chunk_y = 8;     // world_height / chunk_size
    int32_t min_chunk_y = -1;
};

// Forward declarations
class WorldGenerator;
class LightingEngine;

// Main world manager class
class WorldManager {
public:
    explicit WorldManager(const WorldConfig& config = WorldConfig{});
    ~WorldManager();
    
    // Initialization
    bool Initialize();
    void Shutdown();
    
    // Block access
    BlockType GetBlock(const BlockPos& pos) const;
    BlockType GetBlock(int32_t x, int32_t y, int32_t z) const;
    
    void SetBlock(const BlockPos& pos, BlockType type, uint32_t player_id = 0);
    void SetBlock(int32_t x, int32_t y, int32_t z, BlockType type, uint32_t player_id = 0);
    
    uint8_t GetMetadata(const BlockPos& pos) const;
    void SetMetadata(const BlockPos& pos, uint8_t metadata);
    
    uint8_t GetLight(const BlockPos& pos) const;
    void SetLight(const BlockPos& pos, uint8_t light);
    
    // Chunk management
    std::shared_ptr<Chunk> GetChunk(const ChunkCoord& coord) const;
    std::shared_ptr<Chunk> LoadChunk(const ChunkCoord& coord);
    void UnloadChunk(const ChunkCoord& coord);
    
    bool IsChunkLoaded(const ChunkCoord& coord) const;
    bool IsChunkGenerated(const ChunkCoord& coord) const;
    
    // Area loading/unloading
    void LoadArea(const ChunkCoord& center, int32_t radius);
    void UnloadArea(const ChunkCoord& center, int32_t radius);
    void UpdateLoadedChunks(const ChunkCoord& player_pos);
    
    // Generation
    void GenerateChunk(const ChunkCoord& coord);
    void GenerateArea(const ChunkCoord& center, int32_t radius);
    
    // Edit tracking
    void AddEdit(const ChunkEdit& edit);
    std::vector<ChunkEdit> GetEdits(const ChunkCoord& coord) const;
    std::vector<ChunkEdit> GetRecentEdits(uint64_t since_timestamp) const;
    void ClearEdits(const ChunkCoord& coord);
    
    // Statistics
    size_t GetLoadedChunkCount() const;
    size_t GetMemoryUsage() const;
    
    // Configuration
    const WorldConfig& GetConfig() const { return m_config; }
    void SetConfig(const WorldConfig& config);
    
    // Save/Load
    bool SaveChunk(const ChunkCoord& coord, const std::string& directory) const;
    bool LoadChunk(const ChunkCoord& coord, const std::string& directory);
    void SaveAll(const std::string& directory) const;
    void LoadAll(const std::string& directory);
    
    // Threading
    void SetGenerationThreads(size_t count);
    void WaitForGeneration();
    
    // Callbacks
    using ChunkLoadCallback = std::function<void(const ChunkCoord&, std::shared_ptr<Chunk>)>;
    using ChunkUnloadCallback = std::function<void(const ChunkCoord&)>;
    using BlockChangeCallback = std::function<void(const ChunkEdit&)>;
    
    void SetChunkLoadCallback(ChunkLoadCallback callback);
    void SetChunkUnloadCallback(ChunkUnloadCallback callback);
    void SetBlockChangeCallback(BlockChangeCallback callback);
    
private:
    WorldConfig m_config;
    
    // Chunk storage
    ChunkLRUCache m_chunk_cache;
    mutable std::shared_mutex m_chunks_mutex;
    
    // Edit tracking
    std::unordered_map<ChunkCoord, std::vector<ChunkEdit>, ChunkCoordHash> m_edits;
    mutable std::mutex m_edits_mutex;
    
    // Generation and lighting
    std::unique_ptr<WorldGenerator> m_generator;
    std::unique_ptr<LightingEngine> m_lighting;
    std::unique_ptr<ThreadPool> m_thread_pool;
    
    // Unloading queue
    struct UnloadRequest {
        ChunkCoord coord;
        uint64_t unload_time;
    };
    std::queue<UnloadRequest> m_unload_queue;
    mutable std::mutex m_unload_mutex;
    
    // Callbacks
    ChunkLoadCallback m_chunk_load_callback;
    ChunkUnloadCallback m_chunk_unload_callback;
    BlockChangeCallback m_block_change_callback;
    
    // Statistics
    mutable std::atomic<size_t> m_total_memory_usage{0};
    
    // Helper methods
    bool IsInBounds(const BlockPos& pos) const;
    bool IsInBounds(const ChunkCoord& coord) const;
    void ProcessUnloadQueue();
    void NotifyChunkLoad(const ChunkCoord& coord, std::shared_ptr<Chunk> chunk);
    void NotifyChunkUnload(const ChunkCoord& coord);
    void NotifyBlockChange(const ChunkEdit& edit);
};

} // namespace voxelvk