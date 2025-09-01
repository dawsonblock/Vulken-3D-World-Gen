#pragma once
#include "world_manager.hpp"
#include "raycast_dda.hpp"
#include "../core/timer.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace voxelvk {

// Forward declarations
class Agent;
class Observation;
class Action;

// IVecEnv interface - vectorized environment interface for RL
class IVecEnv {
public:
    virtual ~IVecEnv() = default;
    
    // Environment lifecycle
    virtual bool Initialize(const std::string& config_path = "") = 0;
    virtual void Shutdown() = 0;
    
    // Core RL interface
    virtual std::vector<Observation> Reset() = 0;
    virtual std::vector<Observation> Reset(const std::vector<int32_t>& env_ids) = 0;
    
    virtual std::tuple<std::vector<Observation>, std::vector<float>, std::vector<bool>, std::vector<std::map<std::string, float>>>
        Step(const std::vector<Action>& actions) = 0;
    
    // Environment properties
    virtual int32_t GetNumEnvs() const = 0;
    virtual std::vector<int32_t> GetObservationShape() const = 0;
    virtual std::vector<int32_t> GetActionShape() const = 0;
    
    // Action space information
    virtual int32_t GetNumDiscreteActions() const = 0;
    virtual std::vector<int32_t> GetActionBranches() const = 0;
    virtual std::vector<std::string> GetActionNames() const = 0;
    
    // Environment state
    virtual bool IsRunning() const = 0;
    virtual void Pause() = 0;
    virtual void Resume() = 0;
    
    // Rendering/visualization
    virtual std::vector<uint8_t> Render(int32_t env_id = 0, const std::string& mode = "rgb_array") = 0;
    virtual void EnableRendering(bool enable) = 0;
    
    // Statistics and monitoring
    virtual std::map<std::string, float> GetStats() const = 0;
    virtual void ResetStats() = 0;
    
    // Configuration
    virtual void SetSeed(int32_t seed) = 0;
    virtual void SetConfig(const std::map<std::string, float>& config) = 0;
    virtual std::map<std::string, float> GetConfig() const = 0;
    
    // Callbacks
    using EpisodeCallback = std::function<void(int32_t env_id, float reward, int32_t length, const std::map<std::string, float>& info)>;
    virtual void SetEpisodeCallback(EpisodeCallback callback) = 0;
};

// Observation data structure
class Observation {
public:
    // Visual observations (voxel grids, images)
    struct VisualObs {
        std::vector<float> data;    // Flattened observation data
        std::vector<int32_t> shape; // Shape [C, H, W, D] for 3D or [C, H, W] for 2D
        std::string encoding = "float32"; // Data encoding
    };
    
    // Vector observations (position, velocity, etc.)
    struct VectorObs {
        std::vector<float> data;
        std::vector<std::string> names; // Names for each component
    };
    
    // Ray observations (raycast results)
    struct RayObs {
        std::vector<float> distances;  // Ray hit distances
        std::vector<int32_t> hit_types; // Block types hit
        std::vector<float> normals;    // Hit normals (3 components per ray)
        int32_t num_rays = 0;
    };
    
    Observation() = default;
    explicit Observation(int32_t env_id) : env_id(env_id) {}
    
    int32_t env_id = 0;
    VisualObs visual;
    VectorObs vector;
    RayObs ray;
    
    // Utility methods
    void Clear();
    size_t GetTotalSize() const;
    bool IsValid() const;
    
    // Serialization for network transfer
    std::vector<uint8_t> Serialize() const;
    bool Deserialize(const std::vector<uint8_t>& data);
};

// Action data structure
class Action {
public:
    // Discrete actions (movement, looking, interaction)
    std::vector<int32_t> discrete;
    
    // Continuous actions (camera angles, movement speed)
    std::vector<float> continuous;
    
    Action() = default;
    explicit Action(int32_t env_id) : env_id(env_id) {}
    
    int32_t env_id = 0;
    
    // Utility methods
    void Clear();
    bool IsValid() const;
    size_t GetSize() const;
    
    // Predefined action types for voxel environments
    enum class Movement : int32_t {
        None = 0,
        Forward = 1,
        Backward = 2,
        Left = 3,
        Right = 4,
        Up = 5,
        Down = 6
    };
    
    enum class Look : int32_t {
        None = 0,
        Up = 1,
        Down = 2,
        Left = 3,
        Right = 4,
        UpLeft = 5,
        UpRight = 6,
        DownLeft = 7,
        DownRight = 8
    };
    
