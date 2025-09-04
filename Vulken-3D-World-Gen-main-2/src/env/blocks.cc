#include "blocks.hpp"
#include "../core/logger.hpp"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace voxelvk {

// Default invalid block properties
const BlockProperties BlockRegistry::s_invalid_properties = {
    .id = 0,
    .name = "invalid",
    .is_solid = false,
    .is_transparent = true,
    .is_fluid = false,
    .is_breakable = false,
    .is_placeable = false,
    .is_opaque = false,
    .cast_shadows = false,
    .receive_shadows = false,
    .light_emission = 0,
    .light_absorption = 0,
    .hardness = 0.0f,
    .resistance = 0.0f,
    .friction = 0.0f,
    .restitution = 0.0f,
    .tool_type = 0,
    .tool_level = 0,
    .break_time = 0.0f,
    .drop_id = 0,
    .drop_count_min = 0,
    .drop_count_max = 0,
    .drop_chance = 0.0f,
    .texture_name = "",
    .model_name = ""
};

BlockRegistry& BlockRegistry::Instance() {
    static BlockRegistry instance;
    static bool initialized = false;
    
    if (!initialized) {
        instance.RegisterDefaultBlocks();
        initialized = true;
    }
    
    return instance;
}

void BlockRegistry::RegisterBlock(const BlockProperties& properties) {
    if (m_blocks.find(properties.id) != m_blocks.end()) {
        VXL_WARN("Block ID {} already registered, overwriting", properties.id);
    }
    
    // Remove old name mapping if it exists
    auto old_name_it = m_name_to_id.find(properties.name);
    if (old_name_it != m_name_to_id.end()) {
        m_name_to_id.erase(old_name_it);
    }
    
    m_blocks[properties.id] = properties;
    m_name_to_id[properties.name] = properties.id;
    
    VXL_DEBUG("Registered block: {} (ID: {})", properties.name, properties.id);
}

const BlockProperties& BlockRegistry::GetProperties(BlockType type) const {
    return GetProperties(static_cast<uint16_t>(type));
}

const BlockProperties& BlockRegistry::GetProperties(uint16_t id) const {
    auto it = m_blocks.find(id);
    return (it != m_blocks.end()) ? it->second : s_invalid_properties;
}

const BlockProperties* BlockRegistry::GetPropertiesPtr(BlockType type) const {
    return GetPropertiesPtr(static_cast<uint16_t>(type));
}

const BlockProperties* BlockRegistry::GetPropertiesPtr(uint16_t id) const {
    auto it = m_blocks.find(id);
    return (it != m_blocks.end()) ? &it->second : nullptr;
}

bool BlockRegistry::IsValid(BlockType type) const {
    return IsValid(static_cast<uint16_t>(type));
}

bool BlockRegistry::IsValid(uint16_t id) const {
    return m_blocks.find(id) != m_blocks.end();
}

bool BlockRegistry::IsSolid(BlockType type) const {
    return GetProperties(type).is_solid;
}

bool BlockRegistry::IsTransparent(BlockType type) const {
    return GetProperties(type).is_transparent;
}

bool BlockRegistry::IsFluid(BlockType type) const {
    return GetProperties(type).is_fluid;
}

bool BlockRegistry::IsBreakable(BlockType type) const {
    return GetProperties(type).is_breakable;
}

bool BlockRegistry::IsPlaceable(BlockType type) const {
    return GetProperties(type).is_placeable;
}

bool BlockRegistry::IsOpaque(BlockType type) const {
    return GetProperties(type).is_opaque;
}

std::vector<BlockType> BlockRegistry::GetSolidBlocks() const {
    std::vector<BlockType> result;
    for (const auto& [id, props] : m_blocks) {
        if (props.is_solid) {
            result.push_back(static_cast<BlockType>(id));
        }
    }
    return result;
}

std::vector<BlockType> BlockRegistry::GetTransparentBlocks() const {
    std::vector<BlockType> result;
    for (const auto& [id, props] : m_blocks) {
        if (props.is_transparent) {
            result.push_back(static_cast<BlockType>(id));
        }
    }
    return result;
}

