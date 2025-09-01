// CUDA implementation of GPU-accelerated DDA raycasting
#include "raycast_dda.hpp"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <stdio.h>

namespace voxelvk {

// CUDA kernel for batch DDA raycasting
__device__ __forceinline__ int3 make_int3_floor(float3 f) {
    return make_int3(floorf(f.x), floorf(f.y), floorf(f.z));
}

__device__ __forceinline__ float3 make_float3_abs(float3 f) {
    return make_float3(fabsf(f.x), fabsf(f.y), fabsf(f.z));
}

__device__ __forceinline__ int3 make_int3_sign(float3 f) {
    return make_int3((f.x > 0) ? 1 : -1, (f.y > 0) ? 1 : -1, (f.z > 0) ? 1 : -1);
}

__device__ uint16_t sampleVoxelGrid(const uint16_t* voxel_data, int3 world_size, int3 pos) {
    if (pos.x < 0 || pos.x >= world_size.x ||
        pos.y < 0 || pos.y >= world_size.y ||
        pos.z < 0 || pos.z >= world_size.z) {
        return 0; // Out of bounds - air
    }
    
    int index = pos.z * world_size.y * world_size.x + pos.y * world_size.x + pos.x;
    return voxel_data[index];
}

__global__ void batchDDARaycast(
    const float3* ray_origins,
    const float3* ray_directions,
    const uint16_t* voxel_data,
    int3 world_size,
    float max_distance,
    int num_rays,
    RaycastResult* results
) {
    int ray_id = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (ray_id >= num_rays) {
        return;
    }
    
    // Initialize result
    RaycastResult& result = results[ray_id];
    result.hit = false;
    result.distance = max_distance;
    result.block_type = 0;
    result.hit_position = make_float3(0, 0, 0);
    result.hit_normal = make_float3(0, 0, 0);
    
    float3 ray_origin = ray_origins[ray_id];
    float3 ray_direction = ray_directions[ray_id];
    
    // Normalize ray direction
    float dir_length = sqrtf(ray_direction.x * ray_direction.x + 
                            ray_direction.y * ray_direction.y + 
                            ray_direction.z * ray_direction.z);
    if (dir_length < 1e-6f) {
        return; // Invalid ray direction
    }
    
    ray_direction.x /= dir_length;
    ray_direction.y /= dir_length;
    ray_direction.z /= dir_length;
    
    // DDA setup
    int3 map_pos = make_int3_floor(ray_origin);
    float3 delta_dist = make_float3_abs(make_float3(1.0f / ray_direction.x, 
                                                   1.0f / ray_direction.y, 
                                                   1.0f / ray_direction.z));
    
    int3 step = make_int3_sign(ray_direction);
    
    float3 side_dist;
    if (ray_direction.x < 0) {
        side_dist.x = (ray_origin.x - map_pos.x) * delta_dist.x;
    } else {
        side_dist.x = (map_pos.x + 1.0f - ray_origin.x) * delta_dist.x;
    }
    
    if (ray_direction.y < 0) {
        side_dist.y = (ray_origin.y - map_pos.y) * delta_dist.y;
    } else {
        side_dist.y = (map_pos.y + 1.0f - ray_origin.y) * delta_dist.y;
    }
    
    if (ray_direction.z < 0) {
        side_dist.z = (ray_origin.z - map_pos.z) * delta_dist.z;
    } else {
        side_dist.z = (map_pos.z + 1.0f - ray_origin.z) * delta_dist.z;
    }
    
    // DDA algorithm
    int max_steps = (int)(max_distance * 2); // Prevent infinite loops
    int side = 0; // Which side was hit (0=X, 1=Y, 2=Z)
    
    for (int step_count = 0; step_count < max_steps; step_count++) {
        // Sample voxel at current position
        uint16_t block_type = sampleVoxelGrid(voxel_data, world_size, map_pos);
        
        if (block_type != 0) {
            // Hit solid block
            result.hit = true;
            result.block_type = block_type;
            result.hit_block_pos = map_pos;
            
            // Calculate hit position and distance
            float3 hit_pos;
            if (side == 0) {
                result.distance = (map_pos.x - ray_origin.x + (1 - step.x) / 2) / ray_direction.x;
                hit_pos.x = map_pos.x + (1 - step.x) / 2;
                hit_pos.y = ray_origin.y + result.distance * ray_direction.y;
                hit_pos.z = ray_origin.z + result.distance * ray_direction.z;
                result.hit_normal = make_float3(-step.x, 0, 0);
            } else if (side == 1) {
                result.distance = (map_pos.y - ray_origin.y + (1 - step.y) / 2) / ray_direction.y;
                hit_pos.x = ray_origin.x + result.distance * ray_direction.x;
                hit_pos.y = map_pos.y + (1 - step.y) / 2;
                hit_pos.z = ray_origin.z + result.distance * ray_direction.z;
                result.hit_normal = make_float3(0, -step.y, 0);
            } else {
                result.distance = (map_pos.z - ray_origin.z + (1 - step.z) / 2) / ray_direction.z;
                hit_pos.x = ray_origin.x + result.distance * ray_direction.x;
                hit_pos.y = ray_origin.y + result.distance * ray_direction.y;
                hit_pos.z = map_pos.z + (1 - step.z) / 2;
                result.hit_normal = make_float3(0, 0, -step.z);
            }
            
            result.hit_position = hit_pos;
            result.distance = fabsf(result.distance);
            break;
        }
        
        // Step to next voxel
        if (side_dist.x < side_dist.y) {
            if (side_dist.x < side_dist.z) {
                side_dist.x += delta_dist.x;
                map_pos.x += step.x;
                side = 0;
            } else {
                side_dist.z += delta_dist.z;
                map_pos.z += step.z;
                side = 2;
            }
        } else {
            if (side_dist.y < side_dist.z) {
                side_dist.y += delta_dist.y;
                map_pos.y += step.y;
                side = 1;
            } else {
                side_dist.z += delta_dist.z;
                map_pos.z += step.z;
                side = 2;
            }
        }
        
        // Check if we've exceeded max distance
        float current_distance = sqrtf((map_pos.x - ray_origin.x) * (map_pos.x - ray_origin.x) +
                                      (map_pos.y - ray_origin.y) * (map_pos.y - ray_origin.y) +
                                      (map_pos.z - ray_origin.z) * (map_pos.z - ray_origin.z));
        
        if (current_distance > max_distance) {
            break;
        }
    }
}

// Optimized kernel for visibility queries (returns only hit/miss)
__global__ void batchVisibilityTest(
    const float3* ray_origins,
    const float3* ray_directions,
    const uint16_t* voxel_data,
    int3 world_size,
    float max_distance,
    int num_rays,
    bool* visibility_results
) {
    int ray_id = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (ray_id >= num_rays) {
        return;
    }
    
    visibility_results[ray_id] = true; // Assume visible unless blocked
    
    float3 ray_origin = ray_origins[ray_id];
    float3 ray_direction = ray_directions[ray_id];
    
    // Normalize ray direction
    float dir_length = sqrtf(ray_direction.x * ray_direction.x + 
                            ray_direction.y * ray_direction.y + 
                            ray_direction.z * ray_direction.z);
    if (dir_length < 1e-6f) {
        visibility_results[ray_id] = false;
        return;
    }
    
    ray_direction.x /= dir_length;
    ray_direction.y /= dir_length;
    ray_direction.z /= dir_length;
    
    // Simplified DDA for visibility (similar to above but early exit)
    int3 map_pos = make_int3_floor(ray_origin);
    float3 delta_dist = make_float3_abs(make_float3(1.0f / ray_direction.x, 
                                                   1.0f / ray_direction.y, 
                                                   1.0f / ray_direction.z));
    
    int3 step = make_int3_sign(ray_direction);
    
    float3 side_dist;
    if (ray_direction.x < 0) {
        side_dist.x = (ray_origin.x - map_pos.x) * delta_dist.x;
    } else {
        side_dist.x = (map_pos.x + 1.0f - ray_origin.x) * delta_dist.x;
    }
    
    if (ray_direction.y < 0) {
        side_dist.y = (ray_origin.y - map_pos.y) * delta_dist.y;
    } else {
        side_dist.y = (map_pos.y + 1.0f - ray_origin.y) * delta_dist.y;
    }
    
    if (ray_direction.z < 0) {
        side_dist.z = (ray_origin.z - map_pos.z) * delta_dist.z;
    } else {
        side_dist.z = (map_pos.z + 1.0f - ray_origin.z) * delta_dist.z;
    }
    
    int max_steps = (int)(max_distance * 2);
    
    for (int step_count = 0; step_count < max_steps; step_count++) {
        uint16_t block_type = sampleVoxelGrid(voxel_data, world_size, map_pos);
        
        if (block_type != 0) {
            visibility_results[ray_id] = false; // Blocked
            return;
        }
        
        // Step to next voxel
        if (side_dist.x < side_dist.y) {
            if (side_dist.x < side_dist.z) {
                side_dist.x += delta_dist.x;
                map_pos.x += step.x;
            } else {
                side_dist.z += delta_dist.z;
                map_pos.z += step.z;
            }
        } else {
            if (side_dist.y < side_dist.z) {
                side_dist.y += delta_dist.y;
                map_pos.y += step.y;
            } else {
                side_dist.z += delta_dist.z;
                map_pos.z += step.z;
            }
        }
        
        // Check distance
        float current_distance = sqrtf((map_pos.x - ray_origin.x) * (map_pos.x - ray_origin.x) +
                                      (map_pos.y - ray_origin.y) * (map_pos.y - ray_origin.y) +
                                      (map_pos.z - ray_origin.z) * (map_pos.z - ray_origin.z));
        
        if (current_distance > max_distance) {
            break; // Reached max distance without hitting anything
        }
    }
}

// Host-side GPU raycast implementation
class GPURaycastDDAImpl {
public:
    GPURaycastDDAImpl() {
        // Allocate device memory
        cudaMalloc(&d_ray_origins_, MAX_BATCH_SIZE * sizeof(float3));
        cudaMalloc(&d_ray_directions_, MAX_BATCH_SIZE * sizeof(float3));
        cudaMalloc(&d_results_, MAX_BATCH_SIZE * sizeof(RaycastResult));
        cudaMalloc(&d_visibility_results_, MAX_BATCH_SIZE * sizeof(bool));
        
        // Create CUDA streams for async operations
        cudaStreamCreate(&cuda_stream_);
        
        printf("GPU DDA Raycast initialized\n");
    }
    
