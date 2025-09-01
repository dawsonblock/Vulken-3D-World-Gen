#pragma once
#include "ai_enhanced_generator.hpp"
#include "ai_enhanced_biome_system.hpp"
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace voxelvk::ai {

// Forward declarations
class WorldManager;  // From your existing world system
class ChunkManager;  // From your existing chunk system

// Coordinate system for world integration
struct ChunkCoordinate {
    int x, z;
    
    bool operator==(const ChunkCoordinate& other) const {
        return x == other.x && z == other.z;
    }
    
    bool operator<(const ChunkCoordinate& other) const {
        if (x != other.x) return x < other.x;
        return z < other.z;
    }
};

// Command types for the runtime system
enum class AICommandType {
    GenerateStructure,      // Generate building/dungeon/bridge
    EnhanceBiome,          // Enhance existing biome
    GenerateTexture,       // Generate material texture
    CreateCustomBiome,     // Create entirely new biome
    GenerateSettlement,    // Generate village/city
    ModifyTerrain,         // Modify existing terrain
    ApplyWeatherEffect,    // Apply weather-based changes
    CreatePathway,         // Generate roads/paths
    PlaceDecorations,      // Add decorative elements
    OptimizeChunk          // Optimize chunk for performance
};

// Base command structure
struct AICommand {
    AICommandType type;
    uint64_t command_id;
    std::chrono::steady_clock::time_point created_time;
    std::chrono::steady_clock::time_point deadline;  // When command should complete
    
    // Priority system (0 = lowest, 10 = highest)
    int priority = 5;
    
    // Execution context
    glm::vec3 world_position;
    ChunkCoordinate chunk_coord;
    
    // Result callback
    std::function<void(bool success, const std::string& result_message)> completion_callback;
    
    // Command-specific data (union for memory efficiency)
    union CommandData {
        StructurePrompt structure_prompt;
        MaterialRequest material_request;
        BiomeContext biome_context;
        
        CommandData() {} // Empty constructor for union
        ~CommandData() {} // Empty destructor for union
    } data;
    
    // String-based parameters for flexible commands
    std::unordered_map<std::string, std::string> string_params;
    std::unordered_map<std::string, float> float_params;
    std::unordered_map<std::string, int> int_params;
    std::unordered_map<std::string, bool> bool_params;
};

// Command execution result
struct CommandResult {
    uint64_t command_id;
    bool success = false;
    std::string message;
    std::chrono::steady_clock::time_point completion_time;
    float execution_time_ms = 0.0f;
    
    // Result data
    MultiModalAiOutputs generated_content;
    std::vector<glm::vec3> affected_positions;
    std::vector<ChunkCoordinate> modified_chunks;
    
    // Performance metrics
    float gpu_utilization = 0.0f;
    size_t memory_used_bytes = 0;
    int cache_hits = 0;
    int cache_misses = 0;
};

// Main runtime command system
class AIRuntimeCommandSystem {
public:
    AIRuntimeCommandSystem();
    ~AIRuntimeCommandSystem();
    
    // Initialization
    bool initialize(TensorRTMultiModelManager* trt_manager,
                   AIBiomeEnhancer* biome_enhancer,
                   AIStructureGenerator* structure_generator);
    
    void shutdown();
    
    // World manager integration
    void integrateWithWorldManager(WorldManager* world_mgr);
    void integrateWithChunkManager(ChunkManager* chunk_mgr);
    
    // === STRUCTURE GENERATION COMMANDS ===
    
    // Generate building at specific location
    uint64_t generateStructureAt(const glm::vec3& position, 
                                const std::string& description,
                                ArchitecturalStyle style = ArchitecturalStyle::Medieval,
                                float complexity = 0.5f);
    
                                bool use_rag = false);  // NEW: toggle RAG

    // Generate dungeon complex
    uint64_t generateDungeonComplex(const glm::vec3& center,
                                  int num_floors = 3,
                                  float complexity = 0.6f,
                                  const std::string& theme = "medieval");
    
    // Generate bridge between two points
    uint64_t generateBridge(const glm::vec3& start_point,
                          const glm::vec3& end_point,
                          const std::string& bridge_type = "stone");
    
    // Generate settlement (village or city)
    uint64_t generateSettlement(const glm::vec2& center,
                              int population_size = 50,
                              const std::string& culture = "medieval",
                              bool include_walls = false);
    
    // === BIOME ENHANCEMENT COMMANDS ===
    
    // Enhance existing biome with AI-generated content
    uint64_t enhanceBiomeAt(const ChunkCoordinate& chunk,
                          const std::string& enhancement_description,
                          float intensity = 0.7f);
    
    // Create entirely new custom biome
    uint64_t generateCustomBiome(const ChunkCoordinate& chunk,
                               const std::string& biome_description,
                               const std::string& biome_name = "Custom");
    
    // Apply seasonal changes to biomes
    uint64_t applySeasonalChanges(const std::vector<ChunkCoordinate>& chunks,
                                const std::string& season,
                                float transition_progress = 1.0f);
    
    // === TEXTURE GENERATION COMMANDS ===
    
    // Generate material texture
    uint64_t generateMaterialTexture(const std::string& material_type,
                                   const std::string& style_description,
                                   int resolution = 256,
                                   bool generate_normal_maps = true);
    
    // Enhance existing textures with AI
    uint64_t enhanceExistingTextures(const std::vector<std::string>& material_names,
                                   const std::string& enhancement_style);
    
    // Generate texture variations
    uint64_t generateTextureVariations(const std::string& base_material,
                                     int num_variations = 4,
                                     float variation_strength = 0.5f);
    
    // === TERRAIN MODIFICATION COMMANDS ===
    
    // Modify terrain with AI-generated features
    uint64_t modifyTerrainAt(const glm::vec3& center,
                           float radius,
                           const std::string& modification_type,
                           float intensity = 0.5f);
    
    // Create pathways between locations
    uint64_t createPathway(const std::vector<glm::vec3>& waypoints,
                         const std::string& path_type = "dirt_road",
                         float width = 3.0f);
    
    // Generate decorative elements
    uint64_t placeDecorations(const ChunkCoordinate& chunk,
                            const std::string& decoration_theme,
                            float density = 0.3f);
    
    // === COMMAND MANAGEMENT ===
    
    // Command status and control
    bool isCommandComplete(uint64_t command_id);
    bool getCommandResult(uint64_t command_id, CommandResult& result);
    bool cancelCommand(uint64_t command_id);
    void cancelAllCommands();
    
    // Priority management
    bool setCommandPriority(uint64_t command_id, int priority);
    void pauseExecution();
    void resumeExecution();
    
    // Batch operations
    std::vector<uint64_t> submitCommandBatch(const std::vector<AICommand>& commands);
    bool waitForBatchCompletion(const std::vector<uint64_t>& command_ids, 
                               float timeout_seconds = 60.0f);
    
    // === PERFORMANCE AND MONITORING ===
    
    struct SystemStats {
        int commands_queued = 0;
        int commands_executing = 0;
        int commands_completed = 0;
        int commands_failed = 0;
        
        float average_execution_time_ms = 0.0f;
        float total_gpu_time_ms = 0.0f;
        size_t total_memory_used = 0;
        
        std::chrono::steady_clock::time_point system_start_time;
        float uptime_hours = 0.0f;
    };
    
    SystemStats getSystemStats() const;
    void resetStats();
    
    // Resource management
    void setMaxConcurrentCommands(int max_commands);
    void setMemoryLimit(size_t max_memory_bytes);
    void setExecutionTimeLimit(float max_time_seconds);
    
    // Configuration
    struct RuntimeConfig {
        int max_concurrent_commands = 4;
        size_t memory_limit_bytes = 2ULL * 1024 * 1024 * 1024; // 2GB
        float command_timeout_seconds = 30.0f;
        
        bool enable_caching = true;
        bool enable_priority_queue = true;
        bool enable_performance_monitoring = true;
        
        float quality_vs_speed_preference = 0.7f; // 0.0 = speed, 1.0 = quality
    };
    
    void updateConfig(const RuntimeConfig& config);
    const RuntimeConfig& getConfig() const { return config_; }
    
    // === CALLBACKS AND EVENTS ===
    
    // Event callbacks
    std::function<void(uint64_t command_id, const CommandResult&)> on_command_completed;
    std::function<void(uint64_t command_id, const std::string& error)> on_command_failed;
    std::function<void(uint64_t command_id, float progress)> on_command_progress;
    std::function<void(const std::string& message)> on_system_message;
    
    // Validation callbacks
    std::function<bool(const AICommand&)> validate_command;
    std::function<bool(const glm::vec3&)> validate_world_position;
    std::function<bool(const ChunkCoordinate&)> validate_chunk_coordinate;
    
private:
    // System components
    TensorRTMultiModelManager* trt_manager_ = nullptr;
    AIBiomeEnhancer* biome_enhancer_ = nullptr;
    AIStructureGenerator* structure_generator_ = nullptr;
    WorldManager* world_manager_ = nullptr;
    ChunkManager* chunk_manager_ = nullptr;
    
    // Configuration
    RuntimeConfig config_;
    
    // Command processing
    std::priority_queue<AICommand> command_queue_;
    std::unordered_map<uint64_t, CommandResult> completed_commands_;
    std::unordered_map<uint64_t, AICommand> executing_commands_;
    
    // Threading
    std::vector<std::thread> worker_threads_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<bool> execution_paused_{false};
    
    // Command ID generation
    std::atomic<uint64_t> next_command_id_{1};
    
    // Performance tracking
    mutable SystemStats stats_;
    mutable std::mutex stats_mutex_;
    
    // Caching system
    std::unordered_map<std::string, MultiModalAiOutputs> generation_cache_;
    std::mutex cache_mutex_;
    
    // Internal methods
    void workerLoop();
    bool executeCommand(const AICommand& command, CommandResult& result);
    
    // Command execution implementations
    bool executeStructureGeneration(const AICommand& command, CommandResult& result);
    bool executeBiomeEnhancement(const AICommand& command, CommandResult& result);
    bool executeTextureGeneration(const AICommand& command, CommandResult& result);
    bool executeTerrainModification(const AICommand& command, CommandResult& result);
    bool executeSettlementGeneration(const AICommand& command, CommandResult& result);
    
    // Integration helpers
    bool integrateGeneratedContent(const MultiModalAiOutputs& content,
                                 const glm::vec3& world_position);
    bool updateWorldChunks(const std::vector<ChunkCoordinate>& chunks);
    bool validateWorldPosition(const glm::vec3& position);
    bool validateChunkCoordinate(const ChunkCoordinate& coord);
    
    // Cache management
    std::string generateCacheKey(const AICommand& command);
    bool getCachedResult(const std::string& cache_key, MultiModalAiOutputs& result);
    void storeCachedResult(const std::string& cache_key, const MultiModalAiOutputs& result);
    void cleanupCache();
    
    // Performance monitoring
    void updatePerformanceStats(const CommandResult& result);
    void logSystemMessage(const std::string& message);
    
    // Error handling
    void handleCommandError(uint64_t command_id, const std::string& error_message);
    bool validateCommand(const AICommand& command);
};

// Utility functions for command system
namespace command_utils {
    // Command creation helpers
    AICommand createStructureCommand(const glm::vec3& position,
                                   const std::string& description,
                                   ArchitecturalStyle style = ArchitecturalStyle::Medieval);
    
    AICommand createBiomeCommand(const ChunkCoordinate& chunk,
                               const std::string& enhancement);
    
    AICommand createTextureCommand(const std::string& material_type,
                                 const std::string& style);
    
    // Coordinate conversion utilities
    ChunkCoordinate worldToChunk(const glm::vec3& world_position, int chunk_size = 32);
    glm::vec3 chunkToWorld(const ChunkCoordinate& chunk, int chunk_size = 32);
    
    // Command validation
    bool isValidDescription(const std::string& description);
    bool isValidWorldPosition(const glm::vec3& position);
    bool isValidChunkCoordinate(const ChunkCoordinate& coord);
    
    // Performance estimation
    float estimateCommandExecutionTime(const AICommand& command);
    size_t estimateCommandMemoryUsage(const AICommand& command);
}

} // namespace voxelvk::ai