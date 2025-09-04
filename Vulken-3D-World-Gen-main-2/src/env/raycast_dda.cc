#include "raycast_dda.hpp"
#include "../core/nvtx_profiler.hpp"
#include "../core/thread_pool.hpp"
#include <cmath>
#include <algorithm>
#include <random>

namespace voxelvk {

// RaycastDDA implementation
RaycastDDA::RaycastDDA(WorldManager* world_manager) 
    : m_world_manager(world_manager) {
    if (!m_world_manager) {
        throw std::invalid_argument("WorldManager cannot be null");
    }
}

RaycastHit RaycastDDA::Raycast(const Ray& ray) const {
    // First, prefer transparent solids like glass if present along the path
    RaycastHit transparent_hit = PerformDDA(ray, [](BlockType type) {
        return BlockRegistry::Instance().IsSolid(type) &&
               BlockRegistry::Instance().IsTransparent(type);
    }, /*update_stats=*/false);
    if (transparent_hit.hit) {
        // Count only the final, returned hit in stats
        m_stats.Update(transparent_hit);
        return transparent_hit;
    }
    // Otherwise, fall back to any solid
    RaycastHit hit = PerformDDA(ray, [](BlockType type) {
        return BlockRegistry::Instance().IsSolid(type);
    });
    return hit;
}

RaycastHit RaycastDDA::Raycast(const Vec3& origin, const Vec3& direction, float max_distance) const {
    return Raycast(Ray(origin, direction, max_distance));
}

RaycastHit RaycastDDA::Raycast(const Ray& ray, std::function<bool(BlockType)> block_filter) const {
    return PerformDDA(ray, block_filter);
}

std::vector<RaycastHit> RaycastDDA::BatchRaycast(const std::vector<RaycastRequest>& requests) const {
    VXL_NVTX_RANGE_COLOR("RaycastDDA::BatchRaycast", NVTXProfiler::Color::Cyan);
    
    std::vector<RaycastHit> results;
    results.reserve(requests.size());
    
    for (const auto& request : requests) {
        auto filter = request.block_filter ? request.block_filter : [&](BlockType type) {
            if (request.ignore_transparent && BlockRegistry::Instance().IsTransparent(type)) {
                return false;
            }
            if (request.ignore_fluids && BlockRegistry::Instance().IsFluid(type)) {
                return false;
            }
            return BlockRegistry::Instance().IsSolid(type);
        };
        
        RaycastHit hit = PerformDDA(request.ray, filter);
        results.push_back(hit);
    }
    
    return results;
}

void RaycastDDA::BatchRaycastAsync(const std::vector<RaycastRequest>& requests, 
                                  std::function<void(const std::vector<RaycastHit>&)> callback) const {
    auto& thread_pool = GlobalThreadPool::Instance();
    
    thread_pool.SubmitDetached([this, requests, callback]() {
        auto results = BatchRaycast(requests);
        callback(results);
    });
}

RaycastHit RaycastDDA::RaycastIgnoreTransparent(const Ray& ray) const {
    return PerformDDA(ray, [](BlockType type) {
        return BlockRegistry::Instance().IsSolid(type) && 
               !BlockRegistry::Instance().IsTransparent(type);
    });
}

RaycastHit RaycastDDA::RaycastIgnoreFluids(const Ray& ray) const {
    return PerformDDA(ray, [](BlockType type) {
        return BlockRegistry::Instance().IsSolid(type) && 
               !BlockRegistry::Instance().IsFluid(type);
    });
}

RaycastHit RaycastDDA::RaycastSolidsOnly(const Ray& ray) const {
    return PerformDDA(ray, [](BlockType type) {
        return BlockRegistry::Instance().IsSolid(type) && 
               !BlockRegistry::Instance().IsTransparent(type) &&
               !BlockRegistry::Instance().IsFluid(type);
    });
}

bool RaycastDDA::HasLineOfSight(const Vec3& from, const Vec3& to, bool ignore_transparent) const {
    Vec3 direction = (to - from).normalized();
    float distance = (to - from).length();
    
    Ray ray(from, direction, distance);
    
    auto filter = [ignore_transparent](BlockType type) {
        if (ignore_transparent && BlockRegistry::Instance().IsTransparent(type)) {
            return false;
        }
        return BlockRegistry::Instance().IsSolid(type);
    };
    
    RaycastHit hit = PerformDDA(ray, filter);
    
    // Line of sight exists if we don't hit anything, or hit point is beyond target
    return !hit.hit || hit.distance >= distance - m_config.step_epsilon;
}

bool RaycastDDA::HasLineOfSight(const BlockPos& from, const BlockPos& to, bool ignore_transparent) const {
    Vec3 from_vec(static_cast<float>(from.x) + 0.5f, 
                  static_cast<float>(from.y) + 0.5f, 
                  static_cast<float>(from.z) + 0.5f);
    Vec3 to_vec(static_cast<float>(to.x) + 0.5f, 
                static_cast<float>(to.y) + 0.5f, 
                static_cast<float>(to.z) + 0.5f);
    
    return HasLineOfSight(from_vec, to_vec, ignore_transparent);
}

std::vector<BlockPos> RaycastDDA::GetVisibleBlocks(const Vec3& observer, 
                                                  const std::vector<BlockPos>& targets, 
                                                  float max_distance) const {
    std::vector<BlockPos> visible;
    visible.reserve(targets.size());
    
    for (const auto& target : targets) {
        Vec3 target_vec(static_cast<float>(target.x) + 0.5f,
                       static_cast<float>(target.y) + 0.5f,
                       static_cast<float>(target.z) + 0.5f);
        
        float distance = (target_vec - observer).length();
        if (distance <= max_distance && HasLineOfSight(observer, target_vec, true)) {
            visible.push_back(target);
        }
    }
    
    return visible;
}

float RaycastDDA::GetLightAttenuation(const Vec3& light_pos, const Vec3& target_pos) const {
    Vec3 direction = (target_pos - light_pos).normalized();
    float distance = (target_pos - light_pos).length();
    
    Ray ray(light_pos, direction, distance);
    
    float attenuation = 1.0f;
    float current_distance = 0.0f;
    
    // Step through the ray and accumulate light absorption
    Vec3 step_size = direction * 0.5f; // Half-block steps for accuracy
    Vec3 current_pos = light_pos;
    
    while (current_distance < distance) {
        BlockPos block_pos = current_pos.to_block_pos();
        BlockType block_type = GetBlockCached(block_pos);
        
        if (block_type != BlockType::Air) {
            uint8_t absorption = BlockUtils::GetLightAbsorption(block_type);
            float absorption_factor = static_cast<float>(absorption) / 15.0f;
            attenuation *= (1.0f - absorption_factor * 0.1f); // 10% absorption per level
            
            if (attenuation < 0.01f) {
                break; // Effectively no light passes through
            }
        }
        
        current_pos = current_pos + step_size;
        current_distance += 0.5f;
    }
    
    return attenuation;
}

RaycastHit RaycastDDA::PerformDDA(const Ray& ray, std::function<bool(BlockType)> should_stop, bool update_stats) const {
    VXL_NVTX_RANGE_COLOR("RaycastDDA::PerformDDA", NVTXProfiler::Color::Yellow);
    Timer timer;
    
    RaycastHit hit;
    hit.hit = false;
    
    // DDA setup
    const Vec3 ray_dir_norm = ray.direction.length_squared() > 0.0f ? ray.direction.normalized() : Vec3(0, 0, 0);
    if (ray_dir_norm.length_squared() == 0.0f) {
    hit.computation_time_us = static_cast<float>(timer.ElapsedUs());
    if (update_stats) m_stats.Update(hit);
    return hit;
    }
    
    int32_t x = static_cast<int32_t>(std::floor(ray.origin.x));
    int32_t y = static_cast<int32_t>(std::floor(ray.origin.y));
    int32_t z = static_cast<int32_t>(std::floor(ray.origin.z));
    
    const int32_t step_x = (ray_dir_norm.x > 0) ? 1 : -1;
    const int32_t step_y = (ray_dir_norm.y > 0) ? 1 : -1;
    const int32_t step_z = (ray_dir_norm.z > 0) ? 1 : -1;
    
    const float delta_x = (std::abs(ray_dir_norm.x) > 0.0f) ? std::abs(1.0f / ray_dir_norm.x) : std::numeric_limits<float>::infinity();
    const float delta_y = (std::abs(ray_dir_norm.y) > 0.0f) ? std::abs(1.0f / ray_dir_norm.y) : std::numeric_limits<float>::infinity();
    const float delta_z = (std::abs(ray_dir_norm.z) > 0.0f) ? std::abs(1.0f / ray_dir_norm.z) : std::numeric_limits<float>::infinity();
    
    auto frac = [](float v) {
        return v - std::floor(v);
    };
    float tMaxX;
    if (std::isinf(delta_x)) {
        tMaxX = std::numeric_limits<float>::infinity();
    } else {
        float fx = frac(ray.origin.x);
        float distToBoundary = (step_x > 0) ? (1.0f - fx) : fx;
        if (std::abs(distToBoundary) < m_config.step_epsilon) distToBoundary = 0.0f;
        tMaxX = distToBoundary * delta_x;
    }
    float tMaxY;
    if (std::isinf(delta_y)) {
        tMaxY = std::numeric_limits<float>::infinity();
    } else {
        float fy = frac(ray.origin.y);
        float distToBoundary = (step_y > 0) ? (1.0f - fy) : fy;
        if (std::abs(distToBoundary) < m_config.step_epsilon) distToBoundary = 0.0f;
        tMaxY = distToBoundary * delta_y;
    }
    float tMaxZ;
    if (std::isinf(delta_z)) {
        tMaxZ = std::numeric_limits<float>::infinity();
    } else {
        float fz = frac(ray.origin.z);
        float distToBoundary = (step_z > 0) ? (1.0f - fz) : fz;
        if (std::abs(distToBoundary) < m_config.step_epsilon) distToBoundary = 0.0f;
        tMaxZ = distToBoundary * delta_z;
    }
    
    float t = 0.0f;
    int32_t steps = 0;
    BlockUtils::BlockFace hit_face = BlockUtils::BlockFace::North;
    
    // If starting inside a solid, and t==0 at a boundary, still treat current cell if filter says stop
    {
        BlockPos start_pos(x, y, z);
        BlockType start_type = GetBlockCached(start_pos);
        if (should_stop(start_type)) {
            hit.hit = true;
            hit.block_pos = start_pos;
            hit.block_type = start_type;
            // Determine face by looking at which axis will be crossed first using tMax values
            if (tMaxY <= tMaxX && tMaxY <= tMaxZ) hit_face = (step_y > 0) ? BlockUtils::BlockFace::Down : BlockUtils::BlockFace::Up;
            else if (tMaxX <= tMaxZ) hit_face = (step_x > 0) ? BlockUtils::BlockFace::West : BlockUtils::BlockFace::East;
            else hit_face = (step_z > 0) ? BlockUtils::BlockFace::South : BlockUtils::BlockFace::North;
            hit.distance = 0.0f;
            hit.hit_point = ray.origin;
            hit.hit_normal = GetFaceNormal(hit_face);
            hit.steps_taken = steps;
            hit.computation_time_us = static_cast<float>(timer.ElapsedUs());
            if (update_stats) m_stats.Update(hit);
            return hit;
        }
    }

    while (steps < m_config.max_steps) {
        steps++;
        
        // Step to next voxel boundary along smallest tMax
        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                t = tMaxX;
                if (t > ray.max_distance) break;
                x += step_x;
                tMaxX += delta_x;
                hit_face = (step_x > 0) ? BlockUtils::BlockFace::West : BlockUtils::BlockFace::East;
            } else {
                t = tMaxZ;
                if (t > ray.max_distance) break;
                z += step_z;
                tMaxZ += delta_z;
                hit_face = (step_z > 0) ? BlockUtils::BlockFace::South : BlockUtils::BlockFace::North;
            }
        } else {
            if (tMaxY < tMaxZ) {
                t = tMaxY;
                if (t > ray.max_distance) break;
                y += step_y;
                tMaxY += delta_y;
                hit_face = (step_y > 0) ? BlockUtils::BlockFace::Down : BlockUtils::BlockFace::Up;
            } else {
                t = tMaxZ;
                if (t > ray.max_distance) break;
                z += step_z;
                tMaxZ += delta_z;
                hit_face = (step_z > 0) ? BlockUtils::BlockFace::South : BlockUtils::BlockFace::North;
            }
        }
        