    ~GPURaycastDDAImpl() {
        // Free device memory
        cudaFree(d_ray_origins_);
        cudaFree(d_ray_directions_);
        cudaFree(d_results_);
        cudaFree(d_visibility_results_);
        cudaFree(d_voxel_data_);
        
        cudaStreamDestroy(cuda_stream_);
    }
    
    bool setVoxelData(const uint16_t* voxel_data, int width, int height, int depth) {
        world_size_ = make_int3(width, height, depth);
        size_t data_size = width * height * depth * sizeof(uint16_t);
        
        // Free existing data
        if (d_voxel_data_) {
            cudaFree(d_voxel_data_);
        }
        
        // Allocate and copy new data
        cudaError_t result = cudaMalloc(&d_voxel_data_, data_size);
        if (result != cudaSuccess) {
            printf("Failed to allocate GPU memory for voxel data: %s\n", cudaGetErrorString(result));
            return false;
        }
        
        result = cudaMemcpy(d_voxel_data_, voxel_data, data_size, cudaMemcpyHostToDevice);
        if (result != cudaSuccess) {
            printf("Failed to copy voxel data to GPU: %s\n", cudaGetErrorString(result));
            return false;
        }
        
        return true;
    }
    
    std::vector<RaycastResult> batchRaycast(const std::vector<Ray>& rays, float max_distance) {
        int num_rays = static_cast<int>(rays.size());
        if (num_rays == 0) {
            return {};
        }
        
        // Limit batch size
        num_rays = std::min(num_rays, MAX_BATCH_SIZE);
        std::vector<RaycastResult> results(num_rays);
        
        // Prepare input data
        std::vector<float3> origins(num_rays);
        std::vector<float3> directions(num_rays);
        
        for (int i = 0; i < num_rays; ++i) {
            origins[i] = make_float3(rays[i].origin.x, rays[i].origin.y, rays[i].origin.z);
            directions[i] = make_float3(rays[i].direction.x, rays[i].direction.y, rays[i].direction.z);
        }
        
        // Copy input data to GPU
        cudaMemcpyAsync(d_ray_origins_, origins.data(), num_rays * sizeof(float3), 
                       cudaMemcpyHostToDevice, cuda_stream_);
        cudaMemcpyAsync(d_ray_directions_, directions.data(), num_rays * sizeof(float3), 
                       cudaMemcpyHostToDevice, cuda_stream_);
        
        // Launch kernel
        int block_size = 256;
        int grid_size = (num_rays + block_size - 1) / block_size;
        
        batchDDARaycast<<<grid_size, block_size, 0, cuda_stream_>>>(
            d_ray_origins_,
            d_ray_directions_,
            d_voxel_data_,
            world_size_,
            max_distance,
            num_rays,
            d_results_
        );
        
        // Check for kernel launch errors
        cudaError_t launch_result = cudaGetLastError();
        if (launch_result != cudaSuccess) {
            printf("Kernel launch failed: %s\n", cudaGetErrorString(launch_result));
            return results;
        }
        
        // Copy results back to host
        cudaMemcpyAsync(results.data(), d_results_, num_rays * sizeof(RaycastResult), 
                       cudaMemcpyDeviceToHost, cuda_stream_);
        
        // Synchronize to ensure completion
        cudaStreamSynchronize(cuda_stream_);
        
        return results;
    }
    