    enum class Interaction : int32_t {
        None = 0,
        Use = 1,
        Attack = 2
    };
    
    enum class BlockSelection : int32_t {
        Slot1 = 0, Slot2 = 1, Slot3 = 2, Slot4 = 3, Slot5 = 4,
        Slot6 = 5, Slot7 = 6, Slot8 = 7, Slot9 = 8, Slot10 = 9
    };
    
    enum class Special : int32_t {
        None = 0,
        Jump = 1,
        Crouch = 2
    };
    
    // Action helpers
    void SetMovement(Movement movement);
    void SetLook(Look look);
    void SetInteraction(Interaction interaction);
    void SetBlockSelection(BlockSelection selection);
    void SetSpecial(Special special);
    
    Movement GetMovement() const;
    Look GetLook() const;
    Interaction GetInteraction() const;
    BlockSelection GetBlockSelection() const;
    Special GetSpecial() const;
};

// Agent state in the environment
class Agent {
public:
    Agent(int32_t id, WorldManager* world_manager);
    ~Agent() = default;
    
    // Agent properties
    int32_t GetId() const { return m_id; }
    Vec3 GetPosition() const { return m_position; }
    Vec3 GetVelocity() const { return m_velocity; }
    Vec3 GetLookDirection() const { return m_look_direction; }
    
    void SetPosition(const Vec3& position) { m_position = position; }
    void SetVelocity(const Vec3& velocity) { m_velocity = velocity; }
    void SetLookDirection(const Vec3& direction) { m_look_direction = direction.normalized(); }
    
    // Agent state
    float GetHealth() const { return m_health; }
    float GetEnergy() const { return m_energy; }
    bool IsAlive() const { return m_health > 0.0f; }
    
    void SetHealth(float health) { m_health = std::clamp(health, 0.0f, 100.0f); }
    void SetEnergy(float energy) { m_energy = std::clamp(energy, 0.0f, 100.0f); }
    
    // Inventory
    std::vector<BlockType> GetInventory() const { return m_inventory; }
    BlockType GetSelectedBlock() const { return m_selected_block; }
    void SetSelectedBlock(BlockType block) { m_selected_block = block; }
    void SetInventorySlot(int32_t slot, BlockType block);
    
    // Actions
    void ApplyAction(const Action& action, float dt);
    void Update(float dt);
    
    // Observations
    Observation GetObservation() const;
    
    // Collision and physics
    bool CheckCollision(const Vec3& new_position) const;
    void HandleCollisions();
    
    // Block interaction
    bool PlaceBlock(const Vec3& target_position, BlockType block_type);
    bool BreakBlock(const Vec3& target_position);
    RaycastHit GetTargetBlock() const;
    
    // Configuration
    struct Config {
        float movement_speed = 4.0f;
        float jump_force = 8.0f;
        float look_sensitivity = 2.0f;
        float reach_distance = 5.0f;
        float collision_radius = 0.3f;
        float collision_height = 1.8f;
        float gravity = -9.81f;
        bool enable_flying = false;
        bool enable_collision = true;
        bool enable_gravity = true;
    };
    
    void SetConfig(const Config& config) { m_config = config; }
    const Config& GetConfig() const { return m_config; }
    
    // Statistics
    struct Stats {
        uint64_t blocks_placed = 0;
        uint64_t blocks_broken = 0;
        uint64_t steps_taken = 0;
        float distance_traveled = 0.0f;
        float time_alive = 0.0f;
        uint32_t deaths = 0;
        
        void Reset() {
            blocks_placed = 0;
            blocks_broken = 0;
            steps_taken = 0;
            distance_traveled = 0.0f;
            time_alive = 0.0f;
            deaths = 0;
        }
    };
    
    const Stats& GetStats() const { return m_stats; }
    void ResetStats() { m_stats.Reset(); }
    
private:
    int32_t m_id;
    WorldManager* m_world_manager;
    std::unique_ptr<RaycastDDA> m_raycaster;
    
    // Agent state
    Vec3 m_position{0, 100, 0};  // Start above ground
    Vec3 m_velocity{0, 0, 0};
    Vec3 m_look_direction{0, 0, 1};
    
    float m_health = 100.0f;
    float m_energy = 100.0f;
    
    // Inventory
    std::vector<BlockType> m_inventory;
    BlockType m_selected_block = BlockType::Stone;
    int32_t m_selected_slot = 0;
    
