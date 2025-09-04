#include "ai_enhanced_biome_system.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <iostream>

namespace voxelvk::ai {

// AIBiomeEnhancer Implementation
AIBiomeEnhancer::AIBiomeEnhancer() {
    // Initialize with default quality settings
    quality_level_ = 0.7f;
    caching_enabled_ = true;
}

AIBiomeEnhancer::~AIBiomeEnhancer() = default;

bool AIBiomeEnhancer::initialize(TensorRTMultiModelManager* trt_manager, 
                                const EnhancedPaletteConfig& config) {
    trt_manager_ = trt_manager;
    config_ = config;
    
    if (!trt_manager_) {
        std::cerr << "AIBiomeEnhancer: TensorRT manager is null" << std::endl;
        return false;
    }
    
    // Initialize default biome rules
    for (int i = 0; i < 22; ++i) {
        ExtendedBiome biome = static_cast<ExtendedBiome>(i);
        BiomeRule rule;
        rule.name = "Biome_" + std::to_string(i);
        
        // Set default parameters based on biome type
        switch (biome) {
            case ExtendedBiome::Plains:
                rule.temperature_range[0] = 0.6f; rule.temperature_range[1] = 0.9f;
                rule.humidity_range[0] = 0.3f; rule.humidity_range[1] = 0.7f;
                rule.surface_blocks = {config_.ids.Grass};
                rule.subsurface_blocks = {config_.ids.Dirt, config_.ids.Stone};
                break;
                
            case ExtendedBiome::Desert:
                rule.temperature_range[0] = 0.8f; rule.temperature_range[1] = 1.0f;
                rule.humidity_range[0] = 0.0f; rule.humidity_range[1] = 0.2f;
                rule.surface_blocks = {config_.ids.Sand};
                rule.subsurface_blocks = {config_.ids.Sand, config_.ids.Stone};
                break;
                
            case ExtendedBiome::AIForest:
                rule.temperature_range[0] = 0.5f; rule.temperature_range[1] = 0.8f;
                rule.humidity_range[0] = 0.6f; rule.humidity_range[1] = 1.0f;
                rule.surface_blocks = {config_.ids.Grass};
                rule.subsurface_blocks = {config_.ids.Dirt};
                rule.vegetation_blocks = {config_.ids.Wood, config_.ids.Leaves};
                rule.structure_density = 0.2f;
                rule.preferred_structures = {"treehouse", "forest_shrine"};
                break;
                
            case ExtendedBiome::AIVolcanic:
                rule.temperature_range[0] = 0.9f; rule.temperature_range[1] = 1.0f;
                rule.humidity_range[0] = 0.1f; rule.humidity_range[1] = 0.4f;
                rule.surface_blocks = {config_.ids.Stone};
                rule.subsurface_blocks = {config_.ids.Stone};
                rule.generate_caves = false;
                rule.special_features["lava_flows"] = 0.3f;
                rule.special_features["geysers"] = 0.1f;
                break;
                
            case ExtendedBiome::AIFloating:
                rule.temperature_range[0] = 0.4f; rule.temperature_range[1] = 0.7f;
                rule.humidity_range[0] = 0.5f; rule.humidity_range[1] = 0.8f;
                rule.surface_blocks = {config_.ids.Stone};
                rule.elevation_range[0] = 0.8f; rule.elevation_range[1] = 1.0f;
                rule.special_features["floating_platforms"] = 0.5f;
                rule.preferred_structures = {"sky_temple", "floating_platform"};
                break;
                
            default:
                // Default values for other biomes
                rule.temperature_range[0] = 0.4f; rule.temperature_range[1] = 0.8f;
                rule.humidity_range[0] = 0.3f; rule.humidity_range[1] = 0.7f;
                rule.surface_blocks = {config_.ids.Grass};
                rule.subsurface_blocks = {config_.ids.Dirt, config_.ids.Stone};
                break;
        }
        
        config_.biome_rules[biome] = rule;
    }
    
    std::cout << "AIBiomeEnhancer initialized with " << config_.biome_rules.size() << " biome rules" << std::endl;
    return true;
}

BiomeRule AIBiomeEnhancer::generateBiomeRule(const std::string& description, 
                                           const EnvironmentalContext& context) {
    // Check cache first
    std::string cache_key = description + "_" + context.current_biome;
    if (caching_enabled_) {
        auto it = biome_rule_cache_.find(cache_key);
        if (it != biome_rule_cache_.end()) {
            return it->second;
        }
    }
    
    BiomeRule rule;
    
    if (trt_manager_ && trt_manager_->isModelLoaded(ContentType::Biome)) {
        // Use AI generation
        BiomeContext biome_context;
        biome_context.world_position = {context.world_position.x, context.world_position.z};
        biome_context.temperature = context.temperature;
        biome_context.humidity = context.humidity;
        biome_context.elevation = context.elevation;
        biome_context.desired_theme = description;
        
        // Extract adjacent biomes
        for (const auto& biome_name : context.nearby_biomes) {
            biome_context.adjacent_biomes.push_back(biome_name);
        }
        
        rule = trt_manager_->generateBiomeRule(biome_context);
        
        // Enhance with description-based modifications
        if (description.find("hot") != std::string::npos) {
            rule.temperature_range[0] = std::max(0.7f, rule.temperature_range[0]);
            rule.temperature_range[1] = std::min(1.0f, rule.temperature_range[1] + 0.2f);
        }
        
        if (description.find("cold") != std::string::npos || description.find("frozen") != std::string::npos) {
            rule.temperature_range[0] = std::max(0.0f, rule.temperature_range[0] - 0.3f);
            rule.temperature_range[1] = std::min(0.4f, rule.temperature_range[1]);
            rule.surface_blocks = {config_.ids.Snow};
        }
        
        if (description.find("wet") != std::string::npos || description.find("swamp") != std::string::npos) {
            rule.humidity_range[0] = std::max(0.6f, rule.humidity_range[0]);
            rule.generate_water_features = true;
        }
        
        if (description.find("dry") != std::string::npos || description.find("arid") != std::string::npos) {
            rule.humidity_range[1] = std::min(0.3f, rule.humidity_range[1]);
            rule.surface_blocks = {config_.ids.Sand, config_.ids.Stone};
        }
        
    } else {
        // Fallback to procedural generation
        rule = generateProceduralBiomeRule(description, context);
    }
    
    // Validate and cache the result
    if (validateBiomeRule(rule)) {
        if (caching_enabled_) {
            biome_rule_cache_[cache_key] = rule;
        }
    }
    
    return rule;
}

BiomeRule AIBiomeEnhancer::generateProceduralBiomeRule(const std::string& description, 
                                                     const EnvironmentalContext& context) {
    BiomeRule rule;
    rule.name = "Procedural_" + description;
    
    // Use simple heuristics based on description keywords
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    // Set temperature based on context and description
    float base_temp = context.temperature;
    if (description.find("hot") != std::string::npos || description.find("volcanic") != std::string::npos) {
        base_temp = std::max(0.8f, base_temp);
    } else if (description.find("cold") != std::string::npos || description.find("frozen") != std::string::npos) {
        base_temp = std::min(0.3f, base_temp);
    }
    
    rule.temperature_range[0] = std::max(0.0f, base_temp - 0.15f);
    rule.temperature_range[1] = std::min(1.0f, base_temp + 0.15f);
    
    // Set humidity
    float base_humidity = context.humidity;
    if (description.find("wet") != std::string::npos || description.find("jungle") != std::string::npos) {
        base_humidity = std::max(0.7f, base_humidity);
    } else if (description.find("dry") != std::string::npos || description.find("desert") != std::string::npos) {
        base_humidity = std::min(0.2f, base_humidity);
    }
    
    rule.humidity_range[0] = std::max(0.0f, base_humidity - 0.2f);
    rule.humidity_range[1] = std::min(1.0f, base_humidity + 0.2f);
    
    // Set elevation preferences
    if (description.find("mountain") != std::string::npos || description.find("high") != std::string::npos) {
        rule.elevation_range[0] = 0.6f;
        rule.elevation_range[1] = 1.0f;
    } else if (description.find("valley") != std::string::npos || description.find("low") != std::string::npos) {
        rule.elevation_range[0] = 0.0f;
        rule.elevation_range[1] = 0.4f;
    } else {
        rule.elevation_range[0] = 0.2f;
        rule.elevation_range[1] = 0.8f;
    }
    
    // Set surface blocks based on temperature and humidity
    if (rule.temperature_range[1] < 0.3f) {
        rule.surface_blocks = {config_.ids.Snow};
        rule.subsurface_blocks = {config_.ids.Dirt, config_.ids.Stone};
    } else if (rule.humidity_range[1] < 0.3f) {
        rule.surface_blocks = {config_.ids.Sand};
        rule.subsurface_blocks = {config_.ids.Sand, config_.ids.Stone};
    } else {
        rule.surface_blocks = {config_.ids.Grass};
        rule.subsurface_blocks = {config_.ids.Dirt, config_.ids.Stone};
        
        if (rule.humidity_range[0] > 0.6f) {
            rule.vegetation_blocks = {config_.ids.Wood, config_.ids.Leaves};
        }
    }
    
    // Set structure preferences
    if (description.find("ruins") != std::string::npos || description.find("ancient") != std::string::npos) {
        rule.preferred_structures = {"ancient_ruins", "stone_temple"};
        rule.structure_density = 0.1f;
    } else if (description.find("village") != std::string::npos || description.find("settlement") != std::string::npos) {
        rule.preferred_structures = {"house", "well", "farm"};
        rule.structure_density = 0.2f;
    } else if (description.find("wild") != std::string::npos || description.find("untamed") != std::string::npos) {
        rule.structure_density = 0.05f;
    } else {
        rule.structure_density = config_.structure_density;
    }
    
    // Set special features
    if (description.find("crystal") != std::string::npos) {
        rule.special_features["crystal_formations"] = 0.3f;
        rule.generate_ore_veins = true;
    }
    
    if (description.find("water") != std::string::npos || description.find("lake") != std::string::npos) {
        rule.generate_water_features = true;
        rule.special_features["lakes"] = 0.2f;
    }
    
    if (description.find("cave") != std::string::npos || description.find("underground") != std::string::npos) {
        rule.generate_caves = true;
        rule.special_features["cave_systems"] = 0.4f;
    }
    
    return rule;
}

VoxelVolume AIBiomeEnhancer::buildEnhancedBiome(const AiOutputs& base_output, 
                                              const EnhancedPaletteConfig& config,
                                              const EnvironmentalContext& context) {
    
    // Start with the original biome building logic
    VoxelVolume volume = BuildVoxelVolumeWithPalette(base_output, config);
    
    if (!config.enable_ai_structures) {
        return volume;
    }
    
    // Detect biomes in the volume
    std::vector<ExtendedBiome> detected_biomes = biome_utils::detectBiomesInVolume(volume);
    
    for (ExtendedBiome biome : detected_biomes) {
        if (isAIBiome(biome)) {
            // Apply AI-specific enhancements
            applyBiomeSpecificFeatures(volume, biome, context);
            
            // Generate AI structures if enabled
            auto biome_rule_it = config.biome_rules.find(biome);
            if (biome_rule_it != config.biome_rules.end()) {
                generateAIBiomeStructures(volume, biome_rule_it->second, context);
            }
        }
    }
    
    // Apply biome transitions
    if (detected_biomes.size() > 1) {
        enhanceBiomeTransitions(volume, detected_biomes, context.world_position);
    }
    
    return volume;
}

bool AIBiomeEnhancer::enhanceBiomeAtLocation(VoxelVolume& volume, 
                                           const glm::vec3& world_pos,
                                           const std::string& enhancement_description) {
    
    EnvironmentalContext context = biome_utils::createContextFromVolume(volume, world_pos);
    BiomeRule enhancement_rule = generateBiomeRule(enhancement_description, context);
    
    if (!validateBiomeRule(enhancement_rule)) {
        std::cerr << "Generated biome rule is invalid for enhancement: " << enhancement_description << std::endl;
        return false;
    }
    
    // Apply the enhancement to a local area around the position
    int radius = 16; // Enhancement radius
    glm::vec3 local_pos = world_pos; // Convert to local coordinates if needed
    
    int start_x = std::max(0, (int)local_pos.x - radius);
    int end_x = std::min(volume.sizeX, (int)local_pos.x + radius);
    int start_z = std::max(0, (int)local_pos.z - radius);
    int end_z = std::min(volume.sizeZ, (int)local_pos.z + radius);
    
    for (int z = start_z; z < end_z; ++z) {
        for (int x = start_x; x < end_x; ++x) {
            float distance = std::sqrt((x - local_pos.x) * (x - local_pos.x) + 
                                     (z - local_pos.z) * (z - local_pos.z));
            
            if (distance <= radius) {
                // Apply enhancement with falloff
                float strength = 1.0f - (distance / radius);
                applyBiomeEnhancementAtPosition(volume, x, z, enhancement_rule, strength);
            }
        }
    }
    
    return true;
}

void AIBiomeEnhancer::applyBiomeEnhancementAtPosition(VoxelVolume& volume, int x, int z, 
                                                    const BiomeRule& rule, float strength) {
    
    // Find surface height at this position
    int surface_y = 0;
    for (int y = volume.sizeY - 1; y >= 0; --y) {
        size_t idx = (size_t)z * volume.sizeY * volume.sizeX + (size_t)y * volume.sizeX + x;
        if (idx < volume.blocks.size() && volume.blocks[idx] != config_.ids.Air) {
            surface_y = y;
            break;
        }
    }
    
    // Apply surface block changes
    if (!rule.surface_blocks.empty() && strength > 0.5f) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, rule.surface_blocks.size() - 1);
        
