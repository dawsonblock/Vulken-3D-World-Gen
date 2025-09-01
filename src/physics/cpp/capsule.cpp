#include "capsule.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace voxelvk::physics {

// Static constants
const Capsule Capsule::UNIT = Capsule(glm::vec3(0.0f), 0.5f, 0.5f);

// Constructors
Capsule::Capsule(const glm::vec3& center, float half_height, float radius)
    : center(center), half_height(half_height), radius(radius) {
    ensureValid();
}

Capsule::Capsule(const glm::vec3& bottom, const glm::vec3& top, float radius)
    : radius(radius) {
    center = (bottom + top) * 0.5f;
    half_height = glm::length(top - bottom) * 0.5f;
    ensureValid();
}

// Static factory methods
Capsule Capsule::fromBottomTop(const glm::vec3& bottom, const glm::vec3& top, float radius) {
    return Capsule(bottom, top, radius);
}

Capsule Capsule::fromCenterHeight(const glm::vec3& center, float total_height, float radius) {
    float capsule_half_height = (total_height - radius * 2.0f) * 0.5f;
    return Capsule(center, std::max(capsule_half_height, 0.0f), radius);
}

Capsule Capsule::playerCapsule(const glm::vec3& position) {
    // Standard player capsule: 0.6 width, 1.8 height
    return Capsule(position, 0.9f, 0.3f);
}

// Bounding volume
AABB Capsule::getBoundingBox() const {
    glm::vec3 extent(radius, half_height + radius, radius);
    return AABB(center, extent);
}

AABB Capsule::getExpandedBoundingBox(float expansion) const {
    glm::vec3 extent(radius + expansion, half_height + radius + expansion, radius + expansion);
    return AABB(center, extent);
}

// Transformation operations
Capsule Capsule::moved(const glm::vec3& delta) const {
    return Capsule(center + delta, half_height, radius);
}

Capsule Capsule::scaled(float scale) const {
    return Capsule(center * scale, half_height * scale, radius * scale);
}

Capsule Capsule::withRadius(float new_radius) const {
    return Capsule(center, half_height, new_radius);
}

Capsule Capsule::withHalfHeight(float new_half_height) const {
    return Capsule(center, new_half_height, radius);
}

// Distance and containment queries
float Capsule::distanceToPoint(const glm::vec3& point) const {
    return std::sqrt(squaredDistanceToPoint(point));
}

float Capsule::squaredDistanceToPoint(const glm::vec3& point) const {
    glm::vec3 closest = closestPoint(point);
    glm::vec3 diff = point - closest;
    return glm::dot(diff, diff);
}

glm::vec3 Capsule::closestPoint(const glm::vec3& point) const {
    // Find closest point on the line segment (capsule axis)
    glm::vec3 segment_closest = capsule_geometry::closestPointOnSegment(point, bottom(), top());
    
    // Project from segment to capsule surface
    glm::vec3 to_point = point - segment_closest;
    float distance = glm::length(to_point);
    
    if (distance <= EPSILON) {
        // Point is on the axis, return any point on the surface
        return segment_closest + glm::vec3(radius, 0, 0);
    }
    
    // Normalize and scale to radius
    return segment_closest + (to_point / distance) * radius;
}

bool Capsule::contains(const glm::vec3& point) const {
    return distanceToPoint(point) <= radius + EPSILON;
}

// Collision queries
bool Capsule::intersects(const AABB& box) const {
    auto result = capsule_collision::capsuleAABBIntersection(*this, box);
    return result.intersects;
}

bool Capsule::intersects(const Capsule& other) const {
    auto result = capsule_collision::capsuleCapsuleIntersection(*this, other);
    return result.intersects;
}

bool Capsule::intersectsVoxel(int x, int y, int z) const {
    auto result = capsule_collision::capsuleVoxelIntersection(*this, x, y, z);
    return result.intersects;
}

bool Capsule::intersectsVoxel(const glm::ivec3& voxel_pos) const {
    return intersectsVoxel(voxel_pos.x, voxel_pos.y, voxel_pos.z);
}

// Support function for GJK collision detection
glm::vec3 Capsule::support(const glm::vec3& direction) const {
    glm::vec3 normalized_dir = capsule_geometry::safeNormalize(direction);
    
    // Choose the extreme point along the capsule axis
    glm::vec3 axis_point;
    if (normalized_dir.y > 0) {
        axis_point = top();
    } else {
        axis_point = bottom();
    }
    
    // Add radius in the direction
    return axis_point + normalized_dir * radius;
}