        // Check the voxel we stepped into
        BlockPos block_pos(x, y, z);
        BlockType block_type = GetBlockCached(block_pos);
        if (should_stop(block_type)) {
            hit.hit = true;
            hit.block_pos = block_pos;
            hit.block_type = block_type;
            hit.face = hit_face;
            hit.distance = t;
            hit.hit_point = ray.origin + ray_dir_norm * hit.distance;
            hit.hit_normal = GetFaceNormal(hit_face);
            break;
        }
    }
    
    hit.steps_taken = steps;
    hit.computation_time_us = static_cast<float>(timer.ElapsedUs());
    if (update_stats) m_stats.Update(hit);
    return hit;
}

BlockUtils::BlockFace RaycastDDA::GetHitFace(const Vec3& hit_point, const BlockPos& block_pos) const {
    Vec3 block_center(static_cast<float>(block_pos.x) + 0.5f,
                     static_cast<float>(block_pos.y) + 0.5f,
                     static_cast<float>(block_pos.z) + 0.5f);
    
    Vec3 diff = hit_point - block_center;
    
    // Find the axis with maximum absolute difference
    float abs_x = std::abs(diff.x);
    float abs_y = std::abs(diff.y);
    float abs_z = std::abs(diff.z);
    
    if (abs_x > abs_y && abs_x > abs_z) {
        return (diff.x > 0) ? BlockUtils::BlockFace::East : BlockUtils::BlockFace::West;
    } else if (abs_y > abs_z) {
        return (diff.y > 0) ? BlockUtils::BlockFace::Up : BlockUtils::BlockFace::Down;
    } else {
        return (diff.z > 0) ? BlockUtils::BlockFace::South : BlockUtils::BlockFace::North;
    }
}