        size_t surface_idx = (size_t)z * volume.sizeY * volume.sizeX + 
                            (size_t)surface_y * volume.sizeX + x;
        if (surface_idx < volume.blocks.size()) {
            volume.blocks[surface_idx] = rule.surface_blocks[dis(gen)];
        }
    }
    
    // Add vegetation if specified
    if (!rule.vegetation_blocks.empty() && strength > 0.3f) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> prob_dis(0.0, 1.0);
        
        if (prob_dis(gen) < 0.1f * strength) { // 10% chance scaled by strength
            int veg_height = 1 + (gen() % 3); // 1-3 blocks tall
            for (int h = 1; h <= veg_height && surface_y + h < volume.sizeY; ++h) {
                size_t idx = (size_t)z * volume.sizeY * volume.sizeX + 
                            (size_t)(surface_y + h) * volume.sizeX + x;
                if (idx < volume.blocks.size()) {
                    std::uniform_int_distribution<> veg_dis(0, rule.vegetation_blocks.size() - 1);
                    volume.blocks[idx] = rule.vegetation_blocks[veg_dis(gen)];
                }
            }
        }
    }
}

ExtendedBiome AIBiomeEnhancer::createDynamicBiome(const BiomeContext& context) {
    // Find an available dynamic biome slot
    for (int i = 18; i <= 21; ++i) {
        ExtendedBiome biome_id = static_cast<ExtendedBiome>(i);
        if (dynamic_biomes_.find(biome_id) == dynamic_biomes_.end()) {
            // Generate biome rule
            BiomeRule rule = generateBiomeRule(context.desired_theme, 
                                             biome_utils::createContextFromBiomeContext(context));
            
            dynamic_biomes_[biome_id] = rule;
            return biome_id;
        }
    }
    
    // No available slots - overwrite the first dynamic biome
    ExtendedBiome biome_id = ExtendedBiome::AIDynamic0;
    BiomeRule rule = generateBiomeRule(context.desired_theme,
                                     biome_utils::createContextFromBiomeContext(context));
    dynamic_biomes_[biome_id] = rule;
    return biome_id;
}

