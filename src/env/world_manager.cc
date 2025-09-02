#include "world_manager.hpp"
#include "../core/nvtx_profiler.hpp"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <shared_mutex>

namespace voxelvk {

// Provide no-op deleters for forward-declared unique_ptr types
void WorldGeneratorDeleter::operator()(WorldGenerator* p) const noexcept { /* no-op, not owned or defined here */ }
void LightingEngineDeleter::operator()(LightingEngine* p) const noexcept { /* no-op, not owned or defined here */ }

// Chunk implementation
Chunk::Chunk(const ChunkCoord& coord) : m_coord(coord) {
    m_blocks.resize(CHUNK_VOLUME, static_cast<uint16_t>(BlockType::Air));
    m_metadata.resize(CHUNK_VOLUME, 0);
    m_light.resize(CHUNK_VOLUME, 0);
    UpdateAccessTime();
}

BlockType Chunk::GetBlock(int32_t x, int32_t y, int32_t z) const {
    if (!IsValidCoord(x, y, z)) {
        return BlockType::Air;
    }
    
    UpdateAccessTime();
    
    if (m_compressed) {
        // Would need to decompress or implement compressed access
        VXL_WARN("Accessing compressed chunk - consider decompressing first");
        return BlockType::Air;
    }
    
    size_t index = GetIndex(x, y, z);
    return static_cast<BlockType>(m_blocks[index]);
}

void Chunk::SetBlock(int32_t x, int32_t y, int32_t z, BlockType type) {
    if (!IsValidCoord(x, y, z)) {
        return;
    }
    
    UpdateAccessTime();
    
    if (m_compressed) {
        Decompress();
    }
    
    size_t index = GetIndex(x, y, z);
    if (m_blocks[index] != static_cast<uint16_t>(type)) {
        m_blocks[index] = static_cast<uint16_t>(type);
        SetDirty(true);
    }
}

uint8_t Chunk::GetMetadata(int32_t x, int32_t y, int32_t z) const {
    if (!IsValidCoord(x, y, z) || m_compressed) {
        return 0;
    }
    
    UpdateAccessTime();
    size_t index = GetIndex(x, y, z);
    return m_metadata[index];
}

void Chunk::SetMetadata(int32_t x, int32_t y, int32_t z, uint8_t metadata) {
    if (!IsValidCoord(x, y, z)) {
        return;
    }
    
    UpdateAccessTime();
    
    if (m_compressed) {
        Decompress();
    }
    
    size_t index = GetIndex(x, y, z);
    if (m_metadata[index] != metadata) {
        m_metadata[index] = metadata;
        SetDirty(true);
    }
}

uint8_t Chunk::GetLight(int32_t x, int32_t y, int32_t z) const {
    if (!IsValidCoord(x, y, z) || m_compressed) {
        return 0;
    }
    
    UpdateAccessTime();
    size_t index = GetIndex(x, y, z);
    return m_light[index];
}

void Chunk::SetLight(int32_t x, int32_t y, int32_t z, uint8_t light) {
    if (!IsValidCoord(x, y, z)) {
        return;
    }
    
    UpdateAccessTime();
    
    if (m_compressed) {
        Decompress();
    }
    
    size_t index = GetIndex(x, y, z);
    m_light[index] = light;
}

void Chunk::Fill(BlockType type) {
    if (m_compressed) {
        Decompress();
    }
    
    uint16_t block_id = static_cast<uint16_t>(type);
    std::fill(m_blocks.begin(), m_blocks.end(), block_id);
    std::fill(m_metadata.begin(), m_metadata.end(), 0);
    std::fill(m_light.begin(), m_light.end(), 0);
    
    SetDirty(true);
    UpdateAccessTime();
}

void Chunk::Clear() {
    Fill(BlockType::Air);
}

void Chunk::Compress() {
    if (m_compressed) {
        return;
    }
    
    VXL_NVTX_RANGE("Chunk::Compress");
    
    // Simple RLE compression for blocks
    m_compressed_data.clear();
    
    if (IsUniform()) {
        // Special case for uniform chunks
        m_compressed_data.push_back(0xFF); // Uniform marker
        uint16_t uniform_type = m_blocks[0];
        m_compressed_data.push_back(uniform_type & 0xFF);
        m_compressed_data.push_back((uniform_type >> 8) & 0xFF);
    } else {
        // RLE compression
        m_compressed_data.push_back(0x00); // RLE marker
        
        for (size_t i = 0; i < m_blocks.size(); ) {
            uint16_t current = m_blocks[i];
            size_t count = 1;
            
            // Count consecutive identical blocks
            while (i + count < m_blocks.size() && 
                   m_blocks[i + count] == current && 
                   count < 255) {
                count++;
            }
            
            // Write count and block type
            m_compressed_data.push_back(static_cast<uint8_t>(count));
            m_compressed_data.push_back(current & 0xFF);
            m_compressed_data.push_back((current >> 8) & 0xFF);
            
            i += count;
        }
    }
    
    // Free original data if compression is effective
    if (m_compressed_data.size() < m_blocks.size() * 2) {
        m_blocks.clear();
        m_metadata.clear();
        m_light.clear();
        m_compressed = true;
        
        VXL_DEBUG("Compressed chunk {} from {} to {} bytes", 
                  GetCoord().x, CHUNK_VOLUME * 3, m_compressed_data.size());
    } else {
        m_compressed_data.clear();
    }
}

void Chunk::Decompress() {
    if (!m_compressed) {
        return;
    }
    
    VXL_NVTX_RANGE("Chunk::Decompress");
    
    // Restore arrays
    m_blocks.resize(CHUNK_VOLUME);
    m_metadata.resize(CHUNK_VOLUME, 0);
    m_light.resize(CHUNK_VOLUME, 0);
    
    if (m_compressed_data.empty()) {
        std::fill(m_blocks.begin(), m_blocks.end(), static_cast<uint16_t>(BlockType::Air));
    } else if (m_compressed_data[0] == 0xFF) {
        // Uniform chunk
        uint16_t uniform_type = m_compressed_data[1] | (m_compressed_data[2] << 8);
        std::fill(m_blocks.begin(), m_blocks.end(), uniform_type);
    } else {
        // RLE decompression
        size_t out_index = 0;
        for (size_t i = 1; i < m_compressed_data.size() && out_index < CHUNK_VOLUME; i += 3) {
            uint8_t count = m_compressed_data[i];
            uint16_t block_type = m_compressed_data[i + 1] | (m_compressed_data[i + 2] << 8);
            
            for (uint8_t j = 0; j < count && out_index < CHUNK_VOLUME; j++) {
                m_blocks[out_index++] = block_type;
            }
        }
    }
    
    m_compressed_data.clear();
    m_compressed = false;
}

size_t Chunk::GetMemoryUsage() const {
    size_t usage = sizeof(Chunk);
    
    if (m_compressed) {
        usage += m_compressed_data.size();
    } else {
        usage += m_blocks.size() * sizeof(uint16_t);
        usage += m_metadata.size() * sizeof(uint8_t);
        usage += m_light.size() * sizeof(uint8_t);
    }
    
    return usage;
}

uint32_t Chunk::GetBlockCount(BlockType type) const {
    if (m_compressed) {
        if (IsUniform() && GetUniformType() == type) {
            return CHUNK_VOLUME;
        }
        return 0; // Would need to decompress for accurate count
    }
    
    uint16_t target = static_cast<uint16_t>(type);
    return static_cast<uint32_t>(std::count(m_blocks.begin(), m_blocks.end(), target));
}

bool Chunk::IsEmpty() const {
    return GetBlockCount(BlockType::Air) == CHUNK_VOLUME;
}

bool Chunk::IsUniform() const {
    if (m_compressed) {
        return !m_compressed_data.empty() && m_compressed_data[0] == 0xFF;
    }
    
    if (m_blocks.empty()) {
        return true;
    }
    
    uint16_t first = m_blocks[0];
    return std::all_of(m_blocks.begin(), m_blocks.end(), 
                      [first](uint16_t block) { return block == first; });
}

BlockType Chunk::GetUniformType() const {
    if (IsUniform()) {
        if (m_compressed && !m_compressed_data.empty()) {
            uint16_t type = m_compressed_data[1] | (m_compressed_data[2] << 8);
            return static_cast<BlockType>(type);
        } else if (!m_blocks.empty()) {
            return static_cast<BlockType>(m_blocks[0]);
        }
    }
    return BlockType::Air;
}

void Chunk::UpdateAccessTime() const {
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    m_last_access_time.store(static_cast<uint64_t>(timestamp));
}

void Chunk::SetNeighbor(int direction, std::shared_ptr<Chunk> neighbor) {
    if (direction >= 0 && direction < 6) {
        std::lock_guard<std::mutex> lock(m_neighbors_mutex);
        m_neighbors[direction] = neighbor;
    }
}

std::shared_ptr<Chunk> Chunk::GetNeighbor(int direction) const {
    if (direction >= 0 && direction < 6) {
        std::lock_guard<std::mutex> lock(m_neighbors_mutex);
        return m_neighbors[direction].lock();
    }
    return nullptr;
}

size_t Chunk::GetIndex(int32_t x, int32_t y, int32_t z) const {
    return static_cast<size_t>(y * CHUNK_SIZE * CHUNK_SIZE + z * CHUNK_SIZE + x);
}

bool Chunk::IsValidCoord(int32_t x, int32_t y, int32_t z) const {
    return x >= 0 && x < CHUNK_SIZE && 
           y >= 0 && y < CHUNK_SIZE && 
           z >= 0 && z < CHUNK_SIZE;
}

// ChunkLRUCache implementation
ChunkLRUCache::ChunkLRUCache(size_t max_size) : m_max_size(max_size) {
}

void ChunkLRUCache::Insert(const ChunkCoord& coord, std::shared_ptr<Chunk> chunk) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::steady_clock::now();
    uint64_t access_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    m_cache[coord] = CacheNode{coord, chunk, access_time};
    