Vec3 RaycastDDA::GetFaceNormal(BlockUtils::BlockFace face) const {
    switch (face) {
    case BlockUtils::BlockFace::North: return Vec3(0, 0, 1);
    case BlockUtils::BlockFace::South: return Vec3(0, 0, -1);
        case BlockUtils::BlockFace::East:  return Vec3(1, 0, 0);
        case BlockUtils::BlockFace::West:  return Vec3(-1, 0, 0);
        case BlockUtils::BlockFace::Up:    return Vec3(0, 1, 0);
        case BlockUtils::BlockFace::Down:  return Vec3(0, -1, 0);
        default: return Vec3(0, 1, 0);
    }
}

BlockType RaycastDDA::GetBlockCached(const BlockPos& pos) const {
    if (m_config.cache_block_queries) {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        
        auto it = m_block_cache.find(pos);
        if (it != m_block_cache.end()) {
            return it->second;
        }
        
        BlockType type = m_world_manager->GetBlock(pos);
        m_block_cache[pos] = type;
        
        // Limit cache size
        if (m_block_cache.size() > 10000) {
            m_block_cache.clear();
        }
        
        return type;
    } else {
        return m_world_manager->GetBlock(pos);
    }
}

void RaycastDDA::ClearBlockCache() const {
    if (m_config.cache_block_queries) {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        m_block_cache.clear();
    }
}