bool AIBiomeEnhancer::updateDynamicBiome(ExtendedBiome biome_id, const BiomeRule& new_rule) {
    if (!isDynamicBiome(biome_id)) {
        std::cerr << "Cannot update non-dynamic biome: " << (int)biome_id << std::endl;
        return false;
    }
    
    if (!validateBiomeRule(new_rule)) {
        std::cerr << "Invalid biome rule for dynamic biome update" << std::endl;
        return false;
    }
    
    dynamic_biomes_[biome_id] = new_rule;
    return true;
}

void AIBiomeEnhancer::enhanceBiomeTransitions(VoxelVolume& volume,
                                            const std::vector<ExtendedBiome>& adjacent_biomes,
                                            const glm::vec3& chunk_position) {
    
    if (adjacent_biomes.size() < 2) return;
    
    float smoothness = config_.biome_transition_smoothness;
    if (smoothness <= 0.0f) return;
    
    // Create a transition map
    std::vector<std::vector<float>> transition_map(volume.sizeX, std::vector<float>(volume.sizeZ, 0.0f));
    
    // Calculate transition zones
    for (int z = 0; z < volume.sizeZ; ++z) {
        for (int x = 0; x < volume.sizeX; ++x) {
            ExtendedBiome current_biome = biome_utils::detectBiomeAt(volume, {x, 0, z});
            
            // Check nearby positions for different biomes
            int transition_count = 0;
            for (int dz = -2; dz <= 2; ++dz) {
                for (int dx = -2; dx <= 2; ++dx) {
                    int nx = x + dx, nz = z + dz;
                    if (nx >= 0 && nx < volume.sizeX && nz >= 0 && nz < volume.sizeZ) {
                        ExtendedBiome nearby_biome = biome_utils::detectBiomeAt(volume, {nx, 0, nz});
                        if (nearby_biome != current_biome) {
                            transition_count++;
                        }
                    }
                }
            }
            
            transition_map[x][z] = std::min(1.0f, transition_count / 10.0f);
        }
    }
    
    // Apply transitions
    for (int z = 0; z < volume.sizeZ; ++z) {
        for (int x = 0; x < volume.sizeX; ++x) {
            float transition_strength = transition_map[x][z] * smoothness;
            if (transition_strength > 0.1f) {
                applyTransitionEffects(volume, x, z, adjacent_biomes, transition_strength);
            }
        }
    }
}

