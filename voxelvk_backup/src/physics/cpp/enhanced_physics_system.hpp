#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include "aabb.hpp"
#include "capsule.hpp"

namespace voxelvk::physics {

/**
 * Enhanced High-Performance Physics System
 * Adds advanced features, optimizations, and multi-threading support
 */

// Forward declarations
class WorldInterface;
struct PhysicsConfig;

/**
 * Advanced collision detection with spatial optimization
 */
class SpatialHashGrid {
public:
    SpatialHashGrid(float cell_size = 4.0f);
    
    void clear();
    void insert(uint32_t entity_id, const AABB& bounds);
    void remove(uint32_t entity_id);
    void update(uint32_t entity_id, const AABB& old_bounds, const AABB& new_bounds);
    
    std::vector<uint32_t> query(const AABB& bounds) const;
    std::vector<uint32_t> queryRadius(const glm::vec3& center, float radius) const;
    
    // Debug info
    size_t getEntityCount() const { return entity_positions_.size(); }
    size_t getCellCount() const { return grid_cells_.size(); }
    
private:
    float cell_size_;
    std::unordered_map<uint64_t, std::vector<uint32_t>> grid_cells_;
    std::unordered_map<uint32_t, std::vector<uint64_t>> entity_positions_;
    
    uint64_t hashPosition(int x, int y, int z) const;
    std::vector<uint64_t> getCells(const AABB& bounds) const;
};

/**
 * Enhanced collision response system
 */
struct CollisionEvent {
    uint32_t entity_a;
    uint32_t entity_b;
    glm::vec3 contact_point;
    glm::vec3 normal;
    float penetration_depth;
    float relative_velocity;
    std::chrono::high_resolution_clock::time_point timestamp;
};

using CollisionCallback = std::function<void(const CollisionEvent&)>;

/**
 * Physics material system for different collision behaviors
 */
struct PhysicsMaterial {
    float restitution = 0.1f;      // Bounciness (0 = no bounce, 1 = perfect bounce)
    float friction = 0.7f;         // Surface friction coefficient
    float density = 1.0f;          // Mass density
    float drag = 0.01f;            // Air resistance
    bool trigger = false;          // If true, doesn't resolve collision but fires events
    
    static PhysicsMaterial stone() { return {0.1f, 0.8f, 2.5f, 0.01f}; }
    static PhysicsMaterial wood() { return {0.3f, 0.6f, 0.8f, 0.02f}; }
    static PhysicsMaterial ice() { return {0.05f, 0.1f, 0.9f, 0.005f}; }
    static PhysicsMaterial mud() { return {0.0f, 0.9f, 1.2f, 0.1f}; }
    static PhysicsMaterial triggerMaterial() { return {0.0f, 0.0f, 0.0f, 0.0f, true}; }
};

/**
 * Physics body for entities with enhanced properties
 */
class PhysicsBody {
public:
    uint32_t entity_id;
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 acceleration{0.0f};
    
    union CollisionShape {
        AABB aabb;
        Capsule capsule;
        
        CollisionShape() { new(&aabb) AABB(); }
        ~CollisionShape() {}
    } shape;
    
    enum class ShapeType { AABB, CAPSULE } shape_type = ShapeType::CAPSULE;
    
    PhysicsMaterial material;
    float mass = 1.0f;
    float inverse_mass = 1.0f;
    
    // State flags
    bool is_static = false;
    bool is_kinematic = false;  // Moves but doesn't respond to forces
    bool is_sleeping = false;   // Optimization: skip if not moving
    bool is_grounded = false;
    bool enable_gravity = true;
    
    // Collision filtering
    uint32_t collision_layer = 1;
    uint32_t collision_mask = 0xFFFFFFFF;
    
    // Sleep optimization
    float sleep_threshold = 0.01f;
    float sleep_timer = 0.0f;
    
public:
    PhysicsBody(uint32_t id, const glm::vec3& pos, ShapeType type = ShapeType::CAPSULE);
    
    void setMass(float new_mass);
    void addForce(const glm::vec3& force);
    void addImpulse(const glm::vec3& impulse);
    void setVelocity(const glm::vec3& vel) { velocity = vel; }
    
