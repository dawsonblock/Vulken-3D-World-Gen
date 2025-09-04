#pragma once
#include "blocks.hpp"
#include "world_manager.hpp"
#include "../core/timer.hpp"
#include <array>
#include <vector>
#include <functional>
#include <cmath>

namespace voxelvk {

// 3D vector for ray calculations
struct Vec3 {
    float x, y, z;
    
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    
    Vec3 operator+(const Vec3& other) const { return Vec3(x + other.x, y + other.y, z + other.z); }
    Vec3 operator-(const Vec3& other) const { return Vec3(x - other.x, y - other.y, z - other.z); }
    Vec3 operator*(float scalar) const { return Vec3(x * scalar, y * scalar, z * scalar); }
    Vec3 operator/(float scalar) const { return Vec3(x / scalar, y / scalar, z / scalar); }
    
    float dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    float length_squared() const { return x * x + y * y + z * z; }
    
    Vec3 normalized() const {
        float len = length();
        return (len > 0.0f) ? (*this / len) : Vec3(0, 0, 0);
    }
    
    BlockPos to_block_pos() const {
        return BlockPos(static_cast<int32_t>(std::floor(x)),
                       static_cast<int32_t>(std::floor(y)),
                       static_cast<int32_t>(std::floor(z)));
    }
};

// Ray structure
struct Ray {
    Vec3 origin;
    Vec3 direction;  // Should be normalized
    float max_distance = 1000.0f;
    
    Ray() = default;
    Ray(const Vec3& orig, const Vec3& dir, float max_dist = 1000.0f)
        : origin(orig), direction(dir.normalized()), max_distance(max_dist) {}
        
    Vec3 at(float t) const { return origin + direction * t; }
};

// Raycast hit result
struct RaycastHit {
    bool hit = false;
    BlockPos block_pos;
    BlockType block_type = BlockType::Air;
    Vec3 hit_point;
    Vec3 hit_normal;
    float distance = 0.0f;
    BlockUtils::BlockFace face = BlockUtils::BlockFace::North;
    
    // For debugging/analysis
    int32_t steps_taken = 0;
    float computation_time_us = 0.0f;
};

// Batch raycast request
struct RaycastRequest {
    Ray ray;
    uint32_t request_id = 0;  // For tracking in batched operations
    void* user_data = nullptr; // Custom user data
    
    // Filtering options
    bool ignore_transparent = false;
    bool ignore_fluids = false;
    std::function<bool(BlockType)> block_filter; // Custom block filter
};

// DDA (Digital Differential Analyzer) raycast implementation
class RaycastDDA {
public:
    explicit RaycastDDA(WorldManager* world_manager);
    
    // Single raycast
    RaycastHit Raycast(const Ray& ray) const;
    RaycastHit Raycast(const Vec3& origin, const Vec3& direction, float max_distance = 1000.0f) const;
    
    // Raycast with custom block filter
    RaycastHit Raycast(const Ray& ray, std::function<bool(BlockType)> block_filter) const;
    
    // Batch raycast operations
    std::vector<RaycastHit> BatchRaycast(const std::vector<RaycastRequest>& requests) const;
    void BatchRaycastAsync(const std::vector<RaycastRequest>& requests, 
                          std::function<void(const std::vector<RaycastHit>&)> callback) const;
    
    // Specialized raycast functions
    RaycastHit RaycastIgnoreTransparent(const Ray& ray) const;
    RaycastHit RaycastIgnoreFluids(const Ray& ray) const;
    RaycastHit RaycastSolidsOnly(const Ray& ray) const;
    
    // Line of sight checks
    bool HasLineOfSight(const Vec3& from, const Vec3& to, bool ignore_transparent = false) const;
    bool HasLineOfSight(const BlockPos& from, const BlockPos& to, bool ignore_transparent = false) const;
    
    // Visibility/occlusion queries
    std::vector<BlockPos> GetVisibleBlocks(const Vec3& observer, const std::vector<BlockPos>& targets, 
                                          float max_distance = 1000.0f) const;
    
    // Ray tracing for lighting/shadows
    float GetLightAttenuation(const Vec3& light_pos, const Vec3& target_pos) const;
    
    // Configuration
    struct Config {
        float step_epsilon = 1e-6f;      // Small epsilon for floating point comparisons
        int32_t max_steps = 10000;       // Maximum DDA steps to prevent infinite loops
        bool early_exit_air = true;      // Stop early when hitting air blocks
        bool interpolate_normals = false; // Interpolate hit normals for smoother results
        bool cache_block_queries = true; // Cache block queries for performance
    };
    
    void SetConfig(const Config& config) { m_config = config; }
    const Config& GetConfig() const { return m_config; }
    