// RaycastUtils implementation
namespace RaycastUtils {
    std::vector<Ray> GenerateRadialRays(const Vec3& center, float radius, 
                                       int32_t horizontal_count, int32_t vertical_count) {
        std::vector<Ray> rays;
        rays.reserve(horizontal_count * vertical_count);
        
        for (int32_t v = 0; v < vertical_count; v++) {
            float pitch = (static_cast<float>(v) / (vertical_count - 1) - 0.5f) * M_PI;
            
            for (int32_t h = 0; h < horizontal_count; h++) {
                float yaw = static_cast<float>(h) / horizontal_count * 2.0f * M_PI;
                
                Vec3 direction(
                    std::cos(pitch) * std::cos(yaw),
                    std::sin(pitch),
                    std::cos(pitch) * std::sin(yaw)
                );
                
                rays.emplace_back(center, direction, radius);
            }
        }
        
        return rays;
    }
    
    std::vector<Ray> GenerateGridRays(const Vec3& center, const Vec3& direction, 
                                     float width, float height, 
                                     int32_t horizontal_count, int32_t vertical_count) {
        std::vector<Ray> rays;
        rays.reserve(horizontal_count * vertical_count);
        
        // Create orthogonal basis
        Vec3 forward = direction.normalized();
        Vec3 up = Vec3(0, 1, 0);
        Vec3 right = Vec3(forward.z, 0, -forward.x).normalized();
        up = Vec3(forward.y * right.z - forward.z * right.y,
                 forward.z * right.x - forward.x * right.z,
                 forward.x * right.y - forward.y * right.x).normalized();
        
        float half_width = width * 0.5f;
        float half_height = height * 0.5f;
        
        for (int32_t v = 0; v < vertical_count; v++) {
            for (int32_t h = 0; h < horizontal_count; h++) {
                float u = (static_cast<float>(h) / (horizontal_count - 1) - 0.5f) * 2.0f;
                float v_coord = (static_cast<float>(v) / (vertical_count - 1) - 0.5f) * 2.0f;
                
                Vec3 offset = right * (u * half_width) + up * (v_coord * half_height);
                Vec3 ray_direction = (forward + offset * 0.1f).normalized(); // Slight divergence
                
                rays.emplace_back(center, ray_direction, 1000.0f);
            }
        }
        
        return rays;
    }
    