    std::vector<bool> batchVisibilityTest(const std::vector<Ray>& rays, float max_distance) {
        int num_rays = static_cast<int>(rays.size());
        if (num_rays == 0) {
            return {};
        }
        
        num_rays = std::min(num_rays, MAX_BATCH_SIZE);
        std::vector<bool> visibility_results(num_rays);
        
        // Prepare input data
        std::vector<float3> origins(num_rays);
        std::vector<float3> directions(num_rays);
        
        for (int i = 0; i < num_rays; ++i) {
            origins[i] = make_float3(rays[i].origin.x, rays[i].origin.y, rays[i].origin.z);
            directions[i] = make_float3(rays[i].direction.x, rays[i].direction.y, rays[i].direction.z);
        }
        
        // Copy to GPU
        cudaMemcpyAsync(d_ray_origins_, origins.data(), num_rays * sizeof(float3), 
                       cudaMemcpyHostToDevice, cuda_stream_);
        cudaMemcpyAsync(d_ray_directions_, directions.data(), num_rays * sizeof(float3), 
                       cudaMemcpyHostToDevice, cuda_stream_);
        
        // Launch visibility kernel
        int block_size = 256;
        int grid_size = (num_rays + block_size - 1) / block_size;
        
        batchVisibilityTest<<<grid_size, block_size, 0, cuda_stream_>>>(
            d_ray_origins_,
            d_ray_directions_,
            d_voxel_data_,
            world_size_,
            max_distance,
            num_rays,
            d_visibility_results_
        );
        
        // Copy results back
        cudaMemcpyAsync(visibility_results.data(), d_visibility_results_, num_rays * sizeof(bool), 
                       cudaMemcpyDeviceToHost, cuda_stream_);
        
        cudaStreamSynchronize(cuda_stream_);
        
        return visibility_results;
    }
    
