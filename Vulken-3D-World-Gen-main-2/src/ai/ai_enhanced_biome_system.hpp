#pragma once
#include "ai_enhanced_generator.hpp"
#include "ai_biome_palette.hpp"
#include "ai_tensorrt_manager.hpp"
#include <array>
#include <unordered_map>

namespace voxelvk::ai {

// Extended biome enumeration with AI-enhanced biomes
enum class ExtendedBiome : int32_t {
    // Original biomes
    Plains = 0, Desert = 1, Taiga = 2, Snow = 3, Swamp = 4, Mountain = 5, Custom0 = 6, Custom1 = 7,
    
    // AI-Enhanced biomes (starting from 8)
    AIForest = 8,       // AI-generated dense forests with complex tree structures
    AIMesa = 9,         // AI-generated mesa formations with layered stone
    AITundra = 10,      // AI-enhanced tundra with unique ice formations
    AIJungle = 11,      // AI-generated tropical jungle with diverse vegetation
    AIVolcanic = 12,    // AI-generated volcanic regions with lava features
    AIFloating = 13,    // AI-generated floating islands and sky formations
    AICrystal = 14,     // AI-generated crystal caves and formations
    AIAlien = 15,       // AI-generated alien-like biomes with unique blocks
    AIUnderwater = 16,  // AI-enhanced underwater biomes with coral structures
    AISky = 17,         // AI-generated sky biomes with cloud formations
    
    // Dynamic biomes (generated at runtime)
    AIDynamic0 = 18,    // Runtime generated biome slot 0
    AIDynamic1 = 19,    // Runtime generated biome slot 1
    AIDynamic2 = 20,    // Runtime generated biome slot 2
    AIDynamic3 = 21     // Runtime generated biome slot 3
};

// Enhanced palette configuration supporting AI-generated content
struct EnhancedPaletteConfig : public PaletteConfig {
    // Extended biome surface mapping (22 biomes instead of 8)
    std::array<uint16_t, 22> extended_biome_surface_map;
    
    // AI-generated material templates
    std::unordered_map<std::string, MaterialTemplate> ai_materials;
    
    // Dynamic generation parameters
    float structure_density = 0.1f;        // Structures per chunk (0.0-1.0)
    float terrain_variation = 1.0f;        // Terrain height variation multiplier
    float biome_transition_smoothness = 0.5f; // How smoothly biomes transition
    
    // AI generation toggles
    bool enable_ai_structures = true;      // Generate AI structures in biomes
    bool enable_ai_textures = true;        // Use AI-generated textures
    bool enable_dynamic_biomes = true;     // Allow runtime biome generation
    bool enable_biome_evolution = false;   // Biomes change over time
    
    // Performance parameters
    int max_ai_structures_per_chunk = 3;   // Limit AI structures for performance
    float ai_generation_quality = 0.7f;    // Quality vs speed trade-off (0.0-1.0)
    bool cache_ai_generations = true;      // Cache generated content
    
    // Biome-specific rules
    std::unordered_map<ExtendedBiome, BiomeRule> biome_rules;
    
    // Environmental parameters
    struct WeatherSystem {
        bool enable_dynamic_weather = false;
        float weather_change_frequency = 0.1f; // Changes per game hour
        std::vector<std::string> weather_types = {"clear", "rain", "storm", "fog"};
    } weather_system;
    
    // Seasonal changes
    struct SeasonalSystem {
        bool enable_seasons = false;
        float season_length_days = 30.0f;
        bool affect_biome_appearance = true;
        bool affect_structure_generation = false;
    } seasonal_system;
    
    // Constructor with defaults
    EnhancedPaletteConfig() : PaletteConfig() {
        // Initialize extended biome surface map with defaults
        for (size_t i = 0; i < 8; ++i) {
            extended_biome_surface_map[i] = biome_surface_map[i];
        }
        
        // Set defaults for AI-enhanced biomes
        extended_biome_surface_map[8] = ids.Grass;   // AIForest
        extended_biome_surface_map[9] = ids.Stone;   // AIMesa
        extended_biome_surface_map[10] = ids.Snow;   // AITundra
        extended_biome_surface_map[11] = ids.Grass;  // AIJungle
        extended_biome_surface_map[12] = ids.Stone;  // AIVolcanic
        extended_biome_surface_map[13] = ids.Stone;  // AIFloating
        extended_biome_surface_map[14] = ids.Stone;  // AICrystal
        extended_biome_surface_map[15] = ids.Stone;  // AIAlien
        extended_biome_surface_map[16] = ids.Sand;   // AIUnderwater
        extended_biome_surface_map[17] = ids.Snow;   // AISky
        extended_biome_surface_map[18] = ids.Grass;  // AIDynamic0
        extended_biome_surface_map[19] = ids.Grass;  // AIDynamic1
        extended_biome_surface_map[20] = ids.Grass;  // AIDynamic2
        extended_biome_surface_map[21] = ids.Grass;  // AIDynamic3
    }
};

// Material template for AI-generated materials
struct MaterialTemplate {
    std::string base_material;            // "stone", "wood", "metal", etc.
    std::vector<std::string> style_tags;  // "weathered", "polished", "ancient"
    
