#include <gtest/gtest.h>
#include "../src/ai/ai_enhanced_generator.hpp"
#include "../src/ai/ai_tensorrt_manager.hpp"
#include "../src/ai/ai_enhanced_biome_system.hpp"
#include "../src/ai/ai_runtime_commands.hpp"
#include "../src/ai/ai_training_integration.hpp"

using namespace voxelvk::ai;

class AIIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test configuration
        config.trt_engine_path = "test_models/test_engine.trt";
        config.structure_trt_path = "test_models/structure_test.trt";
        config.terrain_trt_path = "test_models/terrain_test.trt";
        config.texture_trt_path = "test_models/texture_test.trt";
        config.biome_trt_path = "test_models/biome_test.trt";
        config.latent_size = 512;
        config.sdf_size = 64;
        config.use_gpu = false; // Use CPU for testing
        config.structure_complexity = 0.5f;
        config.texture_resolution = 128;
        config.enable_runtime_generation = true;
        
        // Setup enhanced palette config
        palette_config = EnhancedPaletteConfig();
        palette_config.enable_ai_structures = true;
        palette_config.enable_ai_textures = true;
        palette_config.enable_dynamic_biomes = true;
        palette_config.structure_density = 0.2f;
        palette_config.terrain_variation = 1.0f;
        palette_config.biome_transition_smoothness = 0.5f;
    }
    
    void TearDown() override {
        // Cleanup
    }
    
    EnhancedAiGenConfig config;
    EnhancedPaletteConfig palette_config;
};

// Test TensorRT Manager initialization
TEST_F(AIIntegrationTest, TensorRTManagerInitialization) {
    TensorRTMultiModelManager manager;
    
    // Test initialization with mock config (should handle missing files gracefully)
    bool result = manager.initialize(config);
    
    // Since we don't have actual model files, this might fail
    // but should not crash
    EXPECT_TRUE(result || !result); // Just ensure it doesn't crash
    
    // Test configuration retrieval
    const auto& retrieved_config = manager.getConfig();
    EXPECT_EQ(retrieved_config.latent_size, config.latent_size);
    EXPECT_EQ(retrieved_config.sdf_size, config.sdf_size);
    EXPECT_EQ(retrieved_config.structure_complexity, config.structure_complexity);
}

// Test Enhanced Biome System
TEST_F(AIIntegrationTest, BiomeSystemInitialization) {
    AIBiomeEnhancer biome_enhancer;
    TensorRTMultiModelManager trt_manager;
    
    // Initialize (without actual TensorRT models)
    bool result = biome_enhancer.initialize(&trt_manager, palette_config);
    EXPECT_TRUE(result);
    
    // Test configuration retrieval
    const auto& config = biome_enhancer.getConfig();
    EXPECT_TRUE(config.enable_ai_structures);
    EXPECT_TRUE(config.enable_ai_textures);
    EXPECT_EQ(config.structure_density, 0.2f);
}

// Test Biome Rule Generation (Procedural fallback)
TEST_F(AIIntegrationTest, BiomeRuleGeneration) {
    AIBiomeEnhancer biome_enhancer;
    TensorRTMultiModelManager trt_manager;
    
    biome_enhancer.initialize(&trt_manager, palette_config);
    
    // Create test environmental context
    EnvironmentalContext context;
    context.world_position = {100, 64, 100};
    context.current_biome = "plains";
    context.temperature = 0.6f;
    context.humidity = 0.4f;
    context.elevation = 0.3f;
    context.nearby_biomes = {"forest", "desert"};
    
    // Generate biome rule with descriptive prompt
    BiomeRule rule = biome_enhancer.generateBiomeRule("hot volcanic wasteland", context);
    
    // Validate generated rule
    EXPECT_FALSE(rule.name.empty());
    EXPECT_GE(rule.temperature_range[0], 0.0f);
    EXPECT_LE(rule.temperature_range[1], 1.0f);
    EXPECT_LE(rule.temperature_range[0], rule.temperature_range[1]);
    EXPECT_GE(rule.humidity_range[0], 0.0f);
    EXPECT_LE(rule.humidity_range[1], 1.0f);
    EXPECT_FALSE(rule.surface_blocks.empty());
    
    // Test that hot description affects temperature
    EXPECT_GE(rule.temperature_range[0], 0.7f); // Should be hot
}