    // Physics state
    bool m_on_ground = false;
    bool m_is_flying = false;
    Vec3 m_last_position{0, 0, 0};
    
    Config m_config;
    Stats m_stats;
    
    // Helper methods
    void UpdatePhysics(float dt);
    void UpdateLook(const Action& action);
    void UpdateMovement(const Action& action, float dt);
    void UpdateInteraction(const Action& action);
    
    Observation EncodeVisualObservation() const;
    Observation::VectorObs EncodeVectorObservation() const;
    Observation::RayObs EncodeRayObservation() const;
};

// Environment configuration
struct EnvironmentConfig {
    // World settings
    int32_t chunk_size = 32;
    int32_t world_height = 256;
    int32_t render_distance = 8;
    
    // Agent settings
    Agent::Config agent_config;
    
    // Observation settings
    struct ObservationConfig {
        bool enable_visual = true;
        bool enable_vector = true;
        bool enable_ray = true;
        
        // Visual observation
        std::vector<int32_t> visual_size = {64, 64, 64}; // W, H, D
        int32_t visual_channels = 1; // Block IDs
        std::string visual_encoding = "uint8"; // uint8, float32
        
        // Ray observation
        int32_t num_rays = 64;
        float ray_max_distance = 16.0f;
        std::string ray_pattern = "radial"; // radial, grid, random
        
        // Vector observation size (calculated automatically)
        int32_t vector_size = 16; // Position, velocity, health, etc.
    } observation;
    
    // Reward settings
    struct RewardConfig {
        float survival_reward = 0.001f;
        float death_penalty = -1.0f;
        float block_place_reward = 0.1f;
        float block_break_reward = 0.05f;
        float exploration_reward = 0.001f;
        float goal_completion_reward = 10.0f;
    } reward;
    
    // Episode settings
    int32_t max_episode_steps = 1000;
    bool auto_reset = true;
    float episode_timeout = 300.0f; // seconds
    
    // Performance settings
    bool use_gpu = true;
    bool async_world_gen = true;
    int32_t max_concurrent_envs = 256;
};

// Goal system for training
class Goal {
public:
    virtual ~Goal() = default;
    
    virtual std::string GetName() const = 0;
    virtual std::string GetDescription() const = 0;
    
    virtual bool IsCompleted(const Agent& agent) const = 0;
    virtual float GetProgress(const Agent& agent) const = 0;
    virtual float GetReward(const Agent& agent, bool completed) const = 0;
    
    virtual void Reset() = 0;
    virtual void Update(float dt) {}
    
    // Goal parameters
    virtual std::map<std::string, float> GetParameters() const { return {}; }
    virtual void SetParameters(const std::map<std::string, float>& params) {}
};

// Concrete goal implementations
class SurvivalGoal : public Goal {
public:
    explicit SurvivalGoal(float duration) : m_target_duration(duration) {}
    
    std::string GetName() const override { return "Survival"; }
    std::string GetDescription() const override { return "Stay alive for " + std::to_string(m_target_duration) + " seconds"; }
    
    bool IsCompleted(const Agent& agent) const override;
    float GetProgress(const Agent& agent) const override;
    float GetReward(const Agent& agent, bool completed) const override;
    void Reset() override;
    
private:
    float m_target_duration;
    float m_start_time = 0.0f;
};

class PlaceBlocksGoal : public Goal {
public:
    explicit PlaceBlocksGoal(int32_t target_count) : m_target_count(target_count) {}
    
    std::string GetName() const override { return "PlaceBlocks"; }
    std::string GetDescription() const override { return "Place " + std::to_string(m_target_count) + " blocks"; }
    
    bool IsCompleted(const Agent& agent) const override;
    float GetProgress(const Agent& agent) const override;
    float GetReward(const Agent& agent, bool completed) const override;
    void Reset() override;
    
private:
    int32_t m_target_count;
    uint64_t m_initial_blocks_placed = 0;
};

class NavigationGoal : public Goal {
public:
    explicit NavigationGoal(const Vec3& target) : m_target_position(target) {}
    
    std::string GetName() const override { return "Navigation"; }
    std::string GetDescription() const override { return "Reach target position"; }
    
    bool IsCompleted(const Agent& agent) const override;
    float GetProgress(const Agent& agent) const override;
    float GetReward(const Agent& agent, bool completed) const override;
    void Reset() override;
    
private:
    Vec3 m_target_position;
    Vec3 m_initial_position{0, 0, 0};
    float m_completion_radius = 2.0f;
};

} // namespace voxelvk