    while (m_cache.size() > m_max_size) {
        EvictLRU();
    }
}

std::shared_ptr<Chunk> ChunkLRUCache::Get(const ChunkCoord& coord) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_cache.find(coord);
    if (it != m_cache.end()) {
        // Update access time
        auto now = std::chrono::steady_clock::now();
        it->second.access_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        return it->second.chunk;
    }
    
    return nullptr;
}

std::shared_ptr<Chunk> ChunkLRUCache::Get(const ChunkCoord& coord) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(coord);
    if (it != m_cache.end()) {
        // Do not mutate access time in const overload
        return it->second.chunk;
    }
    return nullptr;
}

bool ChunkLRUCache::Contains(const ChunkCoord& coord) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cache.find(coord) != m_cache.end();
}

void ChunkLRUCache::Remove(const ChunkCoord& coord) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.erase(coord);
}

void ChunkLRUCache::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.clear();
}

size_t ChunkLRUCache::Size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cache.size();
}

void ChunkLRUCache::SetMaxSize(size_t max_size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_max_size = max_size;
    
    while (m_cache.size() > m_max_size) {
        EvictLRU();
    }
}

std::vector<ChunkCoord> ChunkLRUCache::GetLRUChunks(size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::pair<uint64_t, ChunkCoord>> time_coords;
    time_coords.reserve(m_cache.size());
    
    for (const auto& [coord, node] : m_cache) {
        time_coords.emplace_back(node.access_time, coord);
    }
    
    // Sort by access time (oldest first)
    std::sort(time_coords.begin(), time_coords.end());
    
    std::vector<ChunkCoord> result;
    result.reserve(std::min(count, time_coords.size()));
    
    for (size_t i = 0; i < std::min(count, time_coords.size()); i++) {
        result.push_back(time_coords[i].second);
    }
    
    return result;
}