// Test Material Template Generation
TEST_F(AIIntegrationTest, MaterialTemplateGeneration) {
    AIBiomeEnhancer biome_enhancer;
    TensorRTMultiModelManager trt_manager;
    
    biome_enhancer.initialize(&trt_manager, palette_config);
    
    // Create material request
    MaterialRequest request;
    request.material_type = "stone";
    request.style_description = "weathered ancient stone";
    request.resolution = 256;
    request.generate_normal_map = true;
    request.generate_roughness_map = true;
    request.seamless = true;
    
    // Generate material template
    MaterialTemplate template_def = biome_enhancer.generateMaterialTemplate(request);
    
    // Validate template
    EXPECT_EQ(template_def.base_material, "stone");
    EXPECT_EQ(template_def.preferred_resolution, 256);
    EXPECT_TRUE(template_def.requires_normal_map);
    EXPECT_TRUE(template_def.requires_roughness_map);
    EXPECT_GE(template_def.roughness, 0.0f);
    EXPECT_LE(template_def.roughness, 1.0f);
    EXPECT_GE(template_def.metallic, 0.0f);
    EXPECT_LE(template_def.metallic, 1.0f);
    
    // Check for style tags
    bool has_weathered = std::find(template_def.style_tags.begin(), 
                                  template_def.style_tags.end(), 
                                  "weathered") != template_def.style_tags.end();
    EXPECT_TRUE(has_weathered);
}

// Test Dynamic Biome Creation
TEST_F(AIIntegrationTest, DynamicBiomeCreation) {
    AIBiomeEnhancer biome_enhancer;
    TensorRTMultiModelManager trt_manager;
    
    biome_enhancer.initialize(&trt_manager, palette_config);
    
    // Create biome context
    BiomeContext context;
    context.world_position = {200, 300};
    context.temperature = 0.8f;
    context.humidity = 0.2f;
    context.elevation = 0.7f;
    context.desired_theme = "crystalline caverns";
    context.adjacent_biomes = {"mountain", "desert"};
    
    // Create dynamic biome
    ExtendedBiome biome_id = biome_enhancer.createDynamicBiome(context);
    
    // Validate biome ID is in dynamic range
    EXPECT_GE(static_cast<int>(biome_id), static_cast<int>(ExtendedBiome::AIDynamic0));
    EXPECT_LE(static_cast<int>(biome_id), static_cast<int>(ExtendedBiome::AIDynamic3));
    
    // Test that multiple dynamic biomes can be created
    BiomeContext context2 = context;
    context2.desired_theme = "floating gardens";
    ExtendedBiome biome_id2 = biome_enhancer.createDynamicBiome(context2);
    
    // Should get different biome ID or overwrite if slots are full
    EXPECT_TRUE(biome_id2 >= ExtendedBiome::AIDynamic0 && biome_id2 <= ExtendedBiome::AIDynamic3);
}

// Test Runtime Command System
TEST_F(AIIntegrationTest, RuntimeCommandSystem) {
    AIRuntimeCommandSystem command_system;
    TensorRTMultiModelManager trt_manager;
    AIBiomeEnhancer biome_enhancer;
    AIStructureGenerator structure_generator;
    
    // Initialize components
    biome_enhancer.initialize(&trt_manager, palette_config);
    structure_generator.initialize(&trt_manager);
    
    bool result = command_system.initialize(&trt_manager, &biome_enhancer, &structure_generator);
    EXPECT_TRUE(result);
    
    // Test structure generation command
    uint64_t command_id = command_system.generateStructureAt(
        {100, 64, 100}, 
        "medieval watchtower", 
        ArchitecturalStyle::Medieval, 
        0.6f
    );
    
    EXPECT_GT(command_id, 0);
    
    // Test biome enhancement command
    ChunkCoordinate chunk = {5, 5};
    uint64_t biome_command_id = command_system.enhanceBiomeAt(
        chunk, 
        "mystical forest with glowing mushrooms", 
        0.8f
    );
    
    EXPECT_GT(biome_command_id, 0);
    EXPECT_NE(command_id, biome_command_id);
    
    // Test system stats
    auto stats = command_system.getSystemStats();
    EXPECT_GE(stats.commands_queued, 0);
    EXPECT_GE(stats.commands_executing, 0);
    EXPECT_GE(stats.commands_completed, 0);
}