std::vector<BlockType> BlockRegistry::GetFluidBlocks() const {
    std::vector<BlockType> result;
    for (const auto& [id, props] : m_blocks) {
        if (props.is_fluid) {
            result.push_back(static_cast<BlockType>(id));
        }
    }
    return result;
}

std::vector<BlockType> BlockRegistry::GetLightEmittingBlocks() const {
    std::vector<BlockType> result;
    for (const auto& [id, props] : m_blocks) {
        if (props.light_emission > 0) {
            result.push_back(static_cast<BlockType>(id));
        }
    }
    return result;
}

BlockType BlockRegistry::GetBlockByName(const std::string& name) const {
    auto it = m_name_to_id.find(name);
    return (it != m_name_to_id.end()) ? static_cast<BlockType>(it->second) : BlockType::Air;
}

std::string BlockRegistry::GetBlockName(BlockType type) const {
    const auto& props = GetProperties(type);
    return props.name;
}

std::vector<BlockType> BlockRegistry::GetAllBlocks() const {
    std::vector<BlockType> result;
    result.reserve(m_blocks.size());
    for (const auto& [id, props] : m_blocks) {
        result.push_back(static_cast<BlockType>(id));
    }
    return result;
}

size_t BlockRegistry::GetBlockCount() const {
    return m_blocks.size();
}

bool BlockRegistry::ValidateRegistry() const {
    bool valid = true;
    
    // Check for duplicate names
    std::unordered_map<std::string, uint16_t> name_count;
    for (const auto& [id, props] : m_blocks) {
        name_count[props.name]++;
        if (name_count[props.name] > 1) {
            VXL_ERROR("Duplicate block name: {}", props.name);
            valid = false;
        }
    }
    
    // Check for air block
    if (!IsValid(static_cast<uint16_t>(BlockType::Air))) {
        VXL_ERROR("Air block (ID 0) is not registered");
        valid = false;
    }
    
    return valid;
}

void BlockRegistry::SaveToFile(const std::string& filename) const {
    nlohmann::json j;
    
    for (const auto& [id, props] : m_blocks) {
        nlohmann::json block_json;
        block_json["id"] = props.id;
        block_json["name"] = props.name;
        block_json["is_solid"] = props.is_solid;
        block_json["is_transparent"] = props.is_transparent;
        block_json["is_fluid"] = props.is_fluid;
        block_json["is_breakable"] = props.is_breakable;
        block_json["is_placeable"] = props.is_placeable;
        block_json["is_opaque"] = props.is_opaque;
        block_json["cast_shadows"] = props.cast_shadows;
        block_json["receive_shadows"] = props.receive_shadows;
        block_json["light_emission"] = props.light_emission;
        block_json["light_absorption"] = props.light_absorption;
        block_json["hardness"] = props.hardness;
        block_json["resistance"] = props.resistance;
        block_json["friction"] = props.friction;
        block_json["restitution"] = props.restitution;
        block_json["tool_type"] = props.tool_type;
        block_json["tool_level"] = props.tool_level;
        block_json["break_time"] = props.break_time;
        block_json["drop_id"] = props.drop_id;
        block_json["drop_count_min"] = props.drop_count_min;
        block_json["drop_count_max"] = props.drop_count_max;
        block_json["drop_chance"] = props.drop_chance;
        block_json["texture_name"] = props.texture_name;
        block_json["model_name"] = props.model_name;
        
        j["blocks"].push_back(block_json);
    }
    
    std::ofstream file(filename);
    file << j.dump(2);
    
    VXL_INFO("Saved {} blocks to {}", m_blocks.size(), filename);
}

