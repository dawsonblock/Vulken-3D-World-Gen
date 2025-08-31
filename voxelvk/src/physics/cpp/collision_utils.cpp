#include "collision_utils.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>
#include <vector>

namespace voxelvk::physics {
namespace collision_utils {

glm::vec3 closestPointOnAABB(const glm::vec3& point, const glm::vec3& min_point, const glm::vec3& max_point) {
    // Direct port of Python: np.minimum(np.maximum(p, mn), mx)
    return glm::clamp(point, min_point, max_point);
}

glm::vec3 closestPointOnSegment(const glm::vec3& point, const glm::vec3& seg_a, const glm::vec3& seg_b) {
    // Direct port of Python closest_point_on_segment with numeric stability
    glm::vec3 ab = seg_b - seg_a;
    float ab_dot = glm::dot(ab, ab);
    
    if (ab_dot < 1e-12f) {  // Degenerate segment - matches Python condition
        return seg_a;
    }
    
    float t = glm::dot(point - seg_a, ab) / ab_dot;
    t = glm::clamp(t, 0.0f, 1.0f);  // Equivalent to np.clip(t, 0.0, 1.0)
    
    return seg_a + t * ab;
}

PenetrationResult capsuleBoxPenetration(const Capsule& capsule, 
                                       const glm::vec3& box_min, 
                                       const glm::vec3& box_max) {
    PenetrationResult result;
    
    // Validate box bounds - direct port of Python validation
    if (box_min.x >= box_max.x || box_min.y >= box_max.y || box_min.z >= box_max.z) {
        return result;  // Invalid box
    }
    
    // Validate capsule - direct port of Python validation
    if (capsule.radius <= 0.0f) {
        return result;  // Invalid capsule
    }
    
    // Algorithm matches Python exactly:
    // 1. Find box center
    glm::vec3 box_center = (box_min + box_max) * 0.5f;
    
    // 2. Find closest point on capsule segment to box center
    glm::vec3 q_seg = closestPointOnSegment(box_center, capsule.segmentStart(), capsule.segmentEnd());
    
    // 3. Find closest point on box to segment point
    glm::vec3 q_box = closestPointOnAABB(q_seg, box_min, box_max);
    
    // 4. Calculate separation vector and distance
    glm::vec3 v = q_seg - q_box;
    float dist = glm::length(v);
    float pen = capsule.radius - dist;
    
    if (pen > 0.0f) {
        result.hit = true;
        result.penetration_depth = pen;
        
        // Calculate normal with numeric stability - matches Python logic
        if (dist > 1e-8f) {
            result.normal = v / (dist + 1e-9f);  // Matches Python: v / (dist + 1e-9)
        } else {
            result.normal = glm::vec3(0.0f, 1.0f, 0.0f);  // Default normal
        }
        
        // Ensure normal is valid - matches Python NaN/inf checks
        if (std::isnan(result.normal.x) || std::isnan(result.normal.y) || std::isnan(result.normal.z) ||
            std::isinf(result.normal.x) || std::isinf(result.normal.y) || std::isinf(result.normal.z)) {
            result.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
    
    return result;
}

bool validateAndClampCapsule(Capsule& capsule, const CollisionConfig& config) {
    bool was_modified = false;
    
    // Check for NaN/infinite positions - matches Python validation
    if (std::isnan(capsule.center.x) || std::isnan(capsule.center.y) || std::isnan(capsule.center.z) ||
        std::isinf(capsule.center.x) || std::isinf(capsule.center.y) || std::isinf(capsule.center.z)) {
        std::cout << "Warning: Invalid capsule center: (" << capsule.center.x << ", " 
                  << capsule.center.y << ", " << capsule.center.z << "), resetting to origin" << std::endl;
        capsule.center = glm::vec3(0.0f, 2.0f, 0.0f);
        was_modified = true;
    }
    
    // Validate dimensions - matches Python validation
    if (capsule.radius <= 0.0f || capsule.half_height <= 0.0f) {
        std::cout << "Warning: Invalid capsule dimensions: radius=" << capsule.radius 
                  << ", half_height=" << capsule.half_height << std::endl;
        capsule.radius = std::max(config.min_capsule_radius, capsule.radius);
        capsule.half_height = std::max(config.min_capsule_half_height, capsule.half_height);
        was_modified = true;
    }
    
    // Prevent extreme positions
    if (glm::length(capsule.center) > config.max_position_magnitude) {
        std::cout << "Warning: Capsule position out of bounds, clamping" << std::endl;
        capsule.center = glm::normalize(capsule.center) * config.max_position_magnitude;
        was_modified = true;
    }
    
    return was_modified;
}

CapsuleResolutionResult resolveCapsuleWorld(Capsule& capsule, 
                                          const WorldInterface& world,
                                          int max_iterations,
                                          int max_blocks_checked,
                                          float max_correction_per_iteration,
                                          int search_radius_limit) {
    CollisionConfig config;
    config.max_collision_iterations = max_iterations;
    config.max_blocks_checked = max_blocks_checked;
    config.max_correction_per_iteration = max_correction_per_iteration;
    config.search_radius_limit = search_radius_limit;
    
    return resolveCapsuleWorldAdvanced(capsule, world, config);
}

CapsuleResolutionResult resolveCapsuleWorldAdvanced(Capsule& capsule, 
                                                   const WorldInterface& world,
                                                   const CollisionConfig& config) {
    CapsuleResolutionResult result;
    
    // Validate capsule first - matches Python validation
    bool capsule_modified = validateAndClampCapsule(capsule, config);
    result.position_clamped = capsule_modified;
    
    // Calculate initial bounding box - matches Python algorithm exactly
    glm::vec3 mn = capsule.center - glm::vec3(capsule.radius, 
                                             capsule.half_height + capsule.radius, 
                                             capsule.radius);
    glm::vec3 mx = capsule.center + glm::vec3(capsule.radius, 
                                             capsule.half_height + capsule.radius, 
                                             capsule.radius);
    
    glm::ivec3 bb_min = glm::ivec3(std::floor(mn.x), std::floor(mn.y), std::floor(mn.z));
    glm::ivec3 bb_max = glm::ivec3(std::floor(mx.x), std::floor(mx.y), std::floor(mx.z));
    
    // Limit search area to prevent infinite loops - matches Python exactly
    glm::ivec3 capsule_pos_int = glm::ivec3(std::floor(capsule.center.x), 
                                           std::floor(capsule.center.y), 
                                           std::floor(capsule.center.z));
    
    bb_min = glm::max(bb_min, capsule_pos_int - config.search_radius_limit);
    bb_max = glm::min(bb_max, capsule_pos_int + config.search_radius_limit);
    
    glm::vec3 total_offset(0.0f);
    bool ground = false;
    
    // Main collision resolution loop - matches Python exactly
    for (int iteration = 0; iteration < config.max_collision_iterations; ++iteration) {
        float max_pen = 0.0f;
        glm::vec3 hit_normal(0.0f);
        glm::vec3 contact_normal(0.0f);
        int blocks_checked = 0;
        bool found_collision = false;
        
        // Triple nested loop matches Python exactly: y, z, x order
        for (int y = bb_min.y - 1; y <= bb_max.y + 1; ++y) {
            for (int z = bb_min.z - 1; z <= bb_max.z + 1; ++z) {
                for (int x = bb_min.x - 1; x <= bb_max.x + 1; ++x) {
                    blocks_checked++;
                    
                    // Safety check - matches Python
                    if (blocks_checked > config.max_blocks_checked) {
                        std::cout << "Warning: Too many collision checks (" << blocks_checked 
                                  << "), breaking" << std::endl;
                        goto break_all_loops;
                    }
                    
                    uint16_t block_type;
                    try {
                        block_type = world.getBlockAtWorldPosition(static_cast<float>(x), 
                                                                  static_cast<float>(y), 
                                                                  static_cast<float>(z));
                    } catch (const std::exception& e) {
                        std::cout << "Warning: Error getting block at (" << x << ", " << y 
                                  << ", " << z << "): " << e.what() << std::endl;
                        block_type = 0;  // Assume air on error
                    }
                    
                    // Skip air blocks - matches Python
                    if (!world.isBlockSolid(block_type)) {
                        continue;
                    }
                    
                    // Test collision with this voxel - matches Python exactly
                    glm::vec3 voxel_min(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                    glm::vec3 voxel_max = voxel_min + glm::vec3(1.0f);
                    
                    PenetrationResult penetration = capsuleBoxPenetration(capsule, voxel_min, voxel_max);
                    
                    if (penetration.hit && penetration.penetration_depth > max_pen) {
                        max_pen = penetration.penetration_depth;
                        hit_normal = penetration.normal;
                        contact_normal = penetration.normal;
                        found_collision = true;
                    }
                }
                
                if (blocks_checked > config.max_blocks_checked) break;
            }
            
            if (blocks_checked > config.max_blocks_checked) break;
        }
        
        break_all_loops:
        
        result.blocks_checked += blocks_checked;
        result.iterations_used = iteration + 1;
        
        // Check convergence - matches Python exactly
        if (max_pen <= config.penetration_epsilon || !found_collision) {
            break;
        }
        
        // Limit penetration resolution to prevent explosions - matches Python
        max_pen = std::min(max_pen, config.max_correction_per_iteration);
        glm::vec3 correction = hit_normal * max_pen;
        
        capsule.center += correction;
        total_offset += correction;
        
        // Update bounding box for next iteration - matches Python exactly
        mn = capsule.center - glm::vec3(capsule.radius, 
                                       capsule.half_height + capsule.radius, 
                                       capsule.radius);
        mx = capsule.center + glm::vec3(capsule.radius, 
                                       capsule.half_height + capsule.radius, 
                                       capsule.radius);
        
        bb_min = glm::ivec3(std::floor(mn.x), std::floor(mn.y), std::floor(mn.z));
        bb_max = glm::ivec3(std::floor(mx.x), std::floor(mx.y), std::floor(mx.z));
        
        capsule_pos_int = glm::ivec3(std::floor(capsule.center.x), 
                                    std::floor(capsule.center.y), 
                                    std::floor(capsule.center.z));
        
        bb_min = glm::max(bb_min, capsule_pos_int - config.search_radius_limit);
        bb_max = glm::min(bb_max, capsule_pos_int + config.search_radius_limit);
        
        // Ground detection - matches Python exactly
        if (found_collision && contact_normal.y > config.ground_normal_threshold) {
            ground = true;
        }
    }
    
    result.total_offset = total_offset;
    result.is_on_ground = ground;
    
    return result;
}

bool isCapsulePositionSafe(const Capsule& capsule, const WorldInterface& world) {
    // Check if capsule intersects any solid blocks
    AABB bounds = capsule.getBoundingBox();
    glm::ivec3 min_voxel, max_voxel;
    bounds.getOverlappingVoxelsBounds(min_voxel, max_voxel);
    
    for (int y = min_voxel.y; y <= max_voxel.y; ++y) {
        for (int z = min_voxel.z; z <= max_voxel.z; ++z) {
            for (int x = min_voxel.x; x <= max_voxel.x; ++x) {
                try {
                    uint16_t block_type = world.getBlockAtWorldPosition(static_cast<float>(x), 
                                                                       static_cast<float>(y), 
                                                                       static_cast<float>(z));
                    
                    if (world.isBlockSolid(block_type)) {
                        glm::vec3 voxel_min(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                        glm::vec3 voxel_max = voxel_min + glm::vec3(1.0f);
                        
                        PenetrationResult penetration = capsuleBoxPenetration(capsule, voxel_min, voxel_max);
                        if (penetration.hit) {
                            return false;
                        }
                    }
                } catch (const std::exception&) {
                    // Assume unsafe on error
                    return false;
                }
            }
        }
    }
    
    return true;
}

glm::vec3 findSafePosition(const glm::vec3& desired_position, 
                          const Capsule& capsule_template,
                          const WorldInterface& world,
                          float search_radius) {
    // Try the desired position first
    Capsule test_capsule = capsule_template;
    test_capsule.center = desired_position;
    
    if (isCapsulePositionSafe(test_capsule, world)) {
        return desired_position;
    }
    
    // Search in expanding spheres around the desired position
    const int max_attempts = 100;
    const float step_size = 0.5f;
    
    for (float radius = step_size; radius <= search_radius; radius += step_size) {
        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            // Generate random point on sphere surface
            float theta = (static_cast<float>(attempt) / max_attempts) * 2.0f * 3.14159f;
            float phi = std::acos(1.0f - 2.0f * (static_cast<float>(attempt % 10) / 10.0f));
            
            glm::vec3 offset(
                radius * std::sin(phi) * std::cos(theta),
                radius * std::cos(phi),
                radius * std::sin(phi) * std::sin(theta)
            );
            
            test_capsule.center = desired_position + offset;
            
            if (isCapsulePositionSafe(test_capsule, world)) {
                return test_capsule.center;
            }
        }
    }
    
    // If no safe position found, return the desired position (might cause issues)
    std::cout << "Warning: Could not find safe position near (" << desired_position.x 
              << ", " << desired_position.y << ", " << desired_position.z << ")" << std::endl;
    return desired_position;
}

void batchResolveCapsules(std::vector<Capsule>& capsules,
                         const WorldInterface& world,
                         std::vector<CapsuleResolutionResult>& results,
                         const CollisionConfig& config) {
    results.resize(capsules.size());
    
    for (size_t i = 0; i < capsules.size(); ++i) {
        results[i] = resolveCapsuleWorldAdvanced(capsules[i], world, config);
    }
}

// CollisionProfiler implementation
void CollisionProfiler::beginFrame() {
    current_frame_data_ = CollisionProfileData{};
}

void CollisionProfiler::endFrame() {
    if (current_frame_data_.total_capsules_processed > 0) {
        current_frame_data_.average_time_per_capsule = 
            current_frame_data_.total_time_ms / current_frame_data_.total_capsules_processed;
        current_frame_data_.average_iterations_per_capsule = 
            static_cast<float>(current_frame_data_.total_iterations) / current_frame_data_.total_capsules_processed;
    }
}

void CollisionProfiler::recordCapsuleResolution(const CapsuleResolutionResult& result, float time_ms) {
    current_frame_data_.total_time_ms += time_ms;
    current_frame_data_.total_iterations += result.iterations_used;
    current_frame_data_.total_blocks_checked += result.blocks_checked;
    current_frame_data_.total_capsules_processed++;
}

CollisionProfileData CollisionProfiler::getFrameData() const {
    return current_frame_data_;
}

void CollisionProfiler::reset() {
    current_frame_data_ = CollisionProfileData{};
}

DebugCollisionInfo debugResolveCapsuleWorld(Capsule& capsule, 
                                           const WorldInterface& world,
                                           const CollisionConfig& config) {
    DebugCollisionInfo debug_info;
    debug_info.original_position = capsule.center;
    
    // Run normal collision resolution but capture debug data
    CapsuleResolutionResult result = resolveCapsuleWorldAdvanced(capsule, world, config);
    
    debug_info.final_position = capsule.center;
    debug_info.total_correction = result.total_offset;
    debug_info.iteration_count = result.iterations_used;
    debug_info.converged = (result.iterations_used < config.max_collision_iterations);
    
    // TODO: Implement detailed debug capture if needed
    // This would require modifying the resolution function to capture intermediate states
    
    return debug_info;
}

} // namespace collision_utils
} // namespace voxelvk::physics