void ChunkLRUCache::EvictLRU() {
    if (m_cache.empty()) {
        return;
    }
    
    // Find the least recently accessed chunk
    auto oldest_it = m_cache.begin();
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it->second.access_time < oldest_it->second.access_time) {
            oldest_it = it;
        }
    }
    
    VXL_DEBUG("Evicting chunk ({}, {}, {}) from cache", 
              oldest_it->first.x, oldest_it->first.y, oldest_it->first.z);
    
    m_cache.erase(oldest_it);
}

// WorldManager implementation
WorldManager::WorldManager(const WorldConfig& config) 
    : m_config(config)
    , m_chunk_cache(config.chunk_cache_size) {
}

WorldManager::~WorldManager() {
    Shutdown();
}

bool WorldManager::Initialize() {
    VXL_INFO("Initializing WorldManager...");
    
    // Initialize thread pool
    m_thread_pool = std::make_unique<ThreadPool>(m_config.generation_threads);
    
    // TODO: Initialize world generator and lighting engine
    // m_generator = std::make_unique<WorldGenerator>(m_config);
    // m_lighting = std::make_unique<LightingEngine>(m_config);
    
    VXL_INFO("WorldManager initialized with {} generation threads", m_config.generation_threads);
    return true;
}

void WorldManager::Shutdown() {
    VXL_INFO("Shutting down WorldManager...");
    
    if (m_thread_pool) {
        m_thread_pool->WaitForAll();
        m_thread_pool.reset();
    }
    
    // Save dirty chunks before shutdown
    // TODO: Implement save functionality
    
    m_chunk_cache.Clear();
    
    VXL_INFO("WorldManager shutdown complete");
}