bool BlockRegistry::LoadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        VXL_ERROR("Failed to open block registry file: {}", filename);
        return false;
    }
    
    try {
        nlohmann::json j;
        file >> j;
        
        Clear();
        
        for (const auto& block_json : j["blocks"]) {
            BlockProperties props;
            props.id = block_json["id"];
            props.name = block_json["name"];
            props.is_solid = block_json.value("is_solid", true);
            props.is_transparent = block_json.value("is_transparent", false);
            props.is_fluid = block_json.value("is_fluid", false);
            props.is_breakable = block_json.value("is_breakable", true);
            props.is_placeable = block_json.value("is_placeable", true);
            props.is_opaque = block_json.value("is_opaque", true);
            props.cast_shadows = block_json.value("cast_shadows", true);
            props.receive_shadows = block_json.value("receive_shadows", true);
            props.light_emission = block_json.value("light_emission", 0);
            props.light_absorption = block_json.value("light_absorption", 15);
            props.hardness = block_json.value("hardness", 1.0f);
            props.resistance = block_json.value("resistance", 1.0f);
            props.friction = block_json.value("friction", 0.6f);
            props.restitution = block_json.value("restitution", 0.0f);
            props.tool_type = block_json.value("tool_type", 0);
            props.tool_level = block_json.value("tool_level", 0);
            props.break_time = block_json.value("break_time", 1.0f);
            props.drop_id = block_json.value("drop_id", props.id);
            props.drop_count_min = block_json.value("drop_count_min", 1);
            props.drop_count_max = block_json.value("drop_count_max", 1);
            props.drop_chance = block_json.value("drop_chance", 1.0f);
            props.texture_name = block_json.value("texture_name", "");
            props.model_name = block_json.value("model_name", "");
            
            RegisterBlock(props);
        }
        
        VXL_INFO("Loaded {} blocks from {}", m_blocks.size(), filename);
        return ValidateRegistry();
    }
    catch (const std::exception& e) {
        VXL_ERROR("Failed to parse block registry file {}: {}", filename, e.what());
        return false;
    }
}

void BlockRegistry::Clear() {
    m_blocks.clear();
    m_name_to_id.clear();
}

void BlockRegistry::RegisterDefaultBlocks() {
    // Air
    RegisterBlock({
        .id = static_cast<uint16_t>(BlockType::Air),
        .name = "air",
        .is_solid = false,
        .is_transparent = true,
        .is_fluid = false,
        .is_breakable = false,
        .is_placeable = false,
        .is_opaque = false,
        .cast_shadows = false,
        .receive_shadows = false,
        .light_emission = 0,
        .light_absorption = 0,
        .texture_name = "air"
    });
    
    // Stone
    RegisterBlock({
        .id = static_cast<uint16_t>(BlockType::Stone),
        .name = "stone",
        .is_solid = true,
        .is_transparent = false,
        .is_opaque = true,
        .hardness = 1.5f,
        .tool_type = 1, // pickaxe
        .break_time = 1.5f,
        .drop_id = static_cast<uint16_t>(BlockType::Cobblestone),
        .texture_name = "stone"
    });
    
    // Grass
    RegisterBlock({
        .id = static_cast<uint16_t>(BlockType::Grass),
        .name = "grass",
        .is_solid = true,
        .hardness = 0.6f,
        .tool_type = 3, // shovel
        .break_time = 0.6f,
        .drop_id = static_cast<uint16_t>(BlockType::Dirt),
        .texture_name = "grass"
    });
    
    // Dirt
    RegisterBlock({
        .id = static_cast<uint16_t>(BlockType::Dirt),
        .name = "dirt",
        .is_solid = true,
        .is_transparent = false,
        .is_opaque = true,
        .hardness = 0.5f,
        .tool_type = 3,
        .break_time = 0.5f,
        .texture_name = "dirt"
    });

    // Glass
    RegisterBlock({
        .id = static_cast<uint16_t>(BlockType::Glass),
        .name = "glass",
        .is_solid = true,
        .is_transparent = true,
        .is_opaque = false,
        .hardness = 0.3f,
        .break_time = 0.3f,
        .texture_name = "glass"
    });
    
    // Add more default blocks...
    // (Similar pattern for Dirt, Sand, Water, Wood, etc.)
    
    VXL_INFO("Registered {} default blocks", m_blocks.size());
}

