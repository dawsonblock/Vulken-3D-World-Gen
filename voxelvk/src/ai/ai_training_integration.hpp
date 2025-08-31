#pragma once
#include "ai_enhanced_generator.hpp"
#include "ai_enhanced_biome_system.hpp"
#include "ai_runtime_commands.hpp"
#include <vector>
#include <memory>

namespace voxelvk::ai {

// Training environment generation for RL agents
class AITrainingEnvironmentGenerator {
public:
    AITrainingEnvironmentGenerator();
    ~AITrainingEnvironmentGenerator();
    
    // Initialization
    bool initialize(TensorRTMultiModelManager* trt_manager,
                   AIBiomeEnhancer* biome_enhancer,
                   AIStructureGenerator* structure_generator);
    
    // === SCENARIO GENERATION ===
    
    // Generate training scenario based on curriculum stage
    TrainingScenario generateScenario(const CurriculumStage& stage);
    
    // Create structured learning environments
    std::vector<TrainingScenario> generateCurriculum(const LearningPath& path);
    
    // Dynamic difficulty adjustment
    TrainingScenario adjustDifficulty(const TrainingScenario& base_scenario, 
                                     float difficulty_factor);
    
    // Generate specific scenario types
    TrainingScenario generateSurvivalScenario(DifficultyLevel difficulty,
                                             const EnvironmentalContext& context);
    TrainingScenario generateConstructionScenario(const std::vector<std::string>& build_objectives,
                                                 DifficultyLevel difficulty);
    TrainingScenario generateExplorationScenario(const glm::vec3& world_size,
                                                DifficultyLevel difficulty);
    TrainingScenario generateCombatScenario(int num_enemies,
                                           DifficultyLevel difficulty);
    TrainingScenario generatePuzzleScenario(const std::string& puzzle_type,
                                           DifficultyLevel difficulty);
    
    // === BATCH GENERATION ===
    
    // Generate multiple diverse scenarios
    std::vector<TrainingScenario> generateScenarioBatch(int num_scenarios,
                                                       const CurriculumStage& stage,
                                                       float diversity_factor = 0.8f);
    
    // Generate scenarios for parallel training
    std::vector<TrainingScenario> generateParallelEnvironments(int num_environments,
                                                              const CurriculumStage& stage);
    
    // === SCENARIO VALIDATION AND ANALYSIS ===
    
    // Validate scenario difficulty and feasibility
    struct ScenarioAnalysis {
        bool is_feasible = true;
        float estimated_difficulty = 0.5f;
        float estimated_completion_time = 300.0f; // seconds
        std::vector<std::string> potential_issues;
        std::vector<std::string> learning_opportunities;
        float diversity_score = 0.0f;
    };
    
    ScenarioAnalysis analyzeScenario(const TrainingScenario& scenario);
    bool validateScenario(const TrainingScenario& scenario);
    
    // Measure scenario diversity
    float calculateDiversity(const std::vector<TrainingScenario>& scenarios);
    
    // === PERFORMANCE OPTIMIZATION ===
    
    // Configure generation parameters for performance
    void setGenerationQuality(float quality); // 0.0 = fast, 1.0 = high quality
    void setMaxGenerationTime(float max_seconds);
    void enableScenarioCaching(bool enable);
    
    // Pregenerate scenarios for faster training startup
    bool pregenerateScenarios(const LearningPath& path, int scenarios_per_stage = 10);
    std::vector<TrainingScenario> getPregenerated Scenarios(const std::string& stage_name);
    
private:
    // Core components
    TensorRTMultiModelManager* trt_manager_ = nullptr;
    AIBiomeEnhancer* biome_enhancer_ = nullptr;
    AIStructureGenerator* structure_generator_ = nullptr;
    
    // Configuration
    float generation_quality_ = 0.7f;
    float max_generation_time_ = 10.0f;
    bool caching_enabled_ = true;
    
    // Scenario cache
    std::unordered_map<std::string, std::vector<TrainingScenario>> pregenerated_scenarios_;
    std::unordered_map<std::string, TrainingScenario> scenario_cache_;
    
    // Generation helpers
    EnvironmentalContext createTrainingContext(const CurriculumStage& stage);
    std::vector<TrainingObjective> generateObjectives(const CurriculumStage& stage);
    std::vector<StructureBlueprint> generateTrainingStructures(const CurriculumStage& stage,
                                                              const EnvironmentalContext& context);
    BiomeRule generateTrainingBiome(const CurriculumStage& stage);
    
