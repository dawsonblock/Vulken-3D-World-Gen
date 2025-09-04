#pragma once

#include "raycast_dda.hpp"
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace voxelvk {

// Enhanced raycast result with more detailed information
struct RaycastResult {
    bool hit = false;
    float distance = 0.0f;
    uint16_t block_type = 0;
    glm::vec3 hit_position = {0, 0, 0};
    glm::vec3 hit_normal = {0, 0, 0};
    glm::ivec3 hit_block_pos = {0, 0, 0};
    
    // Additional information
    int steps_taken = 0;
    float3 exact_hit_position; // CUDA float3 for GPU compatibility
};

// Ray structure for batch operations
struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
    
    Ray() = default;
    Ray(const glm::vec3& o, const glm::vec3& d) : origin(o), direction(d) {}
};

// Forward declaration for implementation
class GPURaycastDDAImpl;

// Enhanced GPU-accelerated DDA raycasting system
class GPURaycastDDA {
public:
    GPURaycastDDA();
    ~GPURaycastDDA();
    
    // Initialize with voxel data
    bool Initialize(const uint16_t* voxel_data, int width, int height, int depth);
    
    // Single ray operations
    RaycastResult Raycast(const Ray& ray, float max_distance = 1000.0f);
    bool IsVisible(const glm::vec3& from, const glm::vec3& to, float max_distance = 1000.0f);
    
    // Batch operations for high performance
    std::vector<RaycastResult> BatchRaycast(const std::vector<Ray>& rays, float max_distance = 1000.0f);
    std::vector<bool> BatchVisibilityTest(const std::vector<Ray>& rays, float max_distance = 1000.0f);
    
    // Update voxel data (for dynamic worlds)
    void UpdateVoxelData(const uint16_t* voxel_data, int x, int y, int z, 
                        int width, int height, int depth);
    
    // Performance statistics
    struct PerformanceStats {
        uint64_t total_rays_cast = 0;
        uint64_t total_hits = 0;
        float average_steps_per_ray = 0.0f;
        float average_raycast_time_ms = 0.0f;
        float rays_per_second = 0.0f;
    };
    
    PerformanceStats GetPerformanceStats() const;
    void ResetPerformanceStats();

private:
    std::unique_ptr<GPURaycastDDAImpl> impl_;
};

// Utility functions for common raycasting patterns
namespace raycast_utils {
    
    // Generate rays for frustum casting (for occlusion culling)
    std::vector<Ray> GenerateFrustumRays(const glm::vec3& camera_pos, 
                                        const glm::vec3& camera_dir,
                                        const glm::vec3& camera_up,
                                        float fov, float aspect_ratio,
                                        int ray_width = 16, int ray_height = 16);
    
    // Generate rays in a sphere pattern (for area visibility)
    std::vector<Ray> GenerateSphereRays(const glm::vec3& center, int num_rays);
    
    // Generate rays between two points (for line-of-sight checking)
    std::vector<Ray> GenerateLineRays(const glm::vec3& start, const glm::vec3& end, int num_samples);
    
    // Ray generation for shadow mapping
    std::vector<Ray> GenerateShadowRays(const glm::vec3& light_pos, 
                                       const std::vector<glm::vec3>& sample_points);
    
    // Generate random rays for Monte Carlo sampling
    std::vector<Ray> GenerateRandomRays(const glm::vec3& origin, int num_rays, 
                                       float min_distance = 1.0f, float max_distance = 100.0f);
}

// Specialized raycasting classes for different use cases

class OcclusionCuller {
public:
    OcclusionCuller(std::shared_ptr<GPURaycastDDA> raycast_system);
    
    // Test if objects are occluded from camera view
    std::vector<bool> TestOcclusion(const glm::vec3& camera_pos,
                                   const glm::vec3& camera_dir,
                                   const std::vector<glm::vec3>& object_positions,
                                   float max_distance = 1000.0f);
    
    // Hierarchical occlusion culling
    struct OcclusionNode {
        glm::vec3 bounds_min, bounds_max;
        std::vector<int> object_indices;
        std::vector<std::unique_ptr<OcclusionNode>> children;
        bool is_occluded = false;
    };
    
    void BuildOcclusionHierarchy(const std::vector<glm::vec3>& object_positions,
                                const std::vector<glm::vec3>& object_sizes);
    
    std::vector<int> CullOccludedObjects(const glm::vec3& camera_pos,
                                        const glm::vec3& camera_dir);

private:
    std::shared_ptr<GPURaycastDDA> raycast_system_;
    std::unique_ptr<OcclusionNode> occlusion_root_;
};

class LightingSystem {
public:
    LightingSystem(std::shared_ptr<GPURaycastDDA> raycast_system);
    
    // Calculate shadow casting
    std::vector<float> CalculateShadows(const glm::vec3& light_pos,
                                       const std::vector<glm::vec3>& sample_points);
    
    // Global illumination sampling
    std::vector<glm::vec3> SampleGlobalIllumination(const glm::vec3& surface_point,
                                                    const glm::vec3& surface_normal,
                                                    int num_samples = 64);
    
    // Volumetric lighting
    std::vector<float> CalculateVolumetricLighting(const glm::vec3& light_pos,
                                                   const glm::vec3& view_start,
                                                   const glm::vec3& view_end,
                                                   int num_samples = 32);

private:
    std::shared_ptr<GPURaycastDDA> raycast_system_;
};

class PhysicsRaycast {
public:
    PhysicsRaycast(std::shared_ptr<GPURaycastDDA> raycast_system);
    
    // Physics-based ray queries
    RaycastResult RaycastClosest(const Ray& ray, float max_distance = 1000.0f);
    std::vector<RaycastResult> RaycastAll(const Ray& ray, float max_distance = 1000.0f);
    
    // Sweep tests (moving objects)
    RaycastResult SweepTest(const glm::vec3& start, const glm::vec3& end,
                           const glm::vec3& object_size);
    
    // Overlap tests
    bool OverlapTest(const glm::vec3& center, const glm::vec3& size);
    
private:
    std::shared_ptr<GPURaycastDDA> raycast_system_;
};

// Performance-optimized raycast manager
class RaycastManager {
public:
    RaycastManager();
    ~RaycastManager();
    
    bool Initialize(const uint16_t* voxel_data, int width, int height, int depth);
    
    // Get specialized raycasting systems
    std::shared_ptr<GPURaycastDDA> GetBaseSystem() { return base_raycast_; }
    std::shared_ptr<OcclusionCuller> GetOcclusionCuller() { return occlusion_culler_; }
    std::shared_ptr<LightingSystem> GetLightingSystem() { return lighting_system_; }
    std::shared_ptr<PhysicsRaycast> GetPhysicsRaycast() { return physics_raycast_; }
    
    // Update systems when world changes
    void UpdateWorld(const uint16_t* voxel_data, int x, int y, int z,
                    int width, int height, int depth);
    
    // Performance monitoring
    struct SystemStats {
        GPURaycastDDA::PerformanceStats base_stats;
        uint64_t occlusion_queries = 0;
        uint64_t lighting_queries = 0;
        uint64_t physics_queries = 0;
        float total_gpu_utilization = 0.0f;
    };
    
    SystemStats GetSystemStats() const;
    void ResetStats();

private:
    std::shared_ptr<GPURaycastDDA> base_raycast_;
    std::shared_ptr<OcclusionCuller> occlusion_culler_;
    std::shared_ptr<LightingSystem> lighting_system_;
    std::shared_ptr<PhysicsRaycast> physics_raycast_;
};

} // namespace voxelvk