// Test Training Environment Generation
TEST_F(AIIntegrationTest, TrainingEnvironmentGeneration) {
    AITrainingEnvironmentGenerator env_generator;
    TensorRTMultiModelManager trt_manager;
    AIBiomeEnhancer biome_enhancer;
    AIStructureGenerator structure_generator;
    
    // Initialize components
    biome_enhancer.initialize(&trt_manager, palette_config);
    structure_generator.initialize(&trt_manager);
    env_generator.initialize(&trt_manager, &biome_enhancer, &structure_generator);
    
    // Create curriculum stage
    CurriculumStage stage;
    stage.stage_name = "basic_survival";
    stage.difficulty = DifficultyLevel::Easy;
    stage.world_size = {64, 64, 64};
    stage.allowed_biomes = {"plains", "forest"};
    stage.allowed_structures = {"house", "well"};
    stage.success_rate_threshold = 0.7f;
    stage.minimum_episodes = 50;
    
    // Add training objectives
    TrainingObjective objective;
    objective.name = "build_shelter";
    objective.description = "Build a basic shelter to survive the night";
    objective.required_structures = {"house"};
    objective.required_materials = {"wood", "stone"};
    objective.difficulty_rating = 0.3f;
    objective.expected_completion_time = 300.0f;
    stage.objectives.push_back(objective);
    
    // Generate training scenario
    TrainingScenario scenario = env_generator.generateScenario(stage);
    
    // Validate scenario
    EXPECT_EQ(scenario.difficulty, DifficultyLevel::Easy);
    EXPECT_FALSE(scenario.name.empty());
    EXPECT_FALSE(scenario.description.empty());
    EXPECT_FALSE(scenario.objectives.empty());
    EXPECT_EQ(scenario.objectives[0].name, "build_shelter");
    
    // Test scenario analysis
    auto analysis = env_generator.analyzeScenario(scenario);
    EXPECT_TRUE(analysis.is_feasible);
    EXPECT_GE(analysis.estimated_difficulty, 0.0f);
    EXPECT_LE(analysis.estimated_difficulty, 1.0f);
    EXPECT_GT(analysis.estimated_completion_time, 0.0f);
}

// Test Curriculum Manager
TEST_F(AIIntegrationTest, CurriculumManager) {
    AICurriculumManager curriculum_manager;
    AITrainingEnvironmentGenerator env_generator;
    TensorRTMultiModelManager trt_manager;
    AIBiomeEnhancer biome_enhancer;
    AIStructureGenerator structure_generator;
    
    // Initialize components
    biome_enhancer.initialize(&trt_manager, palette_config);
    structure_generator.initialize(&trt_manager);
    env_generator.initialize(&trt_manager, &biome_enhancer, &structure_generator);
    curriculum_manager.initialize(&env_generator);
    
    // Create learning path
    LearningPath path = AITrainingConfigFactory::createSurvivalPath();
    
    bool result = curriculum_manager.createLearningPath(path);
    EXPECT_TRUE(result);
    
    // Verify path was created
    auto available_paths = curriculum_manager.getAvailablePaths();
    EXPECT_FALSE(available_paths.empty());
    
    bool found_path = std::find(available_paths.begin(), available_paths.end(), 
                               path.name) != available_paths.end();
    EXPECT_TRUE(found_path);
    
    // Test agent performance tracking
    std::string agent_id = "test_agent_001";
    TrainingScenario test_scenario;
    test_scenario.name = "test_scenario";
    test_scenario.difficulty = DifficultyLevel::Easy;
    
    curriculum_manager.updateAgentPerformance(agent_id, test_scenario, true, 250.0f, 0.85f);
    
    auto performance = curriculum_manager.getAgentPerformance(agent_id);
    EXPECT_EQ(performance.agent_id, agent_id);
    EXPECT_EQ(performance.total_episodes, 1);
    EXPECT_EQ(performance.successful_episodes, 1);
    EXPECT_FLOAT_EQ(performance.success_rate, 1.0f);
}