BlockType WorldManager::GetBlock(const BlockPos& pos) const {
    return GetBlock(pos.x, pos.y, pos.z);
}

BlockType WorldManager::GetBlock(int32_t x, int32_t y, int32_t z) const {
    BlockPos pos(x, y, z);
    if (!IsInBounds(pos)) {
        return BlockType::Air;
    }
    
    ChunkCoord chunk_coord = pos.ToChunkCoord(m_config.chunk_size);
    auto chunk = GetChunk(chunk_coord);
    
    if (!chunk) {
        return BlockType::Air;
    }
    
    auto local_pos = pos.ToLocalPos(m_config.chunk_size);
    return chunk->GetBlock(local_pos[0], local_pos[1], local_pos[2]);
}

void WorldManager::SetBlock(const BlockPos& pos, BlockType type, uint32_t player_id) {
    SetBlock(pos.x, pos.y, pos.z, type, player_id);
}

void WorldManager::SetBlock(int32_t x, int32_t y, int32_t z, BlockType type, uint32_t player_id) {
    BlockPos pos(x, y, z);
    if (!IsInBounds(pos)) {
        return;
    }
    
    ChunkCoord chunk_coord = pos.ToChunkCoord(m_config.chunk_size);
    auto chunk = GetChunk(chunk_coord);
    
    if (!chunk) {
        // Try to load/generate the chunk
        chunk = LoadChunk(chunk_coord);
        if (!chunk) {
            VXL_WARN("Failed to load chunk for block placement at ({}, {}, {})", x, y, z);
            return;
        }
    }
    
    auto local_pos = pos.ToLocalPos(m_config.chunk_size);
    BlockType old_type = chunk->GetBlock(local_pos[0], local_pos[1], local_pos[2]);
    
    if (old_type != type) {
        chunk->SetBlock(local_pos[0], local_pos[1], local_pos[2], type);
        
        // Record the edit
        ChunkEdit edit(pos, old_type, type, 0, 0, player_id);
        AddEdit(edit);
        NotifyBlockChange(edit);
        
        // TODO: Update lighting if needed
        // TODO: Notify neighboring chunks if block is on boundary
    }
}

std::shared_ptr<Chunk> WorldManager::GetChunk(const ChunkCoord& coord) const {
    return m_chunk_cache.Get(coord);
}

std::shared_ptr<Chunk> WorldManager::LoadChunk(const ChunkCoord& coord) {
    if (!IsInBounds(coord)) {
        return nullptr;
    }
    
    auto existing = GetChunk(coord);
    if (existing) {
        return existing;
    }
    
    VXL_NVTX_RANGE_COLOR("WorldManager::LoadChunk", NVTXProfiler::Color::Blue);
    
    // Create new chunk
    auto chunk = std::make_shared<Chunk>(coord);
    
    // Try to load from disk first
    // TODO: Implement chunk loading from disk
    
    // If not found on disk, generate it
    if (!chunk->IsGenerated()) {
        if (m_config.async_generation && m_thread_pool) {
            // Async generation
            m_thread_pool->SubmitDetached([this, chunk]() {
                GenerateChunk(chunk->GetCoord());
            });
        } else {
            // Sync generation
            GenerateChunk(coord);
        }
    }
    
    // Add to cache
    m_chunk_cache.Insert(coord, chunk);
    
    NotifyChunkLoad(coord, chunk);
    
    return chunk;
}