    // Difficulty adjustment algorithms
    void adjustStructureComplexity(std::vector<StructureBlueprint>& structures, 
                                  float difficulty_factor);
    void adjustObjectiveDifficulty(std::vector<TrainingObjective>& objectives,
                                  float difficulty_factor);
    void adjustEnvironmentalChallenges(EnvironmentalContext& context,
                                     float difficulty_factor);
};

// Learning path definition for curriculum learning
struct LearningPath {
    std::string name;
    std::string description;
    std::vector<CurriculumStage> stages;
    
    // Path-wide parameters
    float overall_difficulty_progression = 1.2f; // Multiplier per stage
    bool allow_stage_skipping = false;
    int max_retries_per_stage = 3;
    
    // Success criteria for path completion
    float minimum_success_rate = 0.8f;
    int minimum_completed_stages = 0; // 0 = all stages required
    
    // AI generation preferences for this path
    std::vector<std::string> preferred_biomes;
    std::vector<std::string> preferred_structures;
    std::vector<std::string> preferred_materials;
    
    // Adaptive features
    bool enable_dynamic_difficulty = true;
    bool enable_personalization = false;
    float adaptation_rate = 0.1f; // How quickly to adapt difficulty
};

// Enhanced curriculum integration with AI content
class AICurriculumManager {
public:
    AICurriculumManager();
    ~AICurriculumManager();
    
    bool initialize(AITrainingEnvironmentGenerator* env_generator);
    
    // === CURRICULUM MANAGEMENT ===
    
    // Create and manage learning paths
    bool createLearningPath(const LearningPath& path);
    bool updateLearningPath(const std::string& path_name, const LearningPath& updated_path);
    bool deleteLearningPath(const std::string& path_name);
    
    // Get learning paths
    std::vector<std::string> getAvailablePaths() const;
    LearningPath getLearningPath(const std::string& path_name) const;
    
    // === ADAPTIVE CURRICULUM ===
    
    // Agent performance tracking
    struct AgentPerformance {
        std::string agent_id;
        std::string current_path;
        std::string current_stage;
        
        float success_rate = 0.0f;
        float average_completion_time = 0.0f;
        int total_episodes = 0;
        int successful_episodes = 0;
        
        // Performance trends
        std::vector<float> recent_scores;
        float learning_trend = 0.0f; // Positive = improving, negative = struggling
        
        // Personalization data
        std::vector<std::string> preferred_scenario_types;
        std::vector<std::string> challenging_areas;
        std::unordered_map<std::string, float> skill_ratings;
    };
    
    // Track agent progress
    void updateAgentPerformance(const std::string& agent_id, 
                               const TrainingScenario& scenario,
                               bool success,
                               float completion_time,
                               float score);
    
    AgentPerformance getAgentPerformance(const std::string& agent_id) const;
    
    // Adaptive stage progression
    bool shouldAdvanceStage(const std::string& agent_id);
    bool shouldRetryStage(const std::string& agent_id);
    CurriculumStage getNextStage(const std::string& agent_id);
    
    // Dynamic difficulty adjustment
    CurriculumStage adjustStageForAgent(const std::string& agent_id, 
                                       const CurriculumStage& base_stage);
    
    // === MULTI-AGENT CURRICULUM ===
    
    // Manage curriculum for multiple agents
    std::vector<TrainingScenario> generateMultiAgentScenarios(
        const std::vector<std::string>& agent_ids,
        int num_scenarios = 1);
    
    // Collaborative learning scenarios
    TrainingScenario generateCollaborativeScenario(
        const std::vector<std::string>& agent_ids,
        const std::string& collaboration_type = "construction");
    
    // Competitive learning scenarios
    TrainingScenario generateCompetitiveScenario(
        const std::vector<std::string>& agent_ids,
        const std::string& competition_type = "resource_gathering");
    
    // === CURRICULUM ANALYTICS ===
    
    struct CurriculumAnalytics {
        std::string path_name;
        int total_agents = 0;
        int agents_completed = 0;
        float average_completion_time_hours = 0.0f;
        
        // Stage-by-stage breakdown
        std::vector<std::string> stage_names;
        std::vector<float> stage_success_rates;
        std::vector<float> stage_average_times;
        std::vector<int> stage_retry_counts;
        
        // Common failure points
        std::vector<std::string> challenging_stages;
        std::vector<std::string> common_failure_reasons;
        
        // Effectiveness metrics
        float learning_efficiency = 0.0f; // Learning per unit time
        float curriculum_difficulty_balance = 0.0f; // How well balanced stages are
        std::vector<std::string> improvement_suggestions;
    };
    
    CurriculumAnalytics analyzeCurriculumEffectiveness(const std::string& path_name);
    
    // Generate curriculum improvement suggestions
    std::vector<std::string> generateImprovementSuggestions(const std::string& path_name);
    