// Utility methods
bool Capsule::isValid() const {
    return radius >= MIN_RADIUS && 
           half_height >= MIN_HALF_HEIGHT &&
           std::isfinite(center.x) && std::isfinite(center.y) && std::isfinite(center.z) &&
           std::isfinite(radius) && std::isfinite(half_height);
}

void Capsule::validate() {
    ensureValid();
}

float Capsule::volume() const {
    // Volume = cylinder volume + 2 * hemisphere volume
    // Cylinder: π * r² * h
    // Sphere: (4/3) * π * r³
    const float pi = 3.14159265359f;
    float cylinder_volume = pi * radius * radius * (half_height * 2.0f);
    float sphere_volume = (4.0f / 3.0f) * pi * radius * radius * radius;
    return cylinder_volume + sphere_volume;
}

float Capsule::surfaceArea() const {
    // Surface area = cylinder side area + 2 * hemisphere area
    // Cylinder side: 2 * π * r * h
    // Sphere: 4 * π * r²
    const float pi = 3.14159265359f;
    float cylinder_area = 2.0f * pi * radius * (half_height * 2.0f);
    float sphere_area = 4.0f * pi * radius * radius;
    return cylinder_area + sphere_area;
}

// Debug information
Capsule::DebugInfo Capsule::getDebugInfo() const {
    DebugInfo info;
    info.segment_start = bottom();
    info.segment_end = top();
    info.segment_length = half_height * 2.0f;
    info.bounding_box = getBoundingBox();
    info.is_degenerate = (half_height < MIN_HALF_HEIGHT) || (radius < MIN_RADIUS);
    return info;
}

// Comparison operators
bool Capsule::operator==(const Capsule& other) const {
    return glm::all(glm::epsilonEqual(center, other.center, EPSILON)) &&
           glm::epsilonEqual(half_height, other.half_height, EPSILON) &&
           glm::epsilonEqual(radius, other.radius, EPSILON);
}

bool Capsule::operator!=(const Capsule& other) const {
    return !(*this == other);
}

// Private methods
void Capsule::ensureValid() {
    // Clamp to minimum values
    radius = std::max(radius, MIN_RADIUS);
    half_height = std::max(half_height, MIN_HALF_HEIGHT);
    
    // Handle NaN/infinite values
    if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)) {
        center = glm::vec3(0.0f);
    }
    
    if (!std::isfinite(radius)) {
        radius = MIN_RADIUS;
    }
    
    if (!std::isfinite(half_height)) {
        half_height = MIN_HALF_HEIGHT;
    }
}