    void updateVoxelRegion(const uint16_t* voxel_data, int x, int y, int z, 
                          int width, int height, int depth) {
        // Update a region of the voxel data on GPU
        if (!d_voxel_data_) {
            return;
        }
        
        // Calculate memory offset and size
        size_t offset = (z * world_size_.y * world_size_.x + y * world_size_.x + x) * sizeof(uint16_t);
        size_t region_size = width * height * depth * sizeof(uint16_t);
        
        // For simplicity, copy the entire region as a contiguous block
        // In practice, you might want to copy row by row for non-contiguous regions
        if (width == world_size_.x && height == world_size_.y) {
            // Contiguous region - can copy directly
            cudaMemcpyAsync((uint8_t*)d_voxel_data_ + offset, voxel_data, region_size, 
                           cudaMemcpyHostToDevice, cuda_stream_);
        } else {
            // Non-contiguous region - copy slice by slice
            for (int dz = 0; dz < depth; ++dz) {
                for (int dy = 0; dy < height; ++dy) {
                    size_t src_offset = (dz * height * width + dy * width) * sizeof(uint16_t);
                    size_t dst_offset = ((z + dz) * world_size_.y * world_size_.x + 
                                        (y + dy) * world_size_.x + x) * sizeof(uint16_t);
                    
                    cudaMemcpyAsync((uint8_t*)d_voxel_data_ + dst_offset, 
                                   (const uint8_t*)voxel_data + src_offset, 
                                   width * sizeof(uint16_t), 
                                   cudaMemcpyHostToDevice, cuda_stream_);
                }
            }
        }
    }
    