// BlockUtils implementation
namespace BlockUtils {
    uint32_t EncodeBlockState(BlockType type, uint8_t metadata) {
        return (static_cast<uint32_t>(type) & 0xFFFF) | ((static_cast<uint32_t>(metadata) & 0xFF) << 16);
    }
    
    BlockType DecodeBlockType(uint32_t state) {
        return static_cast<BlockType>(state & 0xFFFF);
    }
    
    uint8_t DecodeMetadata(uint32_t state) {
        return static_cast<uint8_t>((state >> 16) & 0xFF);
    }
    
    BlockFace GetOppositeFace(BlockFace face) {
        switch (face) {
            case BlockFace::North: return BlockFace::South;
            case BlockFace::South: return BlockFace::North;
            case BlockFace::East: return BlockFace::West;
            case BlockFace::West: return BlockFace::East;
            case BlockFace::Up: return BlockFace::Down;
            case BlockFace::Down: return BlockFace::Up;
            default: return face;
        }
    }
    
    std::array<int32_t, 3> GetFaceNormal(BlockFace face) {
        switch (face) {
            case BlockFace::North: return {0, 0, -1};
            case BlockFace::South: return {0, 0, 1};
            case BlockFace::East: return {1, 0, 0};
            case BlockFace::West: return {-1, 0, 0};
            case BlockFace::Up: return {0, 1, 0};
            case BlockFace::Down: return {0, -1, 0};
            default: return {0, 0, 0};
        }
    }
    
    std::array<int32_t, 3> GetFaceOffset(BlockFace face) {
        return GetFaceNormal(face);
    }
    
    bool CanPlace(BlockType block, BlockType adjacent, BlockFace face) {
        const auto& block_props = BlockRegistry::Instance().GetProperties(block);
        const auto& adjacent_props = BlockRegistry::Instance().GetProperties(adjacent);
        
        if (!block_props.is_placeable) {
            return false;
        }
        
        // Can't place solid blocks in solid blocks
        if (block_props.is_solid && adjacent_props.is_solid) {
            return false;
        }
        
        // Special rules for fluids
        if (block_props.is_fluid && adjacent_props.is_fluid) {
            return false; // Can't place fluid in fluid
        }
        
        return true;
    }
    
    bool CanBreak(BlockType block, uint8_t tool_type, uint8_t tool_level) {
        const auto& props = BlockRegistry::Instance().GetProperties(block);
        
        if (!props.is_breakable) {
            return false;
        }
        
        // Check tool requirements
        if (props.tool_type > 0 && tool_type != props.tool_type) {
            return false; // Wrong tool type
        }
        
        if (tool_level < props.tool_level) {
            return false; // Tool level too low
        }
        
        return true;
    }
    
    float GetBreakTime(BlockType block, uint8_t tool_type, uint8_t tool_level) {
        const auto& props = BlockRegistry::Instance().GetProperties(block);
        
        if (!CanBreak(block, tool_type, tool_level)) {
            return -1.0f; // Cannot break
        }
        
        float base_time = props.break_time;
        
        // Tool efficiency multiplier
        if (tool_type == props.tool_type) {
            float efficiency = 1.0f + (tool_level * 0.5f);
            base_time /= efficiency;
        }
        
        return base_time;
    }
    
    uint8_t GetLightLevel(BlockType block) {
        return BlockRegistry::Instance().GetProperties(block).light_emission;
    }
    
    uint8_t GetLightAbsorption(BlockType block) {
        return BlockRegistry::Instance().GetProperties(block).light_absorption;
    }
    
    bool BlocksLight(BlockType block) {
        return GetLightAbsorption(block) > 0;
    }
    
    bool HasCollision(BlockType block) {
        return BlockRegistry::Instance().GetProperties(block).is_solid;
    }
    
    float GetFriction(BlockType block) {
        return BlockRegistry::Instance().GetProperties(block).friction;
    }
    
    float GetRestitution(BlockType block) {
        return BlockRegistry::Instance().GetProperties(block).restitution;
    }
}

} // namespace voxelvk