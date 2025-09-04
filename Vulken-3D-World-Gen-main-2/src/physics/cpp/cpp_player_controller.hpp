#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <chrono>
#include <tuple>
#include <utility>
#include "aabb.hpp"
#include "capsule.hpp"
#include "collision_utils.hpp"
#include "voxel_solid.hpp"

namespace voxelvk::physics {

// Forward declarations to avoid circular dependencies are no longer needed
// because we include the concrete headers above. Keep namespace alias for clarity.

/**
 * High-performance C++ Player Controller
 * Direct port of Python PlayerController and PlayerControllerCapsule
 * with enhanced features and optimizations
 */
class CppPlayerController {
public:
    
    /**
     * Configuration for physics behavior
     * Matches Python controller parameters exactly
     */
    struct Config {
        // Movement parameters
        float gravity = 28.0f;
        float max_speed = 11.0f;
        float sprint_multiplier = 1.6f;
        float jump_speed = 9.5f;
        float step_height = 0.5f;
        float acceleration = 50.0f;
        float air_acceleration = 10.0f;
        float friction = 12.0f;
        
        // Collision resolution parameters
        int max_collision_iterations = 8;
        int max_blocks_checked = 1000;
        float max_correction_per_iteration = 2.0f;
        int search_radius_limit = 16;
        
        // Numeric stability
        float epsilon = 1e-6f;
        float binary_search_iterations = 8;
        float sweep_step_size = 0.1f;
        float ground_normal_threshold = 0.7f;
        
        // Collision shape
        glm::vec3 aabb_half_extent{0.3f, 0.9f, 0.3f};  // AABB mode dimensions
        float capsule_radius = 0.3f;                    // Capsule mode radius
        float capsule_half_height = 0.9f;               // Capsule mode half-height
        
        static Config defaultConfig() {
            return Config{};
        }
    };
    
    /**
     * Input state for movement
     * Matches Python input format exactly
     */
    struct InputState {
        bool forward = false, backward = false;
        bool left = false, right = false;
        bool jump = false, sprint = false;
        bool up = false, down = false;  // For creative mode
        
        // Convenience methods
        bool hasMovementInput() const {
            return forward || backward || left || right || up || down;
        }
        
        glm::vec2 getMovementVector() const {
            return glm::vec2(
                (right ? 1.0f : 0.0f) - (left ? 1.0f : 0.0f),
                (forward ? 1.0f : 0.0f) - (backward ? 1.0f : 0.0f)
            );
        }
    };
    
    /**
     * Debug information for performance monitoring and debugging
     */
    struct DebugInfo {
        int blocks_checked_last_frame = 0;
        int collision_iterations_last_frame = 0;
        float total_correction_applied = 0.0f;
        bool position_clamped = false;
        std::vector<glm::ivec3> checked_blocks;
        glm::vec3 velocity_before_collision{0.0f};
        glm::vec3 velocity_after_collision{0.0f};
        bool step_up_activated = false;
        float frame_time_ms = 0.0f;
    };
    
    /**
     * Collision mode selection
     */
    enum class CollisionMode {
        AABB,    // Faster, less accurate (matches Python PlayerController)
        CAPSULE  // Slower, more accurate (matches Python PlayerControllerCapsule)
    };

public:
    /**
     * Constructor
     * @param world_interface Interface to world for block queries
     * @param spawn_position Initial player position
     * @param collision_mode Whether to use AABB or Capsule collision
     * @param config Physics configuration
     */
    CppPlayerController(std::shared_ptr<collision_utils::WorldInterface> world_interface,
                       const glm::vec3& spawn_position,
                       CollisionMode collision_mode = CollisionMode::CAPSULE,
                       const Config& config = Config::defaultConfig());

    // Core update loop
    void update(float dt, const glm::vec3& camera_forward, const glm::vec3& camera_right);
    void setInput(const InputState& input) { input_ = input; }
    
    // State queries
    const glm::vec3& getPosition() const { return position_; }
    const glm::vec3& getVelocity() const { return velocity_; }
    bool isOnGround() const { return on_ground_; }
    CollisionMode getCollisionMode() const { return collision_mode_; }
    
    // Configuration
    void setConfig(const Config& config) { config_ = config; }
    const Config& getConfig() const { return config_; }
    void switchCollisionMode(CollisionMode mode);
    
    // Position manipulation
    void setPosition(const glm::vec3& position);
    void teleport(const glm::vec3& position);  // Instant move without collision
    
    // Physics state
    void setVelocity(const glm::vec3& velocity) { velocity_ = velocity; }
    void addVelocity(const glm::vec3& delta_velocity) { velocity_ += delta_velocity; }
    void setOnGround(bool on_ground) { on_ground_ = on_ground; }
    
    // Debug and profiling
    const DebugInfo& getDebugInfo() const { return debug_info_; }
    void resetDebugInfo();
    
