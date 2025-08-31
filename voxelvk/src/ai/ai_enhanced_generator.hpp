#pragma once
#include "ai_object_generator.hpp"
#include "ai_biome_palette.hpp"
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace voxelvk::ai {

// Enhanced content types for AI generation
enum class ContentType { 
    Structure,      // Buildings, dungeons, bridges
    Terrain,        // Biome enhancement, terrain features
    Texture,        // Material generation, texture synthesis
    Biome,          // Dynamic biome rules and generation
    Settlement      // Cities, villages, complex structures
};

enum class ArchitecturalStyle {
    Medieval, Modern, Fantasy, SciFi, Rustic, Industrial, Organic, Crystalline
};

enum class DifficultyLevel {
    Beginner, Easy, Medium, Hard, Expert, Master
};

// Enhanced configuration extending existing AiGenConfig
struct EnhancedAiGenConfig : public AiGenConfig {
    // Multiple TensorRT engines for different content types
    std::string structure_trt_path = "models/structure_generator.trt";
    std::string terrain_trt_path = "models/terrain_enhancer.trt";
    std::string texture_trt_path = "models/texture_generator.trt";
    std::string biome_trt_path = "models/biome_generator.trt";
    std::string settlement_trt_path = "models/settlement_generator.trt";
    
    // Enhanced parameters
    float structure_complexity = 0.5f;     // 0.0 = simple, 1.0 = complex
    int texture_resolution = 256;          // Generated texture resolution
    bool enable_runtime_generation = true; // Allow runtime content generation
    
    // Performance tuning
    int max_concurrent_generations = 4;    // Parallel generation limit
    float memory_pool_size_gb = 2.0f;     // GPU memory pool size
    bool use_fp16 = true;                  // Use half precision for performance
    
    // Quality settings
    float detail_level = 0.7f;            // Detail vs performance trade-off
    bool enable_post_processing = true;    // Apply post-processing filters
    int max_structure_size = 128;         // Maximum structure dimension
};

    // Retrieval-Augmented Generation (RAG) options
    bool enable_rag = false;                  // Enable RAG prompt augmentation
    std::string rag_index_path = "data/rag_index.faiss";  // FAISS index path (optional)
    std::string rag_docs_path = "data/rag_documents.txt"; // Documents store (required)
    std::string rag_embedding_onnx_path = "";             // Optional ONNX embedding model
    int rag_top_k = 3;                       // Number of docs to retrieve


// Structure generation components
struct StructurePrompt {
    std::string description;               // "medieval castle with towers"
    glm::vec3 position = {0, 0, 0};       // World position
    glm::vec3 size_bounds = {32, 32, 32}; // Maximum dimensions
    ArchitecturalStyle style = ArchitecturalStyle::Medieval;
    float complexity = 0.5f;              // Structure complexity
    
    // Contextual parameters
    std::string biome_context = "plains"; // Biome for style adaptation
    bool require_foundation = true;       // Generate foundation/basement
    bool allow_multi_story = true;       // Allow multiple floors
    std::vector<std::string> required_features; // "door", "windows", "roof"
};

struct FunctionalZone {
    std::string type;                     // "bedroom", "kitchen", "corridor"
    VoxelGrid zone_blocks;               // Blocks defining this zone
    glm::vec3 center;                    // Zone center point
    std::vector<glm::vec3> entry_points; // Access points
};

struct ConnectivityGraph {
    struct Connection {
        int zone_a, zone_b;
        std::vector<glm::vec3> path;     // Connecting path
        std::string connection_type;     // "door", "corridor", "stairs"
    };
    std::vector<Connection> connections;
};

struct MaterialMapping {
    std::map<uint16_t, std::string> block_to_material; // Block ID -> Material name
    std::map<std::string, std::vector<uint16_t>> material_palette; // Material -> Block variants
};

struct StructureBlueprint {
    VoxelGrid structure;                          // Main structure voxels
    std::vector<glm::vec3> anchor_points;        // Key structural points
    MaterialMapping material_map;                // Material assignments
    ConnectivityGraph connections;               // Internal connectivity
    std::vector<FunctionalZone> zones;          // Functional areas
    
    // Metadata
    std::string structure_type;                  // "building", "dungeon", "bridge"
    ArchitecturalStyle style;
    float estimated_complexity;
    glm::vec3 bounding_box;                     // Actual size
    
    // Integration data
    std::vector<glm::vec3> integration_points;  // Points for world integration
    bool requires_terrain_modification = false; // Needs terrain changes
};

// Biome enhancement
struct BiomeRule {
    std::string name;
    float temperature_range[2] = {0.0f, 1.0f};
    float humidity_range[2] = {0.0f, 1.0f};
    float elevation_range[2] = {0.0f, 1.0f};
    
    // Block generation rules
    std::vector<uint16_t> surface_blocks;       // Possible surface blocks
    std::vector<uint16_t> subsurface_blocks;    // Subsurface layers
    std::vector<uint16_t> vegetation_blocks;    // Vegetation types
    
    // Structure generation rules
    float structure_density = 0.1f;            // Structures per chunk
    std::vector<std::string> preferred_structures; // Structure types for this biome
    
    // Special features
    bool generate_caves = true;
    bool generate_water_features = false;
    bool generate_ore_veins = true;
    std::map<std::string, float> special_features; // Custom biome features
};

struct BiomeContext {
    glm::vec2 world_position;
    float temperature, humidity, elevation;
    std::vector<std::string> adjacent_biomes;
    std::string desired_theme;                  // "mystical", "harsh", "lush"
};

// Texture generation
struct MaterialRequest {
    std::string material_type;                  // "stone", "wood", "metal"
    std::string style_description;             // "weathered medieval stone"
    int resolution = 256;                      // Texture resolution
    bool generate_normal_map = true;           // Generate normal maps
    bool generate_roughness_map = true;        // Generate roughness maps
    bool seamless = true;                      // Ensure texture tiles seamlessly
};

struct TextureData {
    std::vector<uint8_t> diffuse_map;          // RGB diffuse texture
    std::vector<uint8_t> normal_map;           // Normal map data
    std::vector<uint8_t> roughness_map;        // Roughness/metallic data
    std::vector<uint8_t> ao_map;               // Ambient occlusion
    
    int width, height;                         // Texture dimensions
    std::string material_name;                 // Associated material
    
    // Metadata
    float tiling_factor = 1.0f;               // Recommended tiling
    std::vector<std::string> compatible_materials; // Compatible material types
};

struct MaterialPalette {
    std::map<std::string, TextureData> textures;
    std::map<uint16_t, std::string> block_material_mapping;
    std::vector<std::string> style_tags;       // "medieval", "modern", etc.
};

// Enhanced outputs combining all generation types
struct MultiModalAiOutputs : public AiOutputs {
    // Structure generation results
    std::vector<StructureBlueprint> structures;
    
    // Enhanced biome data
    std::vector<BiomeRule> dynamic_biome_rules;
    std::map<std::string, float> biome_parameters;
    
    // Texture generation results
    std::vector<TextureData> generated_textures;
    MaterialPalette enhanced_materials;
    
    // Settlement data
    std::vector<StructureBlueprint> settlement_structures;
    std::vector<glm::vec2> road_network;      // Settlement roads/paths
    
    // Quality metrics
    float generation_quality = 0.0f;         // Overall quality score
    float performance_score = 0.0f;          // Performance metric
    std::vector<std::string> warnings;       // Generation warnings
};

// Environmental context for generation
struct EnvironmentalContext {
    glm::vec3 world_position;
    std::string current_biome;
    float temperature, humidity, elevation;
    std::vector<std::string> nearby_biomes;
    std::vector<StructureBlueprint> nearby_structures;
    
    // Player/agent context
    bool player_nearby = false;
    std::vector<std::string> player_objectives; // Current player goals
    
    // World state
    int time_of_day = 12;                     // 0-23 hour
    std::string weather = "clear";            // Weather conditions
    float world_age = 0.0f;                  // How long world has existed
};

// Training-specific structures
struct TrainingObjective {
    std::string name;                         // "collect_wood", "build_shelter"
    std::string description;                  // Human-readable description
    std::vector<std::string> required_structures; // Structures needed
    std::vector<std::string> required_materials;  // Materials needed
    float difficulty_rating = 0.5f;          // 0.0-1.0 difficulty
    float expected_completion_time = 300.0f;  // Expected seconds to complete
};

struct CurriculumStage {
    std::string stage_name;
    DifficultyLevel difficulty;
    std::vector<TrainingObjective> objectives;
    EnhancedAiGenConfig generation_config;
    
    // Environmental parameters
    glm::vec3 world_size = {128, 128, 128};
    std::vector<std::string> allowed_biomes;
    std::vector<std::string> allowed_structures;
    
    // Progression criteria
    float success_rate_threshold = 0.8f;
    int minimum_episodes = 100;
    std::vector<std::string> advancement_conditions;
};

struct TrainingScenario {
    std::string name;
    std::string description;
    DifficultyLevel difficulty;
    
    // Generated content for this scenario
    std::vector<StructureBlueprint> structures;
    BiomeRule custom_biome_config;
    std::vector<TrainingObjective> objectives;
    
    // World configuration
    glm::vec3 spawn_point;
    glm::vec3 world_bounds;
    std::vector<glm::vec3> key_locations; // Important locations for objectives
    
    // Evaluation metrics
    std::vector<std::string> success_metrics;
    float target_completion_rate = 0.7f;
    float estimated_difficulty = 0.5f;
};

} // namespace voxelvk::ai