    // Performance monitoring
    struct Stats {
        uint64_t total_rays_cast = 0;
        uint64_t total_steps_taken = 0;
        uint64_t total_hits = 0;
        uint64_t total_misses = 0;
        double total_time_ms = 0.0;
        double avg_time_per_ray_us = 0.0;
        double avg_steps_per_ray = 0.0;
        
        void Reset() {
            total_rays_cast = 0;
            total_steps_taken = 0;
            total_hits = 0;
            total_misses = 0;
            total_time_ms = 0.0;
            avg_time_per_ray_us = 0.0;
            avg_steps_per_ray = 0.0;
        }
        
        void Update(const RaycastHit& hit) {
            total_rays_cast++;
            total_steps_taken += hit.steps_taken;
            total_time_ms += hit.computation_time_us / 1000.0;
            
            if (hit.hit) {
                total_hits++;
            } else {
                total_misses++;
            }
            
            avg_time_per_ray_us = (total_time_ms * 1000.0) / total_rays_cast;
            avg_steps_per_ray = static_cast<double>(total_steps_taken) / total_rays_cast;
        }
    };
    
    const Stats& GetStats() const { return m_stats; }
    void ResetStats() { m_stats.Reset(); }
    // Accessor for utilities that need world context
    const WorldManager& GetWorldManager() const { return *m_world_manager; }
    
private:
    WorldManager* m_world_manager;
    Config m_config;
    mutable Stats m_stats;
    
    // Core DDA implementation
    RaycastHit PerformDDA(const Ray& ray, std::function<bool(BlockType)> should_stop, bool update_stats = true) const;
    
    // Helper functions
    BlockUtils::BlockFace GetHitFace(const Vec3& hit_point, const BlockPos& block_pos) const;
    Vec3 GetFaceNormal(BlockUtils::BlockFace face) const;
    bool ShouldStopAtBlock(BlockType type, const RaycastRequest& request) const;
    
    // Optimization helpers
    mutable std::unordered_map<BlockPos, BlockType> m_block_cache;
    mutable std::mutex m_cache_mutex;
    
    BlockType GetBlockCached(const BlockPos& pos) const;
    void ClearBlockCache() const;
};

// Specialized raycast utilities
namespace RaycastUtils {
    // Generate rays for common patterns
    std::vector<Ray> GenerateRadialRays(const Vec3& center, float radius, 
                                       int32_t horizontal_count, int32_t vertical_count);
    
    std::vector<Ray> GenerateGridRays(const Vec3& center, const Vec3& direction, 
                                     float width, float height, 
                                     int32_t horizontal_count, int32_t vertical_count);
    
    std::vector<Ray> GenerateSphereRays(const Vec3& center, int32_t count);
    
    // Visibility analysis
    struct VisibilityAnalysis {
        float visibility_percentage;
        int32_t visible_rays;
        int32_t total_rays;
        float avg_distance;
        std::vector<RaycastHit> ray_hits;
    };
    
    VisibilityAnalysis AnalyzeVisibility(const RaycastDDA& raycaster, 
                                        const Vec3& observer, 
                                        const std::vector<Ray>& rays);
    
    // Shadow/lighting helpers
    float CalculateShadowAttenuation(const RaycastDDA& raycaster,
                                    const Vec3& light_pos,
                                    const Vec3& target_pos);
    
    // Occlusion culling
    std::vector<BlockPos> CullOccludedBlocks(const RaycastDDA& raycaster,
                                           const Vec3& observer,
                                           const std::vector<BlockPos>& blocks);
    
    // Path finding helpers
    bool IsPathClear(const RaycastDDA& raycaster,
                     const Vec3& start, const Vec3& end,
                     float clearance_radius = 0.5f);
    
    std::vector<Vec3> GetObstaclesAlongPath(const RaycastDDA& raycaster,
                                          const Vec3& start, const Vec3& end);
}

// GPU-accelerated batch raycast (future extension)
#ifdef VXL_ENABLE_CUDA
class GPURaycastDDA {
public:
    explicit GPURaycastDDA(WorldManager* world_manager);
    ~GPURaycastDDA();
    
    // Batch raycast on GPU
    std::vector<RaycastHit> BatchRaycast(const std::vector<RaycastRequest>& requests) const;
    
    // Async GPU raycast
    std::future<std::vector<RaycastHit>> BatchRaycastAsync(const std::vector<RaycastRequest>& requests) const;
    
    // Configuration
    void SetBlockSize(int32_t block_size) { m_block_size = block_size; }
    void SetGridSize(int32_t grid_size) { m_grid_size = grid_size; }
    
private:
    WorldManager* m_world_manager;
    int32_t m_block_size = 256;
    int32_t m_grid_size = 64;
    
    // CUDA resources
    void* m_device_rays = nullptr;
    void* m_device_hits = nullptr;
    void* m_device_world_data = nullptr;
    
    void InitializeCUDA();
    void CleanupCUDA();
    void UploadWorldData() const;
};
#endif

} // namespace voxelvk