    static constexpr int MAX_BATCH_SIZE = 65536; // Maximum rays per batch
    
private:
    // Device memory
    float3* d_ray_origins_ = nullptr;
    float3* d_ray_directions_ = nullptr;
    RaycastResult* d_results_ = nullptr;
    bool* d_visibility_results_ = nullptr;
    uint16_t* d_voxel_data_ = nullptr;
    
    // World parameters
    int3 world_size_;
    
    // CUDA resources
    cudaStream_t cuda_stream_;
};

// GPURaycastDDA implementation
GPURaycastDDA::GPURaycastDDA() : impl_(std::make_unique<GPURaycastDDAImpl>()) {}

GPURaycastDDA::~GPURaycastDDA() = default;

bool GPURaycastDDA::Initialize(const uint16_t* voxel_data, int width, int height, int depth) {
    return impl_->setVoxelData(voxel_data, width, height, depth);
}

RaycastResult GPURaycastDDA::Raycast(const Ray& ray, float max_distance) {
    std::vector<Ray> rays = {ray};
    auto results = impl_->batchRaycast(rays, max_distance);
    return results.empty() ? RaycastResult{} : results[0];
}

std::vector<RaycastResult> GPURaycastDDA::BatchRaycast(const std::vector<Ray>& rays, float max_distance) {
    return impl_->batchRaycast(rays, max_distance);
}

std::vector<bool> GPURaycastDDA::BatchVisibilityTest(const std::vector<Ray>& rays, float max_distance) {
    return impl_->batchVisibilityTest(rays, max_distance);
}

void GPURaycastDDA::UpdateVoxelData(const uint16_t* voxel_data, int x, int y, int z, 
                                   int width, int height, int depth) {
    impl_->updateVoxelRegion(voxel_data, x, y, z, width, height, depth);
}

bool GPURaycastDDA::IsVisible(const glm::vec3& from, const glm::vec3& to, float max_distance) {
    glm::vec3 direction = to - from;
    float distance = glm::length(direction);
    
    if (distance > max_distance) {
        return false;
    }
    
    direction /= distance;
    
    Ray ray;
    ray.origin = from;
    ray.direction = direction;
    
    std::vector<Ray> rays = {ray};
    auto results = impl_->batchVisibilityTest(rays, distance);
    
    return results.empty() ? false : results[0];
}

} // namespace voxelvk