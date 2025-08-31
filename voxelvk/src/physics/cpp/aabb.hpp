#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <array>
#include <algorithm>
#include <vector>

namespace voxelvk::physics {

/**
 * High-performance Axis-Aligned Bounding Box implementation
 * Optimized for frequent collision queries in voxel environments
 */
class AABB {
public:
    glm::vec3 center{0.0f};
    glm::vec3 half_extent{0.5f};
    
    // Constructors
    AABB() = default;
    AABB(const glm::vec3& center, const glm::vec3& half_extent);
    AABB(const glm::vec3& min_point, const glm::vec3& max_point, bool from_min_max);
    
    // Static factory methods
    static AABB fromMinMax(const glm::vec3& min_point, const glm::vec3& max_point);
    static AABB fromCenterSize(const glm::vec3& center, const glm::vec3& size);
    static AABB fromPoints(const std::vector<glm::vec3>& points);
    
    // Core properties
    glm::vec3 min() const { return center - half_extent; }
    glm::vec3 max() const { return center + half_extent; }
    glm::vec3 size() const { return half_extent * 2.0f; }
    float volume() const;
    float surfaceArea() const;
    
    // Transformation operations
    AABB moved(const glm::vec3& delta) const;
    AABB expanded(const glm::vec3& expansion) const;
    AABB expanded(float expansion) const;
    AABB scaled(const glm::vec3& scale) const;
    AABB scaled(float scale) const;
    
    // Collision queries
    bool overlaps(const AABB& other) const;
    bool overlapsVoxel(int x, int y, int z) const;
    bool overlapsVoxel(const glm::ivec3& voxel_pos) const;
    bool contains(const glm::vec3& point) const;
    bool contains(const AABB& other) const;
    
    // Distance and projection
    float distanceToPoint(const glm::vec3& point) const;
    float squaredDistanceToPoint(const glm::vec3& point) const;
    glm::vec3 closestPoint(const glm::vec3& point) const;
    
    // Intersection operations
    AABB intersection(const AABB& other) const;
    AABB unionAABB(const AABB& other) const;
    bool intersect(const AABB& other, AABB& result) const;
    
    // Voxel grid operations (optimized for world queries)
    void getOverlappingVoxels(std::vector<glm::ivec3>& voxels) const;
    void getOverlappingVoxelsBounds(glm::ivec3& min_voxel, glm::ivec3& max_voxel) const;
    int countOverlappingVoxels() const;
    
    // Utility methods
    bool isValid() const;
    bool isEmpty() const;
    void validate();
    void clear();
    
    // Debug and visualization
    std::array<glm::vec3, 8> getCorners() const;
    glm::vec3 getCorner(int index) const;  // 0-7, binary encoding of corner
    
    // Comparison operators
    bool operator==(const AABB& other) const;
    bool operator!=(const AABB& other) const;
    
    // Constants
    static const AABB EMPTY;
    static const AABB INFINITE;
    static const AABB UNIT;  // Center at origin, size 1x1x1
    
private:
    static constexpr float EPSILON = 1e-6f;
    
    // Internal utilities
    void ensureValid();
};

// Non-member utility functions
namespace aabb_utils {
    
    // Batch operations for performance
    void batchOverlapTest(const std::vector<AABB>& boxes_a, 
                         const std::vector<AABB>& boxes_b,
                         std::vector<bool>& results);
    
    void batchVoxelOverlapTest(const std::vector<AABB>& boxes,
                              const std::vector<glm::ivec3>& voxels,
                              std::vector<std::vector<bool>>& results);
    
    // Spatial queries
    std::vector<int> findOverlapping(const std::vector<AABB>& boxes, const AABB& query);
    std::vector<int> findContaining(const std::vector<AABB>& boxes, const glm::vec3& point);
    
    // Construction helpers
    AABB boundingBox(const std::vector<AABB>& boxes);
    AABB boundingBox(const std::vector<glm::vec3>& points);
    
    // Intersection tests optimized for common cases
    bool fastVoxelOverlap(const AABB& box, int x, int y, int z);
    bool fastAABBOverlap(const AABB& a, const AABB& b);
}

} // namespace voxelvk::physics