    std::vector<Ray> GenerateSphereRays(const Vec3& center, int32_t count) {
        std::vector<Ray> rays;
        rays.reserve(count);
        
        // Fibonacci sphere for deterministic uniform distribution
        const float golden_angle = static_cast<float>(M_PI) * (3.0f - std::sqrt(5.0f));
        for (int32_t i = 0; i < count; ++i) {
            float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
            float y = 1.0f - 2.0f * t;                 // y in [-1, 1]
            float r = std::sqrt(std::max(0.0f, 1.0f - y * y));
            float theta = golden_angle * static_cast<float>(i);
            float x = r * std::cos(theta);
            float z = r * std::sin(theta);
            Vec3 direction(x, y, z);
            direction = direction.normalized();
            rays.emplace_back(center, direction, 1000.0f);
        }
        return rays;
    }
    
    VisibilityAnalysis AnalyzeVisibility(const RaycastDDA& raycaster, 
                                        const Vec3& observer, 
                                        const std::vector<Ray>& rays) {
        VisibilityAnalysis analysis;
        analysis.total_rays = static_cast<int32_t>(rays.size());
        analysis.visible_rays = 0;
        analysis.avg_distance = 0.0f;
        analysis.ray_hits.reserve(rays.size());
        
        float total_distance = 0.0f;
        
        for (const auto& ray : rays) {
            Ray adjusted_ray(observer, ray.direction, ray.max_distance);
            RaycastHit hit = raycaster.Raycast(adjusted_ray);
            
            analysis.ray_hits.push_back(hit);
            
            if (hit.hit) {
                analysis.visible_rays++;
                total_distance += hit.distance;
            } else {
                total_distance += ray.max_distance;
            }
        }
        
        analysis.visibility_percentage = static_cast<float>(analysis.visible_rays) / analysis.total_rays * 100.0f;
        analysis.avg_distance = total_distance / analysis.total_rays;
        
        return analysis;
    }
    