    // Visual properties
    glm::vec3 base_color = {0.5f, 0.5f, 0.5f};
    float roughness = 0.5f;
    float metallic = 0.0f;
    float emission = 0.0f;
    
    // Generation parameters
    bool requires_normal_map = true;
    bool requires_roughness_map = true;
    bool requires_ao_map = false;
    int preferred_resolution = 256;
    
    // Gameplay properties
    float hardness = 1.0f;                // How hard to break
    float blast_resistance = 1.0f;        // Explosion resistance
    bool is_transparent = false;
    bool emits_light = false;
    int light_level = 0;                  // 0-15
    
    // Environmental properties
    bool affected_by_weather = true;
    bool changes_with_seasons = false;
    std::vector<std::string> biome_variants; // Different looks per biome
};

// AI-enhanced biome generator
class AIBiomeEnhancer {
public:
    AIBiomeEnhancer();
    ~AIBiomeEnhancer();
    
    // Initialization
    bool initialize(TensorRTMultiModelManager* trt_manager, 
                   const EnhancedPaletteConfig& config);
    
    // Biome generation
    BiomeRule generateBiomeRule(const std::string& description, 
                               const EnvironmentalContext& context);
    
    // Enhanced biome building (extends existing functionality)
    VoxelVolume buildEnhancedBiome(const AiOutputs& base_output, 
                                  const EnhancedPaletteConfig& config,
                                  const EnvironmentalContext& context = {});
    
    // Runtime biome modification
    bool enhanceBiomeAtLocation(VoxelVolume& volume, 
                               const glm::vec3& world_pos,
                               const std::string& enhancement_description);
    
    // Dynamic biome creation
    ExtendedBiome createDynamicBiome(const BiomeContext& context);
    bool updateDynamicBiome(ExtendedBiome biome_id, const BiomeRule& new_rule);
    
    // Biome transition enhancement
    void enhanceBiomeTransitions(VoxelVolume& volume,
                                const std::vector<ExtendedBiome>& adjacent_biomes,
                                const glm::vec3& chunk_position);
    
    // Material generation
    MaterialTemplate generateMaterialTemplate(const MaterialRequest& request);
    bool applyMaterialToBlocks(VoxelVolume& volume, 
                              const MaterialTemplate& material,
                              const std::vector<uint16_t>& target_blocks);
    
    // Performance optimization
    void setQualityLevel(float quality);    // 0.0 = fast, 1.0 = high quality
    void enableCaching(bool enable);
    void clearCache();
    
    // Configuration management
    bool updateConfig(const EnhancedPaletteConfig& config);
    const EnhancedPaletteConfig& getConfig() const { return config_; }
    
    // Biome analysis
    struct BiomeAnalysis {
        std::vector<ExtendedBiome> detected_biomes;
        std::vector<float> biome_coverage_percentages;
        float terrain_complexity = 0.0f;
        float structure_density = 0.0f;
        std::vector<std::string> recommended_enhancements;
    };
    
    BiomeAnalysis analyzeBiomeComposition(const VoxelVolume& volume);
    
    // Validation
    bool validateBiomeRule(const BiomeRule& rule);
    bool validateMaterialTemplate(const MaterialTemplate& template_def);
    
private:
    TensorRTMultiModelManager* trt_manager_ = nullptr;
    EnhancedPaletteConfig config_;
    
    // Cache for generated content
    std::unordered_map<std::string, BiomeRule> biome_rule_cache_;
    std::unordered_map<std::string, MaterialTemplate> material_cache_;
    
    // Dynamic biome storage
    std::unordered_map<ExtendedBiome, BiomeRule> dynamic_biomes_;
    
    // Performance settings
    float quality_level_ = 0.7f;
    bool caching_enabled_ = true;
    
    // Internal utilities
    uint16_t getBiomeSurfaceBlock(ExtendedBiome biome, const EnhancedPaletteConfig& config);
    bool isAIBiome(ExtendedBiome biome);
    bool isDynamicBiome(ExtendedBiome biome);
    