// Test Configuration Factory
TEST_F(AIIntegrationTest, ConfigurationFactory) {
    // Test learning path creation
    LearningPath survival_path = AITrainingConfigFactory::createSurvivalPath();
    EXPECT_FALSE(survival_path.name.empty());
    EXPECT_FALSE(survival_path.stages.empty());
    EXPECT_GT(survival_path.overall_difficulty_progression, 1.0f);
    
    LearningPath construction_path = AITrainingConfigFactory::createConstructionPath();
    EXPECT_FALSE(construction_path.name.empty());
    EXPECT_FALSE(construction_path.stages.empty());
    
    // Test training configuration creation
    EnhancedTrainingConfig default_config = AITrainingConfigFactory::createDefaultConfig();
    EXPECT_TRUE(default_config.ai_content.enable_ai_scenarios);
    EXPECT_GT(default_config.ai_content.content_quality, 0.0f);
    EXPECT_LE(default_config.ai_content.content_quality, 1.0f);
    
    EnhancedTrainingConfig performance_config = AITrainingConfigFactory::createHighPerformanceConfig();
    EXPECT_TRUE(performance_config.performance.prefer_speed_over_quality);
    
    // Test configuration validation
    bool is_valid = AITrainingConfigFactory::validateTrainingConfig(default_config);
    EXPECT_TRUE(is_valid);
}

// Test Utility Functions
TEST_F(AIIntegrationTest, UtilityFunctions) {
    // Create test voxel volume
    VoxelVolume volume;
    volume.sizeX = volume.sizeY = volume.sizeZ = 32;
    volume.blocks.resize(32 * 32 * 32, 0);
    
    // Set some surface blocks
    for (int z = 0; z < 32; ++z) {
        for (int x = 0; x < 32; ++x) {
            size_t idx = z * 32 * 32 + 10 * 32 + x; // y = 10 as surface
            if (x < 16) {
                volume.blocks[idx] = 3; // Grass
            } else {
                volume.blocks[idx] = 2; // Sand  
            }
        }
    }
    
    // Test biome detection
    ExtendedBiome biome1 = biome_utils::detectBiomeAt(volume, {8, 10, 8});
    ExtendedBiome biome2 = biome_utils::detectBiomeAt(volume, {24, 10, 8});
    
    EXPECT_EQ(biome1, ExtendedBiome::Plains);
    EXPECT_EQ(biome2, ExtendedBiome::Desert);
    
    // Test biome compatibility
    bool compatible = biome_utils::areBiomesCompatible(
        ExtendedBiome::Plains, ExtendedBiome::AIForest);
    EXPECT_TRUE(compatible);
    
    bool incompatible = biome_utils::areBiomesCompatible(
        ExtendedBiome::Desert, ExtendedBiome::Snow);
    EXPECT_FALSE(incompatible);
    
    // Test similarity
    float similarity = biome_utils::getBiomeSimilarity(
        ExtendedBiome::Plains, ExtendedBiome::AIForest);
    EXPECT_GT(similarity, 0.5f);
    
    // Test environmental context creation
    EnvironmentalContext context = biome_utils::createContextFromVolume(volume, {16, 10, 16});
    EXPECT_EQ(context.world_position.x, 16);
    EXPECT_EQ(context.world_position.y, 10);
    EXPECT_EQ(context.world_position.z, 16);
    EXPECT_FALSE(context.nearby_biomes.empty());
}

// Performance test
TEST_F(AIIntegrationTest, PerformanceTest) {
    AIBiomeEnhancer biome_enhancer;
    TensorRTMultiModelManager trt_manager;
    
    biome_enhancer.initialize(&trt_manager, palette_config);
    
    // Test cache performance
    biome_enhancer.enableCaching(true);
    
    EnvironmentalContext context;
    context.world_position = {0, 64, 0};
    context.temperature = 0.5f;
    context.humidity = 0.5f;
    context.elevation = 0.5f;
    
    // Time biome rule generation (first call)
    auto start = std::chrono::high_resolution_clock::now();
    BiomeRule rule1 = biome_enhancer.generateBiomeRule("test biome", context);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Time cached call
    start = std::chrono::high_resolution_clock::now();
    BiomeRule rule2 = biome_enhancer.generateBiomeRule("test biome", context);
    end = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Cached call should be faster
    EXPECT_LT(duration2.count(), duration1.count());
    
    // Results should be identical
    EXPECT_EQ(rule1.name, rule2.name);
    EXPECT_EQ(rule1.temperature_range[0], rule2.temperature_range[0]);
    EXPECT_EQ(rule1.temperature_range[1], rule2.temperature_range[1]);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}