void AIBiomeEnhancer::applyTransitionEffects(VoxelVolume& volume, int x, int z,
                                           const std::vector<ExtendedBiome>& biomes,
                                           float transition_strength) {
    
    // Find surface height
    int surface_y = 0;
    for (int y = volume.sizeY - 1; y >= 0; --y) {
        size_t idx = (size_t)z * volume.sizeY * volume.sizeX + (size_t)y * volume.sizeX + x;
        if (idx < volume.blocks.size() && volume.blocks[idx] != config_.ids.Air) {
            surface_y = y;
            break;
        }
    }
    
    // Create blended surface
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    if (dis(gen) < transition_strength) {
        // Randomly select one of the adjacent biomes for this position
        std::uniform_int_distribution<> biome_dis(0, biomes.size() - 1);
        ExtendedBiome selected_biome = biomes[biome_dis(gen)];
        
        uint16_t transition_block = getBiomeSurfaceBlock(selected_biome, config_);
        
        size_t surface_idx = (size_t)z * volume.sizeY * volume.sizeX + 
                            (size_t)surface_y * volume.sizeX + x;
        if (surface_idx < volume.blocks.size()) {
            volume.blocks[surface_idx] = transition_block;
        }
    }
}

MaterialTemplate AIBiomeEnhancer::generateMaterialTemplate(const MaterialRequest& request) {
    // Check cache first
    std::string cache_key = request.material_type + "_" + request.style_description;
    if (caching_enabled_) {
        auto it = material_cache_.find(cache_key);
        if (it != material_cache_.end()) {
            return it->second;
        }
    }
    
    MaterialTemplate template_def;
    template_def.base_material = request.material_type;
    template_def.preferred_resolution = request.resolution;
    template_def.requires_normal_map = request.generate_normal_map;
    template_def.requires_roughness_map = request.generate_roughness_map;
    
    // Parse style description for material properties
    std::string style_lower = request.style_description;
    std::transform(style_lower.begin(), style_lower.end(), style_lower.begin(), ::tolower);
    
    // Set base properties based on material type
    if (request.material_type == "stone") {
        template_def.base_color = {0.6f, 0.6f, 0.6f};
        template_def.roughness = 0.8f;
        template_def.metallic = 0.0f;
        template_def.hardness = 3.0f;
    } else if (request.material_type == "wood") {
        template_def.base_color = {0.6f, 0.4f, 0.2f};
        template_def.roughness = 0.7f;
        template_def.metallic = 0.0f;
        template_def.hardness = 1.5f;
    } else if (request.material_type == "metal") {
        template_def.base_color = {0.7f, 0.7f, 0.7f};
        template_def.roughness = 0.2f;
        template_def.metallic = 1.0f;
        template_def.hardness = 4.0f;
    }
    
    // Modify based on style
    if (style_lower.find("weathered") != std::string::npos) {
        template_def.roughness += 0.2f;
        template_def.style_tags.push_back("weathered");
    }
    
    if (style_lower.find("polished") != std::string::npos) {
        template_def.roughness -= 0.3f;
        template_def.style_tags.push_back("polished");
    }
    
    if (style_lower.find("ancient") != std::string::npos) {
        template_def.base_color = template_def.base_color * 0.8f; // Darker
        template_def.style_tags.push_back("ancient");
    }
    
    if (style_lower.find("glowing") != std::string::npos) {
        template_def.emits_light = true;
        template_def.light_level = 8;
        template_def.emission = 0.5f;
        template_def.style_tags.push_back("glowing");
    }
    
    // Clamp values
    template_def.roughness = std::clamp(template_def.roughness, 0.0f, 1.0f);
    template_def.metallic = std::clamp(template_def.metallic, 0.0f, 1.0f);
    template_def.emission = std::clamp(template_def.emission, 0.0f, 1.0f);
    
    // Cache the result
    if (caching_enabled_) {
        material_cache_[cache_key] = template_def;
    }
    
    return template_def;
}

