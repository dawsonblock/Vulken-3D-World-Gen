#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace voxelvk {

// Block properties and registry
enum class BlockType : uint16_t {
    Air = 0,
    Stone = 1,
    Dirt = 2,
    Grass = 3,
    Sand = 4,
    Water = 5,
    Wood = 6,
    Leaves = 7,
    Snow = 8,
    Bedrock = 9,
    CoalOre = 10,
    IronOre = 11,
    GoldOre = 12,
    DiamondOre = 13,
    Glass = 14,
    Cobblestone = 15,
    
    // Add more block types as needed
    MaxBlocks = 256
};

struct BlockProperties {
    uint16_t id;
    std::string name;
    
    // Physics properties
    bool is_solid = true;
    bool is_transparent = false;
    bool is_fluid = false;
    bool is_breakable = true;
    bool is_placeable = true;
    
    // Rendering properties
    bool is_opaque = true;
    bool cast_shadows = true;
    bool receive_shadows = true;
    uint8_t light_emission = 0;  // 0-15
    uint8_t light_absorption = 15; // 0-15
    
    // Material properties
    float hardness = 1.0f;
    float resistance = 1.0f;
    float friction = 0.6f;
    float restitution = 0.0f;
    
    // Mining properties
    uint8_t tool_type = 0;  // 0=hand, 1=pickaxe, 2=axe, 3=shovel
    uint8_t tool_level = 0; // 0=wood, 1=stone, 2=iron, 3=diamond
    float break_time = 1.0f;
    
    // Drops
    uint16_t drop_id = 0;      // What block drops when broken (0 = self)
    uint8_t drop_count_min = 1;
    uint8_t drop_count_max = 1;
    float drop_chance = 1.0f;
    
    // Texture/model info
    std::string texture_name;
    std::string model_name;
    
    // Custom properties (extensible)
    std::unordered_map<std::string, float> custom_floats;
    std::unordered_map<std::string, int32_t> custom_ints;
    std::unordered_map<std::string, std::string> custom_strings;
};

class BlockRegistry {
public:
    static BlockRegistry& Instance();
    
    // Register a block type
    void RegisterBlock(const BlockProperties& properties);
    
    // Get block properties
    const BlockProperties& GetProperties(BlockType type) const;
    const BlockProperties& GetProperties(uint16_t id) const;
    const BlockProperties* GetPropertiesPtr(BlockType type) const;
    const BlockProperties* GetPropertiesPtr(uint16_t id) const;
    
    // Query functions
    bool IsValid(BlockType type) const;
    bool IsValid(uint16_t id) const;
    bool IsSolid(BlockType type) const;
    bool IsTransparent(BlockType type) const;
    bool IsFluid(BlockType type) const;
    bool IsBreakable(BlockType type) const;
    bool IsPlaceable(BlockType type) const;
    bool IsOpaque(BlockType type) const;
    
    // Get lists of blocks by category
    std::vector<BlockType> GetSolidBlocks() const;
    std::vector<BlockType> GetTransparentBlocks() const;
    std::vector<BlockType> GetFluidBlocks() const;
    std::vector<BlockType> GetLightEmittingBlocks() const;
    
    // Block name lookup
    BlockType GetBlockByName(const std::string& name) const;
    std::string GetBlockName(BlockType type) const;
    
    // Get all registered blocks
    std::vector<BlockType> GetAllBlocks() const;
    size_t GetBlockCount() const;
    
    // Validation
    bool ValidateRegistry() const;
    
    // Serialization
    void SaveToFile(const std::string& filename) const;
    bool LoadFromFile(const std::string& filename);
    
    // Clear registry
    void Clear();
    
private:
    BlockRegistry() = default;
    
    std::unordered_map<uint16_t, BlockProperties> m_blocks;
    std::unordered_map<std::string, uint16_t> m_name_to_id;
    
    // Default properties for invalid blocks
    static const BlockProperties s_invalid_properties;
    
    void RegisterDefaultBlocks();
};

// Utility functions for block operations
namespace BlockUtils {
    // Block state encoding/decoding
    uint32_t EncodeBlockState(BlockType type, uint8_t metadata = 0);
    BlockType DecodeBlockType(uint32_t state);
    uint8_t DecodeMetadata(uint32_t state);
    
    // Block face/direction utilities
    enum class BlockFace : uint8_t {
        North = 0,  // -Z
        South = 1,  // +Z
        East = 2,   // +X
        West = 3,   // -X
        Up = 4,     // +Y
        Down = 5    // -Y
    };
    
    BlockFace GetOppositeFace(BlockFace face);
    std::array<int32_t, 3> GetFaceNormal(BlockFace face);
    std::array<int32_t, 3> GetFaceOffset(BlockFace face);
    
    // Block interaction helpers
    bool CanPlace(BlockType block, BlockType adjacent, BlockFace face);
    bool CanBreak(BlockType block, uint8_t tool_type, uint8_t tool_level);
    float GetBreakTime(BlockType block, uint8_t tool_type, uint8_t tool_level);
    
    // Light calculation helpers
    uint8_t GetLightLevel(BlockType block);
    uint8_t GetLightAbsorption(BlockType block);
    bool BlocksLight(BlockType block);
    
    // Collision helpers
    bool HasCollision(BlockType block);
    float GetFriction(BlockType block);
    float GetRestitution(BlockType block);
}

// Block modification events
struct BlockChangeEvent {
    int32_t x, y, z;
    BlockType old_type;
    BlockType new_type;
    uint8_t old_metadata;
    uint8_t new_metadata;
    
    enum class Cause {
        PlayerBreak,
        PlayerPlace,
        WorldGen,
        Physics,
        Scripted,
        Unknown
    } cause = Cause::Unknown;
    
    uint64_t timestamp = 0;
    uint32_t player_id = 0; // For player-caused changes
};

using BlockChangeCallback = std::function<void(const BlockChangeEvent&)>;

} // namespace voxelvk