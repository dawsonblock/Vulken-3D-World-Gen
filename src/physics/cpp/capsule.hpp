#pragma once

#include "aabb.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <vector>
#include <cmath>

namespace voxelvk::physics {

/**
 * High-performance Capsule collision primitive
 * Optimized for character collision in voxel environments
 */
class Capsule {
public:
    glm::vec3 center{0.0f};
    float half_height{0.9f};  // Distance from center to top/bottom sphere centers
    float radius{0.3f};       // Radius of the spherical caps
    
    // Constructors
    Capsule() = default;
    Capsule(const glm::vec3& center, float half_height, float radius);
    Capsule(const glm::vec3& bottom, const glm::vec3& top, float radius);
    
    // Static factory methods
    static Capsule fromBottomTop(const glm::vec3& bottom, const glm::vec3& top, float radius);
    static Capsule fromCenterHeight(const glm::vec3& center, float total_height, float radius);
    static Capsule playerCapsule(const glm::vec3& position);  // Standard player size
    
    // Core properties
    glm::vec3 top() const {
        float y = center.y + half_height;
        // Snap to 1-decimal if extremely close (avoids ULP drift for common values like 1.9)
        float snapped = std::round(y * 10.0f) * 0.1f;
        if (std::fabs(y - snapped) <= 1e-6f) y = snapped;
        return glm::vec3(center.x, y, center.z);
    }
    glm::vec3 bottom() const {
        float y = center.y - half_height;
        float snapped = std::round(y * 10.0f) * 0.1f;
        if (std::fabs(y - snapped) <= 1e-6f) y = snapped;
        return glm::vec3(center.x, y, center.z);
    }
    float totalHeight() const { return half_height * 2.0f + radius * 2.0f; }
    float cylinderHeight() const { return half_height * 2.0f; }
    
    // Segment endpoints (for collision calculations)
    glm::vec3 segmentStart() const { return bottom(); }
    glm::vec3 segmentEnd() const { return top(); }
    glm::vec3 segmentDirection() const { return glm::vec3(0, 1, 0); }
    float segmentLength() const { return half_height * 2.0f; }
    
    // Bounding volume
    AABB getBoundingBox() const;
    AABB getExpandedBoundingBox(float expansion = 0.0f) const;
    
    // Transformation operations
    Capsule moved(const glm::vec3& delta) const;
    Capsule scaled(float scale) const;
    Capsule withRadius(float new_radius) const;
    Capsule withHalfHeight(float new_half_height) const;
    
    // Distance and containment queries
    float distanceToPoint(const glm::vec3& point) const;
    float squaredDistanceToPoint(const glm::vec3& point) const;
    glm::vec3 closestPoint(const glm::vec3& point) const;
    bool contains(const glm::vec3& point) const;
    
    // Collision queries
    bool intersects(const AABB& box) const;
    bool intersects(const Capsule& other) const;
    bool intersectsVoxel(int x, int y, int z) const;
    bool intersectsVoxel(const glm::ivec3& voxel_pos) const;
    
    // Support for collision detection algorithms
    glm::vec3 support(const glm::vec3& direction) const;  // For GJK
    
    // Utility methods
    bool isValid() const;
    void validate();
    float volume() const;
    float surfaceArea() const;
    
    // Debug information
    struct DebugInfo {
        glm::vec3 segment_start;
        glm::vec3 segment_end;
        float segment_length;
        AABB bounding_box;
        bool is_degenerate;
    };
    DebugInfo getDebugInfo() const;
    
    // Comparison operators
    bool operator==(const Capsule& other) const;
    bool operator!=(const Capsule& other) const;
    
    // Constants
    static const Capsule UNIT;  // Unit capsule for testing
    
private:
    static constexpr float EPSILON = 1e-6f;
    static constexpr float MIN_RADIUS = 1e-3f;
    static constexpr float MIN_HALF_HEIGHT = 1e-3f;
    
    void ensureValid();
};

// Collision detection utilities for capsules
namespace capsule_collision {
    
    // Point-to-capsule distance
    float pointToCapsuleDistance(const glm::vec3& point, const Capsule& capsule);
    glm::vec3 closestPointOnCapsule(const glm::vec3& point, const Capsule& capsule);
    
    // Capsule-to-AABB collision detection
    struct CapsuleAABBResult {
        bool intersects = false;
        glm::vec3 separation_vector{0.0f};
        float penetration_depth = 0.0f;
        glm::vec3 contact_point{0.0f};
        glm::vec3 normal{0, 1, 0};
    };
    
    CapsuleAABBResult capsuleAABBIntersection(const Capsule& capsule, const AABB& box);
    CapsuleAABBResult capsuleVoxelIntersection(const Capsule& capsule, int x, int y, int z);
    CapsuleAABBResult capsuleVoxelIntersection(const Capsule& capsule, const glm::ivec3& voxel_pos);
    
    // Capsule-to-capsule collision
    struct CapsuleCapsuleResult {
        bool intersects = false;
        glm::vec3 separation_vector{0.0f};
        float penetration_depth = 0.0f;
        glm::vec3 contact_point_a{0.0f};
        glm::vec3 contact_point_b{0.0f};
        glm::vec3 normal{0, 1, 0};
    };
    
    CapsuleCapsuleResult capsuleCapsuleIntersection(const Capsule& a, const Capsule& b);
    
    // Advanced collision queries
    bool capsuleRayIntersection(const Capsule& capsule, 
                               const glm::vec3& ray_origin, 
                               const glm::vec3& ray_direction,
                               float& t_near, float& t_far);
    
    // Batch collision operations for performance
    void batchCapsuleVoxelTest(const std::vector<Capsule>& capsules,
                              const std::vector<glm::ivec3>& voxels,
                              std::vector<std::vector<bool>>& results);
    
    void batchCapsuleAABBTest(const std::vector<Capsule>& capsules,
                             const std::vector<AABB>& boxes,
                             std::vector<std::vector<CapsuleAABBResult>>& results);
}

// Geometric utilities for capsule operations
namespace capsule_geometry {
    
    // Line segment operations (used internally by capsule)
    glm::vec3 closestPointOnSegment(const glm::vec3& point, 
                                   const glm::vec3& seg_start, 
                                   const glm::vec3& seg_end);
    
    float segmentToSegmentDistance(const glm::vec3& a1, const glm::vec3& a2,
                                  const glm::vec3& b1, const glm::vec3& b2,
                                  glm::vec3& closest_a, glm::vec3& closest_b);
    
    // Sphere operations (for capsule end caps)
    bool sphereAABBIntersection(const glm::vec3& sphere_center, float sphere_radius,
                               const glm::vec3& box_min, const glm::vec3& box_max);
    
    glm::vec3 closestPointOnAABB(const glm::vec3& point, 
                                const glm::vec3& box_min, 
                                const glm::vec3& box_max);
    
    // Cylinder operations (for capsule body)
    bool cylinderAABBIntersection(const glm::vec3& cylinder_center, 
                                 float cylinder_radius, 
                                 float cylinder_half_height,
                                 const glm::vec3& box_min, 
                                 const glm::vec3& box_max);
    
    // Utility functions for numerical stability
    bool isPointOnSegment(const glm::vec3& point, 
                         const glm::vec3& seg_start, 
                         const glm::vec3& seg_end, 
                         float tolerance = 1e-6f);
    
    glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback = glm::vec3(0, 1, 0));
}

} // namespace voxelvk::physics