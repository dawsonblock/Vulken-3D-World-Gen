#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstddef>
#include <glm/glm.hpp>

namespace voxelvk::physics {

// Forward declarations
namespace collision_utils {
    class WorldInterface;
}

/**
 * Block type definitions and solidity checking
 * Direct port of Python voxel_solid.py functionality
 */

// Block type constants - matches Python BlockType enum
enum class BlockType : uint16_t {
    AIR = 0,
    STONE = 1,
    DIRT = 2,
    GRASS = 3,
    WOOD = 4,
    LEAVES = 5,
    WATER = 6,
    SAND = 7,
    GRAVEL = 8,
    COAL_ORE = 9,
    IRON_ORE = 10,
    // Add more block types as needed
    UNKNOWN = 65535
};

/**
 * Block properties structure
 */
struct BlockProperties {
    bool solid = true;           // Whether the block blocks movement
    bool transparent = false;    // Whether the block is see-through
    bool liquid = false;         // Whether the block is a liquid
    bool collideable = true;     // Whether the block participates in collision
    float hardness = 1.0f;       // How hard the block is to break
    float friction = 0.7f;       // Surface friction coefficient
    float restitution = 0.1f;    // Bounce coefficient
};

/**
 * Block registry for managing block properties
 */
class BlockRegistry {
public:
    BlockRegistry();
    
    // Property queries
    bool isSolid(uint16_t block_type) const;
    bool isTransparent(uint16_t block_type) const;
    bool isLiquid(uint16_t block_type) const;
    bool isCollideable(uint16_t block_type) const;
    
    // Get full properties
    const BlockProperties& getProperties(uint16_t block_type) const;
    
    // Registration methods
    void registerBlock(uint16_t block_type, const BlockProperties& properties);
    void registerBlock(BlockType block_type, const BlockProperties& properties);
    
    // Batch operations for performance
    void batchSolidityTest(const std::vector<uint16_t>& block_types, std::vector<bool>& results) const;
    
    // Configuration
    void setDefaultSolid(bool solid) { default_properties_.solid = solid; }
    void setFallbackBehavior(bool treat_unknown_as_solid) { 
        fallback_solid_ = treat_unknown_as_solid; 
    }
    
    // Statistics and debugging
    size_t getRegisteredBlockCount() const { return block_properties_.size(); }
    std::vector<uint16_t> getRegisteredBlockTypes() const;
    void printBlockRegistry() const;
    
    // Singleton access
    static BlockRegistry& getInstance();
    
private:
    std::unordered_map<uint16_t, BlockProperties> block_properties_;
    BlockProperties default_properties_;
    bool fallback_solid_;
    
    void initializeDefaultBlocks();
};

/**
 * Global convenience functions - matches Python interface exactly
 */

// Direct port of Python is_solid function
inline bool is_solid(uint16_t block_type) {
    return BlockRegistry::getInstance().isSolid(block_type);
}

// Additional convenience functions
inline bool is_transparent(uint16_t block_type) {
    return BlockRegistry::getInstance().isTransparent(block_type);
}

inline bool is_liquid(uint16_t block_type) {
    return BlockRegistry::getInstance().isLiquid(block_type);
}

inline bool is_collideable(uint16_t block_type) {
    return BlockRegistry::getInstance().isCollideable(block_type);
}

/**
 * World interface adapter for collision system
 * Provides a bridge between world managers and collision utilities
 * Implementation moved to voxel_solid.cpp to avoid circular dependencies
 */

/**
 * Performance-optimized block checking for collision hot paths
 */
class FastBlockChecker {
public:
    FastBlockChecker();
    
    // Pre-populate solid block set for O(1) lookups
    void prebuildSolidSet();
    
    // Ultra-fast solidity check using hash set
    bool isSolidFast(uint16_t block_type) const;
    
    // Batch checking with vectorized operations where possible
    // Writes 0/1 into results to avoid vector<bool>::data pitfalls
    void batchIsSolid(const uint16_t* block_types, uint8_t* results, size_t count) const;
    
    // Update cache when block registry changes
    void invalidateCache();
    
private:
    std::unordered_set<uint16_t> solid_blocks_;
    bool cache_valid_;
    
    void rebuildCache();
};

    /**
     * Block checking utilities for common patterns
     */
    namespace block_utils {
        
        // Check if a 3D region contains any solid blocks
        bool regionContainsSolid(const collision_utils::WorldInterface& world,
                                const glm::ivec3& min_pos,
                                const glm::ivec3& max_pos);
        
        // Count solid blocks in a region
        int countSolidBlocks(const collision_utils::WorldInterface& world,
                            const glm::ivec3& min_pos,
                            const glm::ivec3& max_pos);
        
        // Find first solid block along a ray
        bool findFirstSolidBlock(const collision_utils::WorldInterface& world,
                                const glm::vec3& start,
                                const glm::vec3& direction,
                                float max_distance,
                                glm::ivec3& hit_pos,
                                float& hit_distance);
        
        // Flood fill to find connected solid regions
        std::vector<glm::ivec3> floodFillSolid(const collision_utils::WorldInterface& world,
                                              const glm::ivec3& start_pos,
                                              int max_blocks = 1000);
        
        // Check if position is on ground (has solid block below)
        bool isOnGround(const collision_utils::WorldInterface& world,
                       const glm::vec3& position,
                       float check_distance = 1.0f);
        
        // Find ground level below a position
        float findGroundLevel(const collision_utils::WorldInterface& world,
                             const glm::vec3& position,
                             float max_search_distance = 100.0f);
    }

} // namespace voxelvk::physics