    // Generation helpers
    bool generateAIBiomeStructures(VoxelVolume& volume, 
                                  const BiomeRule& rule,
                                  const EnvironmentalContext& context);
    void applyBiomeSpecificFeatures(VoxelVolume& volume, 
                                   ExtendedBiome biome,
                                   const EnvironmentalContext& context);
    
    // Transition algorithms
    void applySmoothTransition(VoxelVolume& volume,
                             const glm::vec3& position,
                             ExtendedBiome from_biome,
                             ExtendedBiome to_biome,
                             float transition_factor);
};

// Structure generation specialized for biomes
class AIStructureGenerator {
public:
    AIStructureGenerator();
    ~AIStructureGenerator();
    
    bool initialize(TensorRTMultiModelManager* trt_manager);
    
    // Structure generation
    StructureBlueprint generateBuilding(const StructurePrompt& prompt);
    StructureBlueprint generateDungeon(const DungeonParameters& params);
    StructureBlueprint generateBridge(const BridgeRequest& request);
    
    // Settlement generation
    std::vector<StructureBlueprint> generateSettlement(const SettlementPlan& plan);
    std::vector<StructureBlueprint> generateVillage(const VillageParameters& params);
    std::vector<StructureBlueprint> generateCity(const CityParameters& params);
    
    // Biome-specific structures
    std::vector<StructureBlueprint> generateBiomeStructures(ExtendedBiome biome,
                                                           const EnvironmentalContext& context,
                                                           int max_structures = 5);
    
    // Integration with world
    bool integrateStructureIntoWorld(VoxelVolume& volume,
                                   const StructureBlueprint& structure,
                                   const glm::vec3& position);
    
    // Structure validation
    bool validateStructurePlacement(const VoxelVolume& volume,
                                  const StructureBlueprint& structure,
                                  const glm::vec3& position);
    
private:
    TensorRTMultiModelManager* trt_manager_ = nullptr;
    
    // Structure type parameters
    struct DungeonParameters {
        int num_rooms = 5;
        int num_floors = 1;
        float complexity = 0.5f;
        std::string theme = "medieval";
        bool include_treasure = true;
        bool include_monsters = false;
    };
    
    struct BridgeRequest {
        glm::vec3 start_point;
        glm::vec3 end_point;
        std::string bridge_type = "stone";
        bool include_supports = true;
        float max_span = 64.0f;
    };
    
    struct SettlementPlan {
        glm::vec2 center_position;
        float radius = 100.0f;
        int population_estimate = 50;
        std::string culture = "medieval";
        std::vector<std::string> required_buildings;
    };
    
    struct VillageParameters {
        int num_houses = 10;
        bool include_market = true;
        bool include_walls = false;
        std::string architectural_style = "medieval";
    };
    
    struct CityParameters {
        int num_districts = 4;
        bool include_walls = true;
        bool include_castle = true;
        bool include_roads = true;
        std::string architectural_style = "medieval";
        float urban_density = 0.7f;
    };
    
    // Helper methods
    StructureBlueprint generateHouse(const glm::vec3& position, 
                                   const std::string& style);
    StructureBlueprint generateTower(const glm::vec3& position,
                                   int height, const std::string& purpose);
    std::vector<glm::vec2> generateRoadNetwork(const std::vector<glm::vec3>& key_points);
};

// Utility functions for biome enhancement
namespace biome_utils {
    // Biome detection and analysis
    ExtendedBiome detectBiomeAt(const VoxelVolume& volume, const glm::vec3& position);
    std::vector<ExtendedBiome> detectBiomesInVolume(const VoxelVolume& volume);
    
    // Biome transition calculations
    float calculateBiomeTransition(ExtendedBiome from, ExtendedBiome to, float distance);
    glm::vec3 interpolateBiomeProperties(ExtendedBiome from, ExtendedBiome to, float factor);
    
    // Environmental context generation
    EnvironmentalContext createContextFromVolume(const VoxelVolume& volume, 
                                               const glm::vec3& position);
    
    // Biome compatibility
    bool areBiomesCompatible(ExtendedBiome a, ExtendedBiome b);
    float getBiomeSimilarity(ExtendedBiome a, ExtendedBiome b);
    
    // Material utilities
    std::vector<uint16_t> getRecommendedBlocks(ExtendedBiome biome, 
                                             const EnhancedPaletteConfig& config);
    MaterialTemplate getDefaultMaterialTemplate(const std::string& block_type);
}

} // namespace voxelvk::ai