    AABB getBounds() const;
    glm::vec3 getCenter() const;
    float getVolume() const;
    
    void updateSleepState(float dt);
    void wakeUp() { is_sleeping = false; sleep_timer = 0.0f; }
    
    bool canCollideWith(const PhysicsBody& other) const;
};

/**
 * Advanced physics configuration
 */
struct PhysicsConfig {
    // World parameters
    glm::vec3 gravity{0.0f, -28.0f, 0.0f};
    float fixed_timestep = 1.0f / 120.0f;  // 120 Hz physics
    int max_substeps = 8;
    
    // Collision resolution
    int collision_iterations = 8;
    int position_iterations = 4;
    float penetration_slop = 0.01f;
    float baumgarte_factor = 0.2f;
    
    // Performance settings
    bool use_spatial_hashing = true;
    bool use_sleeping = true;
    bool use_continuous_collision = false;
    int max_bodies_per_thread = 64;
    
    // Debug settings
    bool debug_draw_bounds = false;
    bool debug_draw_contacts = false;
    bool profile_performance = true;
};

/**
 * Performance profiler for physics system
 */
class PhysicsProfiler {
public:
    struct FrameData {
        float total_time_ms = 0.0f;
        float collision_detection_ms = 0.0f;
        float collision_resolution_ms = 0.0f;
        float integration_ms = 0.0f;
        
        int active_bodies = 0;
        int sleeping_bodies = 0;
        int collision_pairs = 0;
        int collision_events = 0;
        
        int spatial_queries = 0;
        int broad_phase_pairs = 0;
        int narrow_phase_tests = 0;
    };
    
    void beginFrame();
    void endFrame();
    void beginPhase(const std::string& phase);
    void endPhase(const std::string& phase);
    
    const FrameData& getCurrentFrame() const { return current_frame_; }
    FrameData& getCurrentFrame() { return current_frame_; }
    FrameData getAverageFrame(int num_frames = 60) const;
    
    void printReport() const;
    void reset();
    
private:
    FrameData current_frame_;
    std::vector<FrameData> frame_history_;
    std::chrono::high_resolution_clock::time_point frame_start_;
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> phase_starts_;
    mutable std::mutex data_mutex_;
};

/**
 * Multi-threaded physics world
 */
class EnhancedPhysicsWorld {
public:
    EnhancedPhysicsWorld(const PhysicsConfig& config = PhysicsConfig{});
    ~EnhancedPhysicsWorld();
    
    // Body management
    uint32_t addBody(const glm::vec3& position, 
                     PhysicsBody::ShapeType shape_type = PhysicsBody::ShapeType::CAPSULE,
                     const PhysicsMaterial& material = PhysicsMaterial{});
    void removeBody(uint32_t body_id);
    PhysicsBody* getBody(uint32_t body_id);
    const PhysicsBody* getBody(uint32_t body_id) const;
    
    // World interface
    void setWorldInterface(std::shared_ptr<WorldInterface> world) { world_interface_ = world; }
    std::shared_ptr<WorldInterface> getWorldInterface() const { return world_interface_; }
    
    // Simulation
    void step(float dt);
    void fixedStep();  // Use fixed timestep
    
    // Event system
    void setCollisionCallback(CollisionCallback callback) { collision_callback_ = callback; }
    
    // Queries
    std::vector<uint32_t> queryAABB(const AABB& bounds) const;
    std::vector<uint32_t> queryRadius(const glm::vec3& center, float radius) const;
    bool raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance,
                uint32_t& hit_body, glm::vec3& hit_point, glm::vec3& hit_normal) const;
    
    // Configuration
    void setConfig(const PhysicsConfig& config) { config_ = config; }
    const PhysicsConfig& getConfig() const { return config_; }
    
    // Debug and profiling
    const PhysicsProfiler& getProfiler() const { return profiler_; }
    size_t getBodyCount() const { return bodies_.size(); }
    size_t getActiveBodyCount() const;
    
    // Threading control
    void setThreadCount(int count);
    int getThreadCount() const { return static_cast<int>(worker_threads_.size()); }
    
private:
    PhysicsConfig config_;
    std::unordered_map<uint32_t, std::unique_ptr<PhysicsBody>> bodies_;
    std::shared_ptr<WorldInterface> world_interface_;
    std::unique_ptr<SpatialHashGrid> spatial_grid_;
    
