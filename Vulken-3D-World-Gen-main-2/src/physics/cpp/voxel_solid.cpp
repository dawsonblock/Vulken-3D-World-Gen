#include "voxel_solid.hpp"
#include "collision_utils.hpp"
#include <iostream>
#include <algorithm>
#include <queue>
#include <vector>

namespace voxelvk::physics {

// BlockRegistry implementation
BlockRegistry::BlockRegistry() 
    : fallback_solid_(true) {
    // Initialize default properties - matches Python default behavior
    default_properties_.solid = false;  // Air by default
    default_properties_.transparent = true;
    default_properties_.liquid = false;
    default_properties_.collideable = false;
    
    initializeDefaultBlocks();
}

void BlockRegistry::initializeDefaultBlocks() {
    // Initialize block properties to match typical voxel game behavior
    
    // AIR - non-solid, transparent
    registerBlock(BlockType::AIR, {
        .solid = false,
        .transparent = true,
        .liquid = false,
        .collideable = false,
        .hardness = 0.0f,
        .friction = 0.0f,
        .restitution = 0.0f
    });
    
    // STONE - solid, opaque
    registerBlock(BlockType::STONE, {
        .solid = true,
        .transparent = false,
        .liquid = false,
        .collideable = true,
        .hardness = 1.5f,
        .friction = 0.8f,
        .restitution = 0.1f
    });
    
    // DIRT - solid, opaque
    registerBlock(BlockType::DIRT, {
        .solid = true,
        .transparent = false,
        .liquid = false,
        .collideable = true,
        .hardness = 0.5f,
        .friction = 0.7f,
        .restitution = 0.05f
    });
    
    // GRASS - solid, opaque
    registerBlock(BlockType::GRASS, {
        .solid = true,
        .transparent = false,
        .liquid = false,
        .collideable = true,
        .hardness = 0.6f,
        .friction = 0.7f,
        .restitution = 0.05f
    });
    
    // WOOD - solid, opaque
    registerBlock(BlockType::WOOD, {
        .solid = true,
        .transparent = false,
        .liquid = false,
        .collideable = true,
        .hardness = 2.0f,
        .friction = 0.6f,
        .restitution = 0.2f
    });
    
    // LEAVES - solid but with different properties
    registerBlock(BlockType::LEAVES, {
        .solid = true,
        .transparent = true,
        .liquid = false,
        .collideable = true,
        .hardness = 0.2f,
        .friction = 0.5f,
        .restitution = 0.3f
    });
    
    // WATER - liquid, transparent, non-solid for movement
    registerBlock(BlockType::WATER, {
        .solid = false,  // Players can move through water
        .transparent = true,
        .liquid = true,
        .collideable = false,  // Special liquid collision handling
        .hardness = 0.0f,
        .friction = 0.2f,
        .restitution = 0.0f
    });
    
    // SAND - solid, opaque
    registerBlock(BlockType::SAND, {
        .solid = true,
        .transparent = false,
        .liquid = false,
        .collideable = true,
        .hardness = 0.5f,
        .friction = 0.6f,
        .restitution = 0.02f
    });
    
    // GRAVEL - solid, opaque
    registerBlock(BlockType::GRAVEL, {
        .solid = true,
        .transparent = false,
        .liquid = false,
        .collideable = true,
        .hardness = 0.6f,
        .friction = 0.7f,
        .restitution = 0.05f
    });
    
    // COAL_ORE - solid, opaque
    registerBlock(BlockType::COAL_ORE, {
        .solid = true,
        .transparent = false,
        .liquid = false,
        .collideable = true,
        .hardness = 3.0f,
        .friction = 0.8f,
        .restitution = 0.1f
    });
    
    // IRON_ORE - solid, opaque
    registerBlock(BlockType::IRON_ORE, {
        .solid = true,
        .transparent = false,
        .liquid = false,
        .collideable = true,
        .hardness = 3.0f,
        .friction = 0.8f,
        .restitution = 0.1f
    });
}

bool BlockRegistry::isSolid(uint16_t block_type) const {
    // Direct port of Python is_solid logic
    try {
        auto it = block_properties_.find(block_type);
        if (it != block_properties_.end()) {
            return it->second.solid;
        }
        
        // Fallback behavior - matches Python: block_type != BlockType.AIR
        if (fallback_solid_) {
            return block_type != static_cast<uint16_t>(BlockType::AIR);
        } else {
            return default_properties_.solid;
        }
    } catch (const std::exception&) {
        // Exception handling matches Python fallback
        return block_type != static_cast<uint16_t>(BlockType::AIR);
    }
}

bool BlockRegistry::isTransparent(uint16_t block_type) const {
    auto it = block_properties_.find(block_type);
    if (it != block_properties_.end()) {
        return it->second.transparent;
    }
    return default_properties_.transparent;
}

bool BlockRegistry::isLiquid(uint16_t block_type) const {
    auto it = block_properties_.find(block_type);
    if (it != block_properties_.end()) {
        return it->second.liquid;
    }
    return default_properties_.liquid;
}

bool BlockRegistry::isCollideable(uint16_t block_type) const {
    auto it = block_properties_.find(block_type);
    if (it != block_properties_.end()) {
        return it->second.collideable;
    }
    return default_properties_.collideable;
}

const BlockProperties& BlockRegistry::getProperties(uint16_t block_type) const {
    auto it = block_properties_.find(block_type);
    if (it != block_properties_.end()) {
        return it->second;
    }
    return default_properties_;
}

void BlockRegistry::registerBlock(uint16_t block_type, const BlockProperties& properties) {
    block_properties_[block_type] = properties;
}

void BlockRegistry::registerBlock(BlockType block_type, const BlockProperties& properties) {
    registerBlock(static_cast<uint16_t>(block_type), properties);
}

void BlockRegistry::batchSolidityTest(const std::vector<uint16_t>& block_types, std::vector<bool>& results) const {
    results.resize(block_types.size());
    
    for (size_t i = 0; i < block_types.size(); ++i) {
        results[i] = isSolid(block_types[i]);
    }
}

std::vector<uint16_t> BlockRegistry::getRegisteredBlockTypes() const {
    std::vector<uint16_t> types;
    types.reserve(block_properties_.size());
    
    for (const auto& pair : block_properties_) {
        types.push_back(pair.first);
    }
    
    std::sort(types.begin(), types.end());
    return types;
}

void BlockRegistry::printBlockRegistry() const {
    std::cout << "Block Registry (" << block_properties_.size() << " registered blocks):" << std::endl;
    
    std::vector<uint16_t> types = getRegisteredBlockTypes();
    for (uint16_t type : types) {
        const BlockProperties& props = getProperties(type);
        std::cout << "  Block " << type << ": solid=" << props.solid 
                  << ", transparent=" << props.transparent 
                  << ", liquid=" << props.liquid
                  << ", collideable=" << props.collideable << std::endl;
    }
}

BlockRegistry& BlockRegistry::getInstance() {
    static BlockRegistry instance;
    return instance;
}

// FastBlockChecker implementation
FastBlockChecker::FastBlockChecker() : cache_valid_(false) {
    prebuildSolidSet();
}

void FastBlockChecker::prebuildSolidSet() {
    rebuildCache();
}

bool FastBlockChecker::isSolidFast(uint16_t block_type) const {
    if (!cache_valid_) {
        const_cast<FastBlockChecker*>(this)->rebuildCache();
    }
    
    return solid_blocks_.find(block_type) != solid_blocks_.end();
}

void FastBlockChecker::batchIsSolid(const uint16_t* block_types, uint8_t* results, size_t count) const {
    if (!cache_valid_) {
        const_cast<FastBlockChecker*>(this)->rebuildCache();
    }
    
    for (size_t i = 0; i < count; ++i) {
    results[i] = (solid_blocks_.find(block_types[i]) != solid_blocks_.end()) ? 1 : 0;
    }
}

void FastBlockChecker::invalidateCache() {
    cache_valid_ = false;
}

void FastBlockChecker::rebuildCache() {
    solid_blocks_.clear();
    
    BlockRegistry& registry = BlockRegistry::getInstance();
    std::vector<uint16_t> all_types = registry.getRegisteredBlockTypes();
    
    for (uint16_t type : all_types) {
        if (registry.isSolid(type)) {
            solid_blocks_.insert(type);
        }
    }
    
    // Also check common unregistered block types
    for (uint16_t type = 1; type < 256; ++type) {
        if (registry.isSolid(type)) {
            solid_blocks_.insert(type);
        }
    }
    
    cache_valid_ = true;
}

// Block utility functions
namespace block_utils {

bool regionContainsSolid(const collision_utils::WorldInterface& world,
                        const glm::ivec3& min_pos,
                        const glm::ivec3& max_pos) {
    for (int y = min_pos.y; y <= max_pos.y; ++y) {
        for (int z = min_pos.z; z <= max_pos.z; ++z) {
            for (int x = min_pos.x; x <= max_pos.x; ++x) {
                try {
                    uint16_t block_type = world.getBlockAtWorldPosition(
                        static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                    
                    if (world.isBlockSolid(block_type)) {
                        return true;
                    }
                } catch (const std::exception&) {
                    // Assume solid on error to be safe
                    return true;
                }
            }
        }
    }
    
    return false;
}

int countSolidBlocks(const collision_utils::WorldInterface& world,
                    const glm::ivec3& min_pos,
                    const glm::ivec3& max_pos) {
    int count = 0;
    
    for (int y = min_pos.y; y <= max_pos.y; ++y) {
        for (int z = min_pos.z; z <= max_pos.z; ++z) {
            for (int x = min_pos.x; x <= max_pos.x; ++x) {
                try {
                    uint16_t block_type = world.getBlockAtWorldPosition(
                        static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                    
                    if (world.isBlockSolid(block_type)) {
                        count++;
                    }
                } catch (const std::exception&) {
                    // Count errors as solid blocks
                    count++;
                }
            }
        }
    }
    
    return count;
}

bool findFirstSolidBlock(const collision_utils::WorldInterface& world,
                        const glm::vec3& start,
                        const glm::vec3& direction,
                        float max_distance,
                        glm::ivec3& hit_pos,
                        float& hit_distance) {
    glm::vec3 normalized_dir = glm::normalize(direction);
    const float step_size = 0.1f;
    
    for (float distance = 0.0f; distance <= max_distance; distance += step_size) {
        glm::vec3 current_pos = start + normalized_dir * distance;
        glm::ivec3 block_pos(
            static_cast<int>(std::floor(current_pos.x)),
            static_cast<int>(std::floor(current_pos.y)),
            static_cast<int>(std::floor(current_pos.z))
        );
        
        try {
            uint16_t block_type = world.getBlockAtWorldPosition(
                static_cast<float>(block_pos.x), 
                static_cast<float>(block_pos.y), 
                static_cast<float>(block_pos.z));
            
            if (world.isBlockSolid(block_type)) {
                hit_pos = block_pos;
                hit_distance = distance;
                return true;
            }
        } catch (const std::exception&) {
            // Treat errors as solid blocks
            hit_pos = block_pos;
            hit_distance = distance;
            return true;
        }
    }
    
    return false;
}

std::vector<glm::ivec3> floodFillSolid(const collision_utils::WorldInterface& world,
                                      const glm::ivec3& start_pos,
                                      int max_blocks) {
    std::vector<glm::ivec3> solid_blocks;
    std::queue<glm::ivec3> to_check;
    std::unordered_set<uint64_t> visited;
    
    // Helper to convert 3D position to unique hash
    auto positionHash = [](const glm::ivec3& pos) -> uint64_t {
        return (static_cast<uint64_t>(pos.x) << 20) | 
               (static_cast<uint64_t>(pos.y) << 10) | 
               static_cast<uint64_t>(pos.z);
    };
    
    to_check.push(start_pos);
    visited.insert(positionHash(start_pos));
    
    // 6-connected neighbors (no diagonals)
    const std::vector<glm::ivec3> neighbors = {
        {1, 0, 0}, {-1, 0, 0},
        {0, 1, 0}, {0, -1, 0},
        {0, 0, 1}, {0, 0, -1}
    };
    
    while (!to_check.empty() && solid_blocks.size() < static_cast<size_t>(max_blocks)) {
        glm::ivec3 current = to_check.front();
        to_check.pop();
        
        try {
            uint16_t block_type = world.getBlockAtWorldPosition(
                static_cast<float>(current.x),
                static_cast<float>(current.y),
                static_cast<float>(current.z));
            
            if (world.isBlockSolid(block_type)) {
                solid_blocks.push_back(current);
                
                // Add neighbors to check
                for (const glm::ivec3& offset : neighbors) {
                    glm::ivec3 neighbor = current + offset;
                    uint64_t hash = positionHash(neighbor);
                    
                    if (visited.find(hash) == visited.end()) {
                        visited.insert(hash);
                        to_check.push(neighbor);
                    }
                }
            }
        } catch (const std::exception&) {
            // Skip error blocks
            continue;
        }
    }
    
    return solid_blocks;
}

bool isOnGround(const collision_utils::WorldInterface& world,
               const glm::vec3& position,
               float check_distance) {
    glm::ivec3 block_pos;
    float hit_distance;
    
    return findFirstSolidBlock(world, position, glm::vec3(0, -1, 0), 
                              check_distance, block_pos, hit_distance);
}

float findGroundLevel(const collision_utils::WorldInterface& world,
                     const glm::vec3& position,
                     float max_search_distance) {
    glm::ivec3 hit_pos;
    float hit_distance;
    
    if (findFirstSolidBlock(world, position, glm::vec3(0, -1, 0), 
                           max_search_distance, hit_pos, hit_distance)) {
        // Return the top surface of the hit block
        return static_cast<float>(hit_pos.y) + 1.0f;
    }
    
    // No ground found, return far below
    return position.y - max_search_distance;
}

} // namespace block_utils

} // namespace voxelvk::physics