#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include "aabb.hpp"
#include "capsule.hpp"
#include <vector>

namespace voxelvk::physics {

/**
 * Low-level collision detection utilities
 * Direct ports of Python collision functions for exact behavioral equivalence
 */
namespace collision_utils {

    /**
     * Find closest point on AABB to a given point
     * Direct port of Python closest_point_on_aabb function
     */
    glm::vec3 closestPointOnAABB(const glm::vec3& point, const glm::vec3& min_point, const glm::vec3& max_point);

    /**
     * Find closest point on line segment with numeric stability
     * Direct port of Python closest_point_on_segment function
     */
    glm::vec3 closestPointOnSegment(const glm::vec3& point, const glm::vec3& seg_a, const glm::vec3& seg_b);

    /**
     * Capsule-box penetration test with full validation
     * Direct port of Python capsule_box_penetration function
     * 
     * Returns:
     * - hit: whether penetration occurred
     * - normal: penetration normal (normalized)
     * - penetration_depth: how deep the penetration is
     */
    struct PenetrationResult {
        bool hit = false;
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        float penetration_depth = 0.0f;
    };

    PenetrationResult capsuleBoxPenetration(const Capsule& capsule, 
                                          const glm::vec3& box_min, 
                                          const glm::vec3& box_max);

    /**
     * Block solidity checking interface
     * Interface to world manager for block type queries
     */
    class WorldInterface {
    public:
        virtual ~WorldInterface() = default;
        virtual uint16_t getBlockAtWorldPosition(float x, float y, float z) const = 0;
        virtual bool isBlockSolid(uint16_t block_type) const = 0;
    };

    /**
     * Resolve capsule collision with world
     * Direct port of Python resolve_capsule_world function
     * 
     * Returns:
     * - total_offset: cumulative position correction applied
     * - is_on_ground: whether character is considered grounded
     */
    struct CapsuleResolutionResult {
        glm::vec3 total_offset{0.0f};
        bool is_on_ground = false;
        int blocks_checked = 0;
        int iterations_used = 0;
        bool position_clamped = false;
    };

    CapsuleResolutionResult resolveCapsuleWorld(Capsule& capsule, 
                                              const WorldInterface& world,
                                              int max_iterations = 8,
                                              int max_blocks_checked = 1000,
                                              float max_correction_per_iteration = 2.0f,
                                              int search_radius_limit = 16);

    /**
     * Configuration for collision resolution
     */
    struct CollisionConfig {
        // Iteration limits
        int max_collision_iterations = 8;
        int max_blocks_checked = 1000;
        
        // Safety limits
        float max_correction_per_iteration = 2.0f;
        int search_radius_limit = 16;
        
        // Ground detection
        float ground_normal_threshold = 0.7f;
    bool enable_pre_fall = false; // If true, simulate downward settling before resolving
        
        // Numeric stability
        float epsilon = 1e-6f;
        float penetration_epsilon = 1e-6f;
        float distance_epsilon = 1e-8f;
        float normal_epsilon = 1e-9f;
        
        // Validation thresholds
        float max_position_magnitude = 1e6f;
        float min_capsule_radius = 0.1f;
        float min_capsule_half_height = 0.1f;
    };

    /**
     * Advanced collision resolution with full configuration
     */
    CapsuleResolutionResult resolveCapsuleWorldAdvanced(Capsule& capsule, 
                                                       const WorldInterface& world,
                                                       const CollisionConfig& config = CollisionConfig{});

    /**
     * Validate and clamp capsule parameters
     * Ensures capsule remains within valid bounds
     */
    bool validateAndClampCapsule(Capsule& capsule, const CollisionConfig& config = CollisionConfig{});

    /**
     * Check if a position is safe (not inside solid blocks)
     */
    bool isCapsulePositionSafe(const Capsule& capsule, const WorldInterface& world);

    /**
     * Find safe position near a given position
     * Returns the closest safe position to the input position
     */
    glm::vec3 findSafePosition(const glm::vec3& desired_position, 
                              const Capsule& capsule_template,
                              const WorldInterface& world,
                              float search_radius = 5.0f);

    /**
     * Batch collision testing for multiple capsules
     */
    void batchResolveCapsules(std::vector<Capsule>& capsules,
                             const WorldInterface& world,
                             std::vector<CapsuleResolutionResult>& results,
                             const CollisionConfig& config = CollisionConfig{});

    /**
     * Performance profiling utilities
     */
    struct CollisionProfileData {
        float total_time_ms = 0.0f;
        int total_iterations = 0;
        int total_blocks_checked = 0;
        int total_capsules_processed = 0;
        float average_time_per_capsule = 0.0f;
        float average_iterations_per_capsule = 0.0f;
    };

    class CollisionProfiler {
    public:
        void beginFrame();
        void endFrame();
        void recordCapsuleResolution(const CapsuleResolutionResult& result, float time_ms);
        CollisionProfileData getFrameData() const;
        void reset();

    private:
        CollisionProfileData current_frame_data_;
    };

    /**
     * Debug utilities for collision visualization
     */
    struct DebugCollisionInfo {
        std::vector<glm::ivec3> checked_blocks;
        std::vector<PenetrationResult> penetrations;
        glm::vec3 original_position{0.0f};
        glm::vec3 final_position{0.0f};
        glm::vec3 total_correction{0.0f};
        int iteration_count = 0;
        bool converged = false;
    };

    DebugCollisionInfo debugResolveCapsuleWorld(Capsule& capsule, 
                                               const WorldInterface& world,
                                               const CollisionConfig& config = CollisionConfig{});

} // namespace collision_utils

} // namespace voxelvk::physics