void WorldManager::UnloadChunk(const ChunkCoord& coord) {
    auto chunk = GetChunk(coord);
    if (!chunk) {
        return;
    }
    
    VXL_NVTX_RANGE_COLOR("WorldManager::UnloadChunk", NVTXProfiler::Color::Red);
    
    // Save if dirty
    if (chunk->IsDirty()) {
        // TODO: Save chunk to disk
        VXL_DEBUG("Saving dirty chunk ({}, {}, {})", coord.x, coord.y, coord.z);
    }
    
    // Compress if enabled
    if (m_config.compress_chunks) {
        chunk->Compress();
    }
    
    m_chunk_cache.Remove(coord);
    NotifyChunkUnload(coord);
}

bool WorldManager::IsChunkLoaded(const ChunkCoord& coord) const {
    return m_chunk_cache.Contains(coord);
}

bool WorldManager::IsChunkGenerated(const ChunkCoord& coord) const {
    auto chunk = GetChunk(coord);
    return chunk && chunk->IsGenerated();
}

void WorldManager::GenerateChunk(const ChunkCoord& coord) {
    VXL_NVTX_RANGE_COLOR("WorldManager::GenerateChunk", NVTXProfiler::Color::Green);
    
    auto chunk = GetChunk(coord);
    if (!chunk) {
        VXL_WARN("Cannot generate chunk - chunk not loaded: ({}, {}, {})", 
                 coord.x, coord.y, coord.z);
        return;
    }
    
    if (chunk->IsGenerated()) {
        return;
    }
    
    // Keep default chunks empty for deterministic tests; world generation can populate as needed.
    chunk->Fill(BlockType::Air);
    
    chunk->SetGenerated(true);
    chunk->SetDirty(true);
    
    VXL_DEBUG("Generated chunk ({}, {}, {})", coord.x, coord.y, coord.z);
}

void WorldManager::AddEdit(const ChunkEdit& edit) {
    ChunkCoord coord = edit.position.ToChunkCoord(m_config.chunk_size);
    
    std::lock_guard<std::mutex> lock(m_edits_mutex);
    m_edits[coord].push_back(edit);
}

size_t WorldManager::GetLoadedChunkCount() const {
    return m_chunk_cache.Size();
}

size_t WorldManager::GetMemoryUsage() const {
    size_t total = 0;
    
    // TODO: Calculate actual memory usage from all loaded chunks
    // This would require iterating through cache and summing chunk memory usage
    
    return total;
}

bool WorldManager::IsInBounds(const BlockPos& pos) const {
    return pos.y >= 0 && pos.y < m_config.world_height;
}

bool WorldManager::IsInBounds(const ChunkCoord& coord) const {
    return coord.y >= m_config.min_chunk_y && coord.y <= m_config.max_chunk_y &&
           std::abs(coord.x) <= m_config.world_radius &&
           std::abs(coord.z) <= m_config.world_radius;
}

void WorldManager::NotifyChunkLoad(const ChunkCoord& coord, std::shared_ptr<Chunk> chunk) {
    if (m_chunk_load_callback) {
        m_chunk_load_callback(coord, chunk);
    }
}

void WorldManager::NotifyChunkUnload(const ChunkCoord& coord) {
    if (m_chunk_unload_callback) {
        m_chunk_unload_callback(coord);
    }
}

void WorldManager::NotifyBlockChange(const ChunkEdit& edit) {
    if (m_block_change_callback) {
        m_block_change_callback(edit);
    }
}

void WorldManager::SetChunkLoadCallback(ChunkLoadCallback callback) {
    m_chunk_load_callback = callback;
}

void WorldManager::SetChunkUnloadCallback(ChunkUnloadCallback callback) {
    m_chunk_unload_callback = callback;
}

void WorldManager::SetBlockChangeCallback(BlockChangeCallback callback) {
    m_block_change_callback = callback;
}

} // namespace voxelvk