    // === CONFIGURATION ===
    
    struct CurriculumConfig {
        // Adaptive parameters
        float success_rate_threshold = 0.8f;
        int minimum_episodes_per_stage = 50;
        float difficulty_adjustment_rate = 0.1f;
        
        // Performance tracking
        int performance_history_length = 100;
        float learning_trend_smoothing = 0.1f;
        
        // Multi-agent settings
        bool enable_agent_collaboration = true;
        bool enable_agent_competition = false;
        float social_learning_weight = 0.2f;
        
        // AI generation preferences
        float scenario_diversity_target = 0.7f;
        bool enable_dynamic_content = true;
        int max_concurrent_generations = 4;
    };
    
    void updateConfig(const CurriculumConfig& config);
    const CurriculumConfig& getConfig() const { return config_; }
    
private:
    AITrainingEnvironmentGenerator* env_generator_ = nullptr;
    CurriculumConfig config_;
    
    // Learning path storage
    std::unordered_map<std::string, LearningPath> learning_paths_;
    
    // Agent tracking
    std::unordered_map<std::string, AgentPerformance> agent_performances_;
    
    // Analytics data
    std::unordered_map<std::string, CurriculumAnalytics> curriculum_analytics_;
    
    // Helper methods
    float calculateLearningTrend(const std::vector<float>& recent_scores);
    bool isStageCompleted(const AgentPerformance& performance, const CurriculumStage& stage);
    CurriculumStage getStageByName(const std::string& path_name, const std::string& stage_name);
    void updateAnalytics(const std::string& path_name, const std::string& agent_id,
                        const TrainingScenario& scenario, bool success, float time);
};

// Integration with existing training configuration
struct EnhancedTrainingConfig {
    // Base training parameters (from your existing config)
    std::string algorithm = "PPO";
    int num_envs = 256;
    int total_timesteps = 10000000;
    
    // AI-enhanced parameters
    struct AIContentGeneration {
        bool enable_ai_scenarios = true;
        bool enable_dynamic_biomes = true;
        bool enable_ai_structures = true;
        bool enable_ai_textures = true;
        
        // Generation quality vs performance
        float content_quality = 0.7f; // 0.0 = fast, 1.0 = high quality
        
        // Content diversity
        float scenario_diversity = 0.8f;
        float biome_diversity = 0.6f;
        float structure_diversity = 0.7f;
        
        // Generation frequency
        int new_scenarios_per_stage = 5;
        int biome_variants_per_stage = 3;
        bool regenerate_failed_scenarios = true;
    } ai_content;
    
    // Curriculum learning with AI
    struct AICurriculum {
        bool enable_adaptive_curriculum = true;
        std::string default_learning_path = "comprehensive_survival";
        
        // Stage progression
        float success_rate_threshold = 0.8f;
        int min_episodes_per_stage = 100;
        int max_retries_per_stage = 3;
        
        // Difficulty adjustment
        bool enable_dynamic_difficulty = true;
        float difficulty_adjustment_rate = 0.1f;
        float min_difficulty = 0.2f;
        float max_difficulty = 1.0f;
        
        // Multi-agent settings
        bool enable_collaborative_learning = false;
        bool enable_competitive_learning = false;
        int agents_per_collaborative_scenario = 4;
    } ai_curriculum;
    
    // Performance optimization
    struct PerformanceSettings {
        int max_concurrent_ai_generations = 4;
        float ai_generation_timeout_seconds = 10.0f;
        bool cache_generated_content = true;
        size_t content_cache_size_mb = 512;
        
        // Quality vs performance trade-offs
        bool prefer_speed_over_quality = false;
        bool enable_ai_generation_throttling = true;
        float max_ai_generation_gpu_usage = 0.3f; // Reserve GPU for training
    } performance;
};

// Factory class for creating AI-enhanced training configurations
class AITrainingConfigFactory {
public:
    // Create predefined learning paths
    static LearningPath createSurvivalPath();
    static LearningPath createConstructionPath();
    static LearningPath createExplorationPath();
    static LearningPath createComprehensivePath();
    
    // Create training configurations
    static EnhancedTrainingConfig createDefaultConfig();
    static EnhancedTrainingConfig createHighPerformanceConfig();
    static EnhancedTrainingConfig createHighQualityConfig();
    static EnhancedTrainingConfig createMultiAgentConfig();
    
    // Validate configurations
    static bool validateTrainingConfig(const EnhancedTrainingConfig& config);
    static std::vector<std::string> getConfigurationWarnings(const EnhancedTrainingConfig& config);
};

} // namespace voxelvk::ai