// Capsule collision utilities
namespace capsule_collision {

float pointToCapsuleDistance(const glm::vec3& point, const Capsule& capsule) {
    return capsule.distanceToPoint(point);
}

glm::vec3 closestPointOnCapsule(const glm::vec3& point, const Capsule& capsule) {
    return capsule.closestPoint(point);
}

CapsuleAABBResult capsuleAABBIntersection(const Capsule& capsule, const AABB& box) {
    CapsuleAABBResult result;
    
    if (box.isEmpty()) {
        return result;
    }
    
    // Find closest point on box to the capsule's line segment
    glm::vec3 box_min = box.min();
    glm::vec3 box_max = box.max();
    
    glm::vec3 segment_start = capsule.bottom();
    glm::vec3 segment_end = capsule.top();
    
    // Find closest point on segment to box center
    glm::vec3 box_center = (box_min + box_max) * 0.5f;
    glm::vec3 closest_on_segment = capsule_geometry::closestPointOnSegment(
        box_center, segment_start, segment_end);
    
    // Find closest point on box to the segment point
    glm::vec3 closest_on_box = capsule_geometry::closestPointOnAABB(
        closest_on_segment, box_min, box_max);
    
    // Calculate distance and check intersection
    glm::vec3 separation = closest_on_segment - closest_on_box;
    float distance = glm::length(separation);
    
    result.intersects = distance <= capsule.radius;
    
    if (result.intersects) {
        result.penetration_depth = capsule.radius - distance;
        result.contact_point = closest_on_box;
        
        if (distance > 1e-6f) {
            result.normal = separation / distance;
        } else {
            // Capsule center is inside box, find best separation direction
            glm::vec3 box_center_to_capsule = closest_on_segment - box_center;
            result.normal = capsule_geometry::safeNormalize(box_center_to_capsule, glm::vec3(0, 1, 0));
        }
        
        result.separation_vector = result.normal * result.penetration_depth;
    }
    
    return result;
}

CapsuleAABBResult capsuleVoxelIntersection(const Capsule& capsule, int x, int y, int z) {
    glm::vec3 voxel_min(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    glm::vec3 voxel_max = voxel_min + glm::vec3(1.0f);
    
    AABB voxel_box = AABB::fromMinMax(voxel_min, voxel_max);
    return capsuleAABBIntersection(capsule, voxel_box);
}

CapsuleAABBResult capsuleVoxelIntersection(const Capsule& capsule, const glm::ivec3& voxel_pos) {
    return capsuleVoxelIntersection(capsule, voxel_pos.x, voxel_pos.y, voxel_pos.z);
}

CapsuleCapsuleResult capsuleCapsuleIntersection(const Capsule& a, const Capsule& b) {
    CapsuleCapsuleResult result;
    
    // Find closest points between the two line segments
    glm::vec3 closest_a, closest_b;
    float distance = capsule_geometry::segmentToSegmentDistance(
        a.bottom(), a.top(), b.bottom(), b.top(), closest_a, closest_b);
    
    float combined_radius = a.radius + b.radius;
    result.intersects = distance <= combined_radius;
    
    if (result.intersects) {
        result.penetration_depth = combined_radius - distance;
        result.contact_point_a = closest_a;
        result.contact_point_b = closest_b;
        
        if (distance > 1e-6f) {
            glm::vec3 separation = closest_a - closest_b;
            result.normal = glm::normalize(separation);
        } else {
            // Capsules are coincident, use arbitrary but consistent direction
            glm::vec3 diff = a.center - b.center;
            result.normal = capsule_geometry::safeNormalize(diff, glm::vec3(1, 0, 0));
        }
        
        result.separation_vector = result.normal * result.penetration_depth;
    }
    
    return result;
}

bool capsuleRayIntersection(const Capsule& capsule, 
                           const glm::vec3& ray_origin, 
                           const glm::vec3& ray_direction,
                           float& t_near, float& t_far) {
    // Simplified implementation - intersect ray with capsule's bounding cylinder + sphere caps
    // This is an approximation for performance; exact capsule-ray intersection is more complex
    
    AABB capsule_bounds = capsule.getBoundingBox();
    
    // First check if ray intersects the bounding box
    glm::vec3 inv_dir = 1.0f / ray_direction;
    glm::vec3 t_min = (capsule_bounds.min() - ray_origin) * inv_dir;
    glm::vec3 t_max = (capsule_bounds.max() - ray_origin) * inv_dir;
    
    glm::vec3 t_enter = glm::min(t_min, t_max);
    glm::vec3 t_exit = glm::max(t_min, t_max);
    
    float t_enter_max = glm::max(glm::max(t_enter.x, t_enter.y), t_enter.z);
    float t_exit_min = glm::min(glm::min(t_exit.x, t_exit.y), t_exit.z);
    
    if (t_enter_max > t_exit_min || t_exit_min < 0) {
        return false;
    }
    
    t_near = std::max(t_enter_max, 0.0f);
    t_far = t_exit_min;
    
    return true;
}

void batchCapsuleVoxelTest(const std::vector<Capsule>& capsules,
                          const std::vector<glm::ivec3>& voxels,
                          std::vector<std::vector<bool>>& results) {
    results.resize(capsules.size());
    
    for (size_t i = 0; i < capsules.size(); ++i) {
        results[i].resize(voxels.size());
        for (size_t j = 0; j < voxels.size(); ++j) {
            results[i][j] = capsules[i].intersectsVoxel(voxels[j]);
        }
    }
}

void batchCapsuleAABBTest(const std::vector<Capsule>& capsules,
                         const std::vector<AABB>& boxes,
                         std::vector<std::vector<CapsuleAABBResult>>& results) {
    results.resize(capsules.size());
    
    for (size_t i = 0; i < capsules.size(); ++i) {
        results[i].resize(boxes.size());
        for (size_t j = 0; j < boxes.size(); ++j) {
            results[i][j] = capsuleAABBIntersection(capsules[i], boxes[j]);
        }
    }
}

} // namespace capsule_collision

// Geometric utilities
namespace capsule_geometry {

glm::vec3 closestPointOnSegment(const glm::vec3& point, 
                               const glm::vec3& seg_start, 
                               const glm::vec3& seg_end) {
    glm::vec3 segment = seg_end - seg_start;
    float segment_length_sq = glm::dot(segment, segment);
    
    if (segment_length_sq < 1e-12f) {
        // Degenerate segment
        return seg_start;
    }
    
    float t = glm::dot(point - seg_start, segment) / segment_length_sq;
    t = glm::clamp(t, 0.0f, 1.0f);
    
    return seg_start + t * segment;
}

float segmentToSegmentDistance(const glm::vec3& a1, const glm::vec3& a2,
                              const glm::vec3& b1, const glm::vec3& b2,
                              glm::vec3& closest_a, glm::vec3& closest_b) {
    glm::vec3 d1 = a2 - a1;
    glm::vec3 d2 = b2 - b1;
    glm::vec3 r = a1 - b1;
    
    float a = glm::dot(d1, d1);
    float e = glm::dot(d2, d2);
    float f = glm::dot(d2, r);
    
    const float epsilon = 1e-12f;
    
    if (a <= epsilon && e <= epsilon) {
        // Both segments are points
        closest_a = a1;
        closest_b = b1;
        return glm::length(closest_a - closest_b);
    }
    
    float s, t;
    
    if (a <= epsilon) {
        // First segment is a point
        s = 0.0f;
        t = glm::clamp(f / e, 0.0f, 1.0f);
    } else {
        float c = glm::dot(d1, r);
        if (e <= epsilon) {
            // Second segment is a point
            t = 0.0f;
            s = glm::clamp(-c / a, 0.0f, 1.0f);
        } else {
            // General case
            float b = glm::dot(d1, d2);
            float denom = a * e - b * b;
            
            if (denom != 0.0f) {
                s = glm::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            } else {
                s = 0.0f;
            }
            
            t = (b * s + f) / e;
            
            if (t < 0.0f) {
                t = 0.0f;
                s = glm::clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = glm::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    
    closest_a = a1 + s * d1;
    closest_b = b1 + t * d2;
    
    return glm::length(closest_a - closest_b);
}

bool sphereAABBIntersection(const glm::vec3& sphere_center, float sphere_radius,
                           const glm::vec3& box_min, const glm::vec3& box_max) {
    glm::vec3 closest = closestPointOnAABB(sphere_center, box_min, box_max);
    float distance_sq = glm::dot(closest - sphere_center, closest - sphere_center);
    return distance_sq <= sphere_radius * sphere_radius;
}

glm::vec3 closestPointOnAABB(const glm::vec3& point, 
                            const glm::vec3& box_min, 
                            const glm::vec3& box_max) {
    return glm::clamp(point, box_min, box_max);
}

bool cylinderAABBIntersection(const glm::vec3& cylinder_center, 
                             float cylinder_radius, 
                             float cylinder_half_height,
                             const glm::vec3& box_min, 
                             const glm::vec3& box_max) {
    // Simplified cylinder-AABB intersection test
    // Check if the cylinder's bounding box overlaps with the AABB
    glm::vec3 cyl_min = cylinder_center - glm::vec3(cylinder_radius, cylinder_half_height, cylinder_radius);
    glm::vec3 cyl_max = cylinder_center + glm::vec3(cylinder_radius, cylinder_half_height, cylinder_radius);
    
    return !(cyl_max.x <= box_min.x || cyl_min.x >= box_max.x ||
             cyl_max.y <= box_min.y || cyl_min.y >= box_max.y ||
             cyl_max.z <= box_min.z || cyl_min.z >= box_max.z);
}

bool isPointOnSegment(const glm::vec3& point, 
                     const glm::vec3& seg_start, 
                     const glm::vec3& seg_end, 
                     float tolerance) {
    glm::vec3 closest = closestPointOnSegment(point, seg_start, seg_end);
    return glm::length(point - closest) <= tolerance;
}

glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback) {
    float length_sq = glm::dot(v, v);
    if (length_sq > 1e-12f) {
        return v / std::sqrt(length_sq);
    }
    return fallback;
}

} // namespace capsule_geometry

} // namespace voxelvk::physics