    // Collision shape queries
    AABB getCurrentAABB() const;
    Capsule getCurrentCapsule() const;
    
    // Utility methods
    bool canReachPosition(const glm::vec3& target_position) const;
    glm::vec3 findNearestSafePosition(const glm::vec3& desired_position) const;
    float getDistanceToGround() const;

private:
    // Core state
    std::shared_ptr<collision_utils::WorldInterface> world_;
    Config config_;
    glm::vec3 position_;
    glm::vec3 velocity_;
    bool on_ground_;
    InputState input_;
    CollisionMode collision_mode_;
    
    // Debug and profiling
    mutable DebugInfo debug_info_;
    
    // Core physics methods
    void updateMovement(float dt, const glm::vec3& camera_forward, const glm::vec3& camera_right);
    void applyGravity(float dt);
    void handleJump();
    void applyFriction(float dt);
    void moveAndCollide(float dt);
    
    // AABB collision methods (matches Python PlayerController exactly)
    void moveAndCollideAABB(float dt);
    std::pair<glm::vec3, bool> sweepAxis(const glm::vec3& start_pos, int axis, float delta);
    bool canOccupyAABB(const glm::vec3& center) const;
    bool aabbVoxelOverlap(const AABB& aabb, int x, int y, int z) const;
    
    // Capsule collision methods (matches Python PlayerControllerCapsule exactly)
    void moveAndCollideCapsule(float dt);
    bool canOccupyCapsule(const glm::vec3& center) const;
    
    // Step-up logic (matches Python implementation)
    void attemptStepUp(float dt, const glm::vec3& original_position);
    
    // Utilities
    bool isBlockSolid(int x, int y, int z) const;
    void validateAndClampPosition();
    void updateDebugInfo(bool step_up_used = false) const;
    
    // Movement calculation helpers
    glm::vec3 calculateWishDirection(const glm::vec3& camera_forward, 
                                    const glm::vec3& camera_right) const;
    float calculateTargetSpeed() const;
    float getEffectiveAcceleration() const;
    
    // Performance monitoring
    std::chrono::high_resolution_clock::time_point frame_start_time_;
    void beginFrameTiming();
    void endFrameTiming();
};

/**
 * Utility functions for player controller integration
 */
namespace player_utils {
    
    /**
     * Create a world interface adapter for any world manager type
     * Template function that works with different world manager implementations
     */
    template<typename WorldManagerType>
    std::shared_ptr<collision_utils::WorldInterface> 
    createWorldInterface(WorldManagerType* world_manager) {
        class WorldAdapter : public collision_utils::WorldInterface {
        public:
            explicit WorldAdapter(WorldManagerType* wm) : world_manager_(wm) {}
            
            uint16_t getBlockAtWorldPosition(float x, float y, float z) const override {
                if (!world_manager_) return 0;
                try {
                    return world_manager_->GetBlock(static_cast<int>(x), 
                                                   static_cast<int>(y), 
                                                   static_cast<int>(z));
                } catch (...) {
                    return 0;  // Air on error
                }
            }
            
            bool isBlockSolid(uint16_t block_type) const override {
                return is_solid(block_type);
            }
            
        private:
            WorldManagerType* world_manager_;
        };
        
        return std::make_shared<WorldAdapter>(world_manager);
    }
    
    /**
     * Performance comparison utilities
     */
    struct PerformanceMetrics {
        float average_frame_time_ms = 0.0f;
        float average_blocks_checked = 0.0f;
        float average_collision_iterations = 0.0f;
        int total_frames_measured = 0;
        CppPlayerController::CollisionMode mode_tested = CppPlayerController::CollisionMode::CAPSULE;
    };
    
    class PerformanceProfiler {
    public:
        void recordFrame(const CppPlayerController& controller);
        PerformanceMetrics getMetrics() const;
        void reset();
        void printReport() const;
        
    private:
        std::vector<float> frame_times_;
        std::vector<int> blocks_checked_;
        std::vector<int> collision_iterations_;
        CppPlayerController::CollisionMode current_mode_;
    };
    
    // Convenience functions for common operations
    glm::vec3 getPlayerSpawnPosition(const collision_utils::WorldInterface& world,
                                    const glm::vec3& desired_position);
    
    bool isPositionSafeForPlayer(const collision_utils::WorldInterface& world,
                                const glm::vec3& position,
                                const CppPlayerController::Config& config);
    
    // Integration helpers for existing codebase
    void copyStateFromPythonController(CppPlayerController& cpp_controller,
                                      const glm::vec3& python_position,
                                      const glm::vec3& python_velocity,
                                      bool python_on_ground);
    
    std::tuple<glm::vec3, glm::vec3, bool> 
    extractStateForPythonController(const CppPlayerController& cpp_controller);
}

} // namespace voxelvk::physics