    // Threading
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> should_stop_{false};
    std::mutex bodies_mutex_;
    
    // Timing
    float accumulator_ = 0.0f;
    std::chrono::high_resolution_clock::time_point last_time_;
    
    // Events and profiling
    CollisionCallback collision_callback_;
    PhysicsProfiler profiler_;  // Remove mutable
    std::vector<CollisionEvent> collision_events_;
    
    // Internal methods
    void broadPhaseCollisionDetection(std::vector<std::pair<uint32_t, uint32_t>>& pairs);
    void narrowPhaseCollisionDetection(const std::vector<std::pair<uint32_t, uint32_t>>& pairs);
    void resolveCollisions();
    void integrateVelocities(float dt);
    void integratePositions(float dt);
    void updateSpatialGrid();
    
    // Threading helpers
    void initializeThreads();
    void shutdownThreads();
    void processBodyChunk(std::vector<PhysicsBody*>& bodies, float dt, int start_idx, int end_idx);
    
    // Body ID generation
    std::atomic<uint32_t> next_body_id_{1};
};

/**
 * Simplified world interface
 */
class WorldInterface {
public:
    virtual ~WorldInterface() = default;
    
    virtual uint16_t getBlockAtWorldPosition(float x, float y, float z) const = 0;
    virtual bool isBlockSolid(uint16_t block_type) const = 0;
    
    // Enhanced interface for materials
    virtual PhysicsMaterial getBlockMaterial(uint16_t block_type) const {
        if (!isBlockSolid(block_type)) {
            return PhysicsMaterial{};
        }
        
        // Default material mapping
        switch (block_type) {
            case 1: return PhysicsMaterial::stone();
            case 4: return PhysicsMaterial::wood();
            default: return PhysicsMaterial{};
        }
    }
    
    // Batch queries for performance
    virtual void batchGetBlocks(const std::vector<glm::ivec3>& positions,
                               std::vector<uint16_t>& block_types) const {
        block_types.resize(positions.size());
        for (size_t i = 0; i < positions.size(); ++i) {
            const auto& pos = positions[i];
            block_types[i] = getBlockAtWorldPosition(static_cast<float>(pos.x),
                                                   static_cast<float>(pos.y),
                                                   static_cast<float>(pos.z));
        }
    }
};

/**
 * Utility functions for common physics operations
 */
namespace physics_utils {
    
    // Force and impulse helpers
    glm::vec3 calculateGravityForce(float mass, const glm::vec3& gravity = glm::vec3(0, -9.81f, 0));
    glm::vec3 calculateDragForce(const glm::vec3& velocity, float drag_coefficient);
    glm::vec3 calculateSpringForce(const glm::vec3& displacement, float spring_constant, float damping = 0.1f);
    
    // Collision response
    std::pair<glm::vec3, glm::vec3> calculateCollisionResponse(
        const PhysicsBody& body_a, const PhysicsBody& body_b,
        const glm::vec3& normal, const glm::vec3& contact_point);
    
    // Utility queries
    bool isPointInside(const glm::vec3& point, const PhysicsBody& body);
    float calculateContactArea(const PhysicsBody& body_a, const PhysicsBody& body_b);
    glm::vec3 calculateCenterOfMass(const std::vector<PhysicsBody*>& bodies);
    
    // Interpolation for smooth rendering
    glm::vec3 interpolatePosition(const glm::vec3& prev_pos, const glm::vec3& current_pos, float alpha);
    
    // Performance optimization helpers
    bool shouldUpdateBody(const PhysicsBody& body, float dt);
    float calculateSleepThreshold(const PhysicsBody& body);
    
    // Debug visualization data
    struct DebugDrawData {
        std::vector<std::pair<glm::vec3, glm::vec3>> lines;  // start, end
        std::vector<std::pair<glm::vec3, float>> spheres;     // center, radius
        std::vector<AABB> boxes;
        std::vector<std::pair<glm::vec3, glm::vec3>> contact_points;  // point, normal
    };
    
    DebugDrawData generateDebugDrawData(const EnhancedPhysicsWorld& world);
}

} // namespace voxelvk::physics