#include "aabb.hpp"
#include <limits>
#include <cmath>
#include <algorithm>
#include <vector>

namespace voxelvk::physics {

// Static constants
const AABB AABB::EMPTY = AABB(glm::vec3(0.0f), glm::vec3(-1.0f));
const AABB AABB::INFINITE = AABB(glm::vec3(0.0f), glm::vec3(std::numeric_limits<float>::max()));
const AABB AABB::UNIT = AABB(glm::vec3(0.0f), glm::vec3(0.5f));

// Constructors
AABB::AABB(const glm::vec3& center, const glm::vec3& half_extent) 
    : center(center), half_extent(half_extent) {
    ensureValid();
}

AABB::AABB(const glm::vec3& min_point, const glm::vec3& max_point, bool from_min_max) {
    if (from_min_max) {
        center = (min_point + max_point) * 0.5f;
        half_extent = (max_point - min_point) * 0.5f;
        ensureValid();
    }
}

// Static factory methods
AABB AABB::fromMinMax(const glm::vec3& min_point, const glm::vec3& max_point) {
    return AABB(min_point, max_point, true);
}

AABB AABB::fromCenterSize(const glm::vec3& center, const glm::vec3& size) {
    return AABB(center, size * 0.5f);
}

AABB AABB::fromPoints(const std::vector<glm::vec3>& points) {
    if (points.empty()) {
        return AABB::EMPTY;
    }
    
    glm::vec3 min_pt = points[0];
    glm::vec3 max_pt = points[0];
    
    for (size_t i = 1; i < points.size(); ++i) {
        min_pt = glm::min(min_pt, points[i]);
        max_pt = glm::max(max_pt, points[i]);
    }
    
    return fromMinMax(min_pt, max_pt);
}

// Core properties
float AABB::volume() const {
    if (isEmpty()) return 0.0f;
    glm::vec3 size_vec = size();
    return size_vec.x * size_vec.y * size_vec.z;
}

float AABB::surfaceArea() const {
    if (isEmpty()) return 0.0f;
    glm::vec3 size_vec = size();
    return 2.0f * (size_vec.x * size_vec.y + size_vec.y * size_vec.z + size_vec.x * size_vec.z);
}

// Transformation operations
AABB AABB::moved(const glm::vec3& delta) const {
    return AABB(center + delta, half_extent);
}

AABB AABB::expanded(const glm::vec3& expansion) const {
    return AABB(center, half_extent + expansion);
}

AABB AABB::expanded(float expansion) const {
    return expanded(glm::vec3(expansion));
}

AABB AABB::scaled(const glm::vec3& scale) const {
    return AABB(center * scale, half_extent * scale);
}

AABB AABB::scaled(float scale) const {
    return scaled(glm::vec3(scale));
}

// Collision queries
bool AABB::overlaps(const AABB& other) const {
    if (isEmpty() || other.isEmpty()) return false;
    
    glm::vec3 min1 = min();
    glm::vec3 max1 = max();
    glm::vec3 min2 = other.min();
    glm::vec3 max2 = other.max();
    
    return !(max1.x <= min2.x || min1.x >= max2.x ||
             max1.y <= min2.y || min1.y >= max2.y ||
             max1.z <= min2.z || min1.z >= max2.z);
}

bool AABB::overlapsVoxel(int x, int y, int z) const {
    if (isEmpty()) return false;
    
    glm::vec3 aabb_min = min();
    glm::vec3 aabb_max = max();
    
    // Voxel occupies [x, x+1] x [y, y+1] x [z, z+1]
    return !(aabb_max.x <= static_cast<float>(x) || 
             aabb_min.x >= static_cast<float>(x + 1) ||
             aabb_max.y <= static_cast<float>(y) || 
             aabb_min.y >= static_cast<float>(y + 1) ||
             aabb_max.z <= static_cast<float>(z) || 
             aabb_min.z >= static_cast<float>(z + 1));
}

bool AABB::overlapsVoxel(const glm::ivec3& voxel_pos) const {
    return overlapsVoxel(voxel_pos.x, voxel_pos.y, voxel_pos.z);
}

bool AABB::contains(const glm::vec3& point) const {
    if (isEmpty()) return false;
    
    glm::vec3 aabb_min = min();
    glm::vec3 aabb_max = max();
    
    return point.x >= aabb_min.x && point.x <= aabb_max.x &&
           point.y >= aabb_min.y && point.y <= aabb_max.y &&
           point.z >= aabb_min.z && point.z <= aabb_max.z;
}

bool AABB::contains(const AABB& other) const {
    if (isEmpty() || other.isEmpty()) return false;
    
    glm::vec3 my_min = min();
    glm::vec3 my_max = max();
    glm::vec3 other_min = other.min();
    glm::vec3 other_max = other.max();
    
    return other_min.x >= my_min.x && other_max.x <= my_max.x &&
           other_min.y >= my_min.y && other_max.y <= my_max.y &&
           other_min.z >= my_min.z && other_max.z <= my_max.z;
}

// Distance and projection
float AABB::distanceToPoint(const glm::vec3& point) const {
    return std::sqrt(squaredDistanceToPoint(point));
}

float AABB::squaredDistanceToPoint(const glm::vec3& point) const {
    if (isEmpty()) return std::numeric_limits<float>::max();
    
    glm::vec3 closest = closestPoint(point);
    glm::vec3 diff = point - closest;
    return glm::dot(diff, diff);
}

glm::vec3 AABB::closestPoint(const glm::vec3& point) const {
    if (isEmpty()) return point;
    
    glm::vec3 aabb_min = min();
    glm::vec3 aabb_max = max();
    
    return glm::clamp(point, aabb_min, aabb_max);
}

// Intersection operations
AABB AABB::intersection(const AABB& other) const {
    if (isEmpty() || other.isEmpty()) return AABB::EMPTY;
    
    glm::vec3 min1 = min();
    glm::vec3 max1 = max();
    glm::vec3 min2 = other.min();
    glm::vec3 max2 = other.max();
    
    glm::vec3 intersection_min = glm::max(min1, min2);
    glm::vec3 intersection_max = glm::min(max1, max2);
    
    // Check if intersection is valid
    if (intersection_min.x > intersection_max.x ||
        intersection_min.y > intersection_max.y ||
        intersection_min.z > intersection_max.z) {
        return AABB::EMPTY;
    }
    
    return fromMinMax(intersection_min, intersection_max);
}

AABB AABB::unionAABB(const AABB& other) const {
    if (isEmpty()) return other;
    if (other.isEmpty()) return *this;
    
    glm::vec3 min1 = min();
    glm::vec3 max1 = max();
    glm::vec3 min2 = other.min();
    glm::vec3 max2 = other.max();
    
    glm::vec3 union_min = glm::min(min1, min2);
    glm::vec3 union_max = glm::max(max1, max2);
    
    return fromMinMax(union_min, union_max);
}

bool AABB::intersect(const AABB& other, AABB& result) const {
    result = intersection(other);
    return !result.isEmpty();
}

// Voxel grid operations
void AABB::getOverlappingVoxels(std::vector<glm::ivec3>& voxels) const {
    voxels.clear();
    if (isEmpty()) return;
    
    glm::ivec3 min_voxel, max_voxel;
    getOverlappingVoxelsBounds(min_voxel, max_voxel);
    
    for (int z = min_voxel.z; z <= max_voxel.z; ++z) {
        for (int y = min_voxel.y; y <= max_voxel.y; ++y) {
            for (int x = min_voxel.x; x <= max_voxel.x; ++x) {
                if (overlapsVoxel(x, y, z)) {
                    voxels.emplace_back(x, y, z);
                }
            }
        }
    }
}

void AABB::getOverlappingVoxelsBounds(glm::ivec3& min_voxel, glm::ivec3& max_voxel) const {
    if (isEmpty()) {
        min_voxel = max_voxel = glm::ivec3(0);
        return;
    }
    
    glm::vec3 aabb_min = min();
    glm::vec3 aabb_max = max();
    
    min_voxel.x = static_cast<int>(std::floor(aabb_min.x));
    min_voxel.y = static_cast<int>(std::floor(aabb_min.y));
    min_voxel.z = static_cast<int>(std::floor(aabb_min.z));
    
    max_voxel.x = static_cast<int>(std::floor(aabb_max.x));
    max_voxel.y = static_cast<int>(std::floor(aabb_max.y));
    max_voxel.z = static_cast<int>(std::floor(aabb_max.z));
}

int AABB::countOverlappingVoxels() const {
    if (isEmpty()) return 0;
    
    glm::ivec3 min_voxel, max_voxel;
    getOverlappingVoxelsBounds(min_voxel, max_voxel);
    
    int count = 0;
    for (int z = min_voxel.z; z <= max_voxel.z; ++z) {
        for (int y = min_voxel.y; y <= max_voxel.y; ++y) {
            for (int x = min_voxel.x; x <= max_voxel.x; ++x) {
                if (overlapsVoxel(x, y, z)) {
                    ++count;
                }
            }
        }
    }
    
    return count;
}

// Utility methods
bool AABB::isValid() const {
    return half_extent.x >= 0.0f && half_extent.y >= 0.0f && half_extent.z >= 0.0f &&
           std::isfinite(center.x) && std::isfinite(center.y) && std::isfinite(center.z) &&
           std::isfinite(half_extent.x) && std::isfinite(half_extent.y) && std::isfinite(half_extent.z);
}

bool AABB::isEmpty() const {
    return half_extent.x < 0.0f || half_extent.y < 0.0f || half_extent.z < 0.0f;
}

void AABB::validate() {
    ensureValid();
}

void AABB::clear() {
    *this = AABB::EMPTY;
}

// Debug and visualization
std::array<glm::vec3, 8> AABB::getCorners() const {
    glm::vec3 aabb_min = min();
    glm::vec3 aabb_max = max();
    
    return {{
        {aabb_min.x, aabb_min.y, aabb_min.z},  // 000
        {aabb_max.x, aabb_min.y, aabb_min.z},  // 100
        {aabb_min.x, aabb_max.y, aabb_min.z},  // 010
        {aabb_max.x, aabb_max.y, aabb_min.z},  // 110
        {aabb_min.x, aabb_min.y, aabb_max.z},  // 001
        {aabb_max.x, aabb_min.y, aabb_max.z},  // 101
        {aabb_min.x, aabb_max.y, aabb_max.z},  // 011
        {aabb_max.x, aabb_max.y, aabb_max.z}   // 111
    }};
}

glm::vec3 AABB::getCorner(int index) const {
    glm::vec3 aabb_min = min();
    glm::vec3 aabb_max = max();
    
    return glm::vec3(
        (index & 1) ? aabb_max.x : aabb_min.x,
        (index & 2) ? aabb_max.y : aabb_min.y,
        (index & 4) ? aabb_max.z : aabb_min.z
    );
}

// Comparison operators
bool AABB::operator==(const AABB& other) const {
    const float eps = EPSILON;
    return glm::all(glm::epsilonEqual(center, other.center, eps)) &&
           glm::all(glm::epsilonEqual(half_extent, other.half_extent, eps));
}

bool AABB::operator!=(const AABB& other) const {
    return !(*this == other);
}

// Private methods
void AABB::ensureValid() {
    // Clamp negative half extents to zero (creates empty AABB)
    half_extent = glm::max(half_extent, glm::vec3(0.0f));
    
    // Handle NaN/infinite values
    if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)) {
        center = glm::vec3(0.0f);
        half_extent = glm::vec3(-1.0f); // Make it empty
    }
    
    if (!std::isfinite(half_extent.x) || !std::isfinite(half_extent.y) || !std::isfinite(half_extent.z)) {
        half_extent = glm::vec3(-1.0f); // Make it empty
    }
}