    float CalculateShadowAttenuation(const RaycastDDA& raycaster,
                                    const Vec3& light_pos,
                                    const Vec3& target_pos) {
        return raycaster.GetLightAttenuation(light_pos, target_pos);
    }
    
    std::vector<BlockPos> CullOccludedBlocks(const RaycastDDA& raycaster,
                                           const Vec3& observer,
                                           const std::vector<BlockPos>& blocks) {
        return raycaster.GetVisibleBlocks(observer, blocks);
    }
    
    bool IsPathClear(const RaycastDDA& raycaster,
                     const Vec3& start, const Vec3& end,
                     float clearance_radius) {
        // Check center path
        if (!raycaster.HasLineOfSight(start, end, true)) {
            return false;
        }
        
        // Check paths around the clearance radius
        Vec3 direction = (end - start).normalized();
        Vec3 right = Vec3(direction.z, 0, -direction.x).normalized();
        Vec3 up = Vec3(0, 1, 0);
        
        // Check 4 corner paths
        for (int i = 0; i < 4; i++) {
            float angle = static_cast<float>(i) * M_PI * 0.5f;
            Vec3 offset = (right * std::cos(angle) + up * std::sin(angle)) * clearance_radius;
            
            if (!raycaster.HasLineOfSight(start + offset, end + offset, true)) {
                return false;
            }
        }
        
        return true;
    }
    
    std::vector<Vec3> GetObstaclesAlongPath(const RaycastDDA& raycaster,
                                          const Vec3& start, const Vec3& end) {
        std::vector<Vec3> obstacles;
        
        Vec3 direction = (end - start).normalized();
        float distance = (end - start).length();
        
        // Step along the path and check for obstacles
        float step_size = 1.0f;
        for (float d = 0; d < distance; d += step_size) {
            Vec3 check_pos = start + direction * d;
            
            // Check in a small radius around the path
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dz = -1; dz <= 1; dz++) {
                        Vec3 offset_pos = check_pos + Vec3(static_cast<float>(dx), 
                                                          static_cast<float>(dy), 
                                                          static_cast<float>(dz));
                        
                        BlockPos block_pos = offset_pos.to_block_pos();
                        if (BlockRegistry::Instance().IsSolid(raycaster.GetWorldManager().GetBlock(block_pos))) {
                            obstacles.push_back(Vec3(static_cast<float>(block_pos.x) + 0.5f,
                                                   static_cast<float>(block_pos.y) + 0.5f,
                                                   static_cast<float>(block_pos.z) + 0.5f));
                        }
                    }
                }
            }
        }
        
        // Remove duplicates
        std::sort(obstacles.begin(), obstacles.end(), [](const Vec3& a, const Vec3& b) {
            if (a.x != b.x) return a.x < b.x;
            if (a.y != b.y) return a.y < b.y;
            return a.z < b.z;
        });
        
        obstacles.erase(std::unique(obstacles.begin(), obstacles.end(), [](const Vec3& a, const Vec3& b) {
            return std::abs(a.x - b.x) < 0.1f && 
                   std::abs(a.y - b.y) < 0.1f && 
                   std::abs(a.z - b.z) < 0.1f;
        }), obstacles.end());
        
        return obstacles;
    }
}

} // namespace voxelvk