bool AIBiomeEnhancer::validateBiomeRule(const BiomeRule& rule) {
    // Check temperature range
    if (rule.temperature_range[0] < 0.0f || rule.temperature_range[1] > 1.0f || 
        rule.temperature_range[0] > rule.temperature_range[1]) {
        return false;
    }
    
    // Check humidity range  
    if (rule.humidity_range[0] < 0.0f || rule.humidity_range[1] > 1.0f ||
        rule.humidity_range[0] > rule.humidity_range[1]) {
        return false;
    }
    
    // Check elevation range
    if (rule.elevation_range[0] < 0.0f || rule.elevation_range[1] > 1.0f ||
        rule.elevation_range[0] > rule.elevation_range[1]) {
        return false;
    }
    
    // Check structure density
    if (rule.structure_density < 0.0f || rule.structure_density > 1.0f) {
        return false;
    }
    
    // Check that required blocks exist
    if (rule.surface_blocks.empty()) {
        return false;
    }
    
    return true;
}

bool AIBiomeEnhancer::validateMaterialTemplate(const MaterialTemplate& template_def) {
    // Check color values
    if (template_def.base_color.x < 0.0f || template_def.base_color.x > 1.0f ||
        template_def.base_color.y < 0.0f || template_def.base_color.y > 1.0f ||
        template_def.base_color.z < 0.0f || template_def.base_color.z > 1.0f) {
        return false;
    }
    
    // Check material properties
    if (template_def.roughness < 0.0f || template_def.roughness > 1.0f ||
        template_def.metallic < 0.0f || template_def.metallic > 1.0f ||
        template_def.emission < 0.0f || template_def.emission > 1.0f) {
        return false;
    }
    
    // Check light level
    if (template_def.light_level < 0 || template_def.light_level > 15) {
        return false;
    }
    
    // Check resolution
    if (template_def.preferred_resolution <= 0 || template_def.preferred_resolution > 2048) {
        return false;
    }
    
    return true;
}

uint16_t AIBiomeEnhancer::getBiomeSurfaceBlock(ExtendedBiome biome, 
                                             const EnhancedPaletteConfig& config) {
    int biome_idx = static_cast<int>(biome);
    if (biome_idx >= 0 && biome_idx < config.extended_biome_surface_map.size()) {
        return config.extended_biome_surface_map[biome_idx];
    }
    return config.ids.Grass; // Default fallback
}

bool AIBiomeEnhancer::isAIBiome(ExtendedBiome biome) {
    int biome_idx = static_cast<int>(biome);
    return biome_idx >= 8; // AI biomes start at index 8
}

bool AIBiomeEnhancer::isDynamicBiome(ExtendedBiome biome) {
    return biome >= ExtendedBiome::AIDynamic0 && biome <= ExtendedBiome::AIDynamic3;
}

void AIBiomeEnhancer::setQualityLevel(float quality) {
    quality_level_ = std::clamp(quality, 0.0f, 1.0f);
}

void AIBiomeEnhancer::enableCaching(bool enable) {
    caching_enabled_ = enable;
    if (!enable) {
        clearCache();
    }
}

void AIBiomeEnhancer::clearCache() {
    biome_rule_cache_.clear();
    material_cache_.clear();
}

// Utility functions implementation
namespace biome_utils {

ExtendedBiome detectBiomeAt(const VoxelVolume& volume, const glm::vec3& position) {
    // Simple detection based on surface block type
    int x = std::clamp((int)position.x, 0, volume.sizeX - 1);
    int z = std::clamp((int)position.z, 0, volume.sizeZ - 1);
    
    // Find surface block
    for (int y = volume.sizeY - 1; y >= 0; --y) {
        size_t idx = (size_t)z * volume.sizeY * volume.sizeX + (size_t)y * volume.sizeX + x;
        if (idx < volume.blocks.size() && volume.blocks[idx] != 0) { // Assuming 0 is air
            uint16_t surface_block = volume.blocks[idx];
            
            // Map surface block to biome (simplified)
            if (surface_block == 2) return ExtendedBiome::Desert;      // Sand
            if (surface_block == 6) return ExtendedBiome::Snow;        // Snow  
            if (surface_block == 3) return ExtendedBiome::Plains;      // Grass
            if (surface_block == 5) return ExtendedBiome::Mountain;    // Stone
            
            break;
        }
    }
    
    return ExtendedBiome::Plains; // Default
}

std::vector<ExtendedBiome> detectBiomesInVolume(const VoxelVolume& volume) {
    std::set<ExtendedBiome> unique_biomes;
    
    // Sample biomes at regular intervals
    int step = std::max(1, volume.sizeX / 8);
    for (int z = 0; z < volume.sizeZ; z += step) {
        for (int x = 0; x < volume.sizeX; x += step) {
            ExtendedBiome biome = detectBiomeAt(volume, {x, 0, z});
            unique_biomes.insert(biome);
        }
    }
    
    return std::vector<ExtendedBiome>(unique_biomes.begin(), unique_biomes.end());
}

EnvironmentalContext createContextFromVolume(const VoxelVolume& volume, const glm::vec3& position) {
    EnvironmentalContext context;
    context.world_position = position;
    context.current_biome = "plains"; // Default
    context.temperature = 0.5f;
    context.humidity = 0.5f;
    context.elevation = position.y / 256.0f; // Normalize assuming max height of 256
    
    // Detect nearby biomes
    std::vector<ExtendedBiome> nearby = detectBiomesInVolume(volume);
    for (ExtendedBiome biome : nearby) {
        context.nearby_biomes.push_back("biome_" + std::to_string((int)biome));
    }
    
    return context;
}

EnvironmentalContext createContextFromBiomeContext(const BiomeContext& biome_context) {
    EnvironmentalContext context;
    context.world_position = {biome_context.world_position.x, 0, biome_context.world_position.y};
    context.temperature = biome_context.temperature;
    context.humidity = biome_context.humidity;
    context.elevation = biome_context.elevation;
    context.nearby_biomes = biome_context.adjacent_biomes;
    return context;
}

bool areBiomesCompatible(ExtendedBiome a, ExtendedBiome b) {
    // Define biome compatibility rules
    if (a == b) return true;
    
    // Desert is not compatible with snow/tundra
    if ((a == ExtendedBiome::Desert && b == ExtendedBiome::Snow) ||
        (a == ExtendedBiome::Snow && b == ExtendedBiome::Desert)) {
        return false;
    }
    
    // Volcanic is not compatible with snow
    if ((a == ExtendedBiome::AIVolcanic && b == ExtendedBiome::Snow) ||
        (a == ExtendedBiome::Snow && b == ExtendedBiome::AIVolcanic)) {
        return false;
    }
    
    return true; // Most biomes are compatible
}

float getBiomeSimilarity(ExtendedBiome a, ExtendedBiome b) {
    if (a == b) return 1.0f;
    
    // Define similarity based on biome characteristics
    // This is simplified - in practice you'd compare temperature, humidity, etc.
    
    // Plains-like biomes
    if ((a == ExtendedBiome::Plains || a == ExtendedBiome::AIForest) &&
        (b == ExtendedBiome::Plains || b == ExtendedBiome::AIForest)) {
        return 0.8f;
    }
    
    // Cold biomes
    if ((a == ExtendedBiome::Snow || a == ExtendedBiome::AITundra) &&
        (b == ExtendedBiome::Snow || b == ExtendedBiome::AITundra)) {
        return 0.9f;
    }
    
    // Hot/dry biomes
    if ((a == ExtendedBiome::Desert || a == ExtendedBiome::AIVolcanic) &&
        (b == ExtendedBiome::Desert || b == ExtendedBiome::AIVolcanic)) {
        return 0.7f;
    }
    
    return 0.3f; // Default low similarity
}

} // namespace biome_utils

} // namespace voxelvk::ai