// Non-member utility functions
namespace aabb_utils {

void batchOverlapTest(const std::vector<AABB>& boxes_a, 
                     const std::vector<AABB>& boxes_b,
                     std::vector<bool>& results) {
    const size_t size_a = boxes_a.size();
    const size_t size_b = boxes_b.size();
    results.resize(size_a * size_b);
    
    for (size_t i = 0; i < size_a; ++i) {
        for (size_t j = 0; j < size_b; ++j) {
            results[i * size_b + j] = boxes_a[i].overlaps(boxes_b[j]);
        }
    }
}

void batchVoxelOverlapTest(const std::vector<AABB>& boxes,
                          const std::vector<glm::ivec3>& voxels,
                          std::vector<std::vector<bool>>& results) {
    results.resize(boxes.size());
    
    for (size_t i = 0; i < boxes.size(); ++i) {
        results[i].resize(voxels.size());
        for (size_t j = 0; j < voxels.size(); ++j) {
            results[i][j] = boxes[i].overlapsVoxel(voxels[j]);
        }
    }
}

std::vector<int> findOverlapping(const std::vector<AABB>& boxes, const AABB& query) {
    std::vector<int> overlapping;
    for (size_t i = 0; i < boxes.size(); ++i) {
        if (boxes[i].overlaps(query)) {
            overlapping.push_back(static_cast<int>(i));
        }
    }
    return overlapping;
}

std::vector<int> findContaining(const std::vector<AABB>& boxes, const glm::vec3& point) {
    std::vector<int> containing;
    for (size_t i = 0; i < boxes.size(); ++i) {
        if (boxes[i].contains(point)) {
            containing.push_back(static_cast<int>(i));
        }
    }
    return containing;
}

AABB boundingBox(const std::vector<AABB>& boxes) {
    if (boxes.empty()) return AABB::EMPTY;
    
    AABB result = boxes[0];
    for (size_t i = 1; i < boxes.size(); ++i) {
        result = result.unionAABB(boxes[i]);
    }
    return result;
}

AABB boundingBox(const std::vector<glm::vec3>& points) {
    return AABB::fromPoints(points);
}

bool fastVoxelOverlap(const AABB& box, int x, int y, int z) {
    // Optimized version with minimal branching
    const glm::vec3 box_min = box.min();
    const glm::vec3 box_max = box.max();
    
    return (box_max.x > static_cast<float>(x)) && 
           (box_min.x < static_cast<float>(x + 1)) &&
           (box_max.y > static_cast<float>(y)) && 
           (box_min.y < static_cast<float>(y + 1)) &&
           (box_max.z > static_cast<float>(z)) && 
           (box_min.z < static_cast<float>(z + 1));
}

bool fastAABBOverlap(const AABB& a, const AABB& b) {
    // Optimized version using center/half-extent directly
    const glm::vec3 diff = glm::abs(a.center - b.center);
    const glm::vec3 combined_extent = a.half_extent + b.half_extent;
    
    return (diff.x <= combined_extent.x) && 
           (diff.y <= combined_extent.y) && 
           (diff.z <= combined_extent.z);
}

} // namespace aabb_utils

} // namespace voxelvk::physics