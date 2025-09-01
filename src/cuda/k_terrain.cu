// CUDA kernel for terrain generation using FBM (Fractional Brownian Motion) and caves
#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <cstdint>

// Block constants
#define BLOCK_AIR 0
#define BLOCK_STONE 1
#define BLOCK_DIRT 2
#define BLOCK_GRASS 3
#define BLOCK_SAND 4
#define BLOCK_WATER 5
#define BLOCK_BEDROCK 9

// Noise functions
__device__ float hash(float n) {
    return frac(sin(n) * 1e4f);
}

__device__ float frac(float x) {
    return x - floorf(x);
}

__device__ float noise(float x, float y, float z) {
    // 3D Perlin-like noise
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);
    int iz = (int)floorf(z);
    
    float fx = frac(x);
    float fy = frac(y);
    float fz = frac(z);
    
    // Smooth interpolation
    float u = fx * fx * (3.0f - 2.0f * fx);
    float v = fy * fy * (3.0f - 2.0f * fy);
    float w = fz * fz * (3.0f - 2.0f * fz);
    
    // Hash function for pseudo-random values
    auto hash3d = [](int x, int y, int z) -> float {
        int n = x + y * 57 + z * 113;
        n = (n << 13) ^ n;
        return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
    };
    
    // Sample corner values
    float c000 = hash3d(ix,     iy,     iz);
    float c100 = hash3d(ix + 1, iy,     iz);
    float c010 = hash3d(ix,     iy + 1, iz);
    float c110 = hash3d(ix + 1, iy + 1, iz);
    float c001 = hash3d(ix,     iy,     iz + 1);
    float c101 = hash3d(ix + 1, iy,     iz + 1);
    float c011 = hash3d(ix,     iy + 1, iz + 1);
    float c111 = hash3d(ix + 1, iy + 1, iz + 1);
    
    // Trilinear interpolation
    float i1 = lerp(c000, c100, u);
    float i2 = lerp(c010, c110, u);
    float i3 = lerp(c001, c101, u);
    float i4 = lerp(c011, c111, u);
    
    float j1 = lerp(i1, i2, v);
    float j2 = lerp(i3, i4, v);
    
    return lerp(j1, j2, w);
}

__device__ float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

__device__ float fbm(float x, float y, float z, int octaves, float persistence, float scale) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = scale;
    float max_value = 0.0f;
    
    for (int i = 0; i < octaves; i++) {
        value += noise(x * frequency, y * frequency, z * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }
    
    return value / max_value;
}

// Biome determination
__device__ int getBiome(float temperature, float humidity) {
    // Simple biome classification
    if (temperature < -0.5f) return 5; // Tundra/Snow
    if (temperature > 1.5f && humidity < 0.2f) return 1; // Desert
    if (humidity > 0.8f && temperature > 0.5f) return 2; // Forest
    if (temperature < 0.2f) return 3; // Mountains
    if (humidity > 0.9f) return 4; // Ocean/Swamp
    return 0; // Plains
}

// Terrain generation parameters
struct TerrainParams {
    int32_t chunk_x, chunk_y, chunk_z;
    int32_t chunk_size;
    int32_t world_height;
    int32_t sea_level;
    float terrain_scale;
    float height_variation;
    float cave_threshold;
    float biome_scale;
    float temperature_scale;
    float humidity_scale;
    int32_t bedrock_height;
    int32_t dirt_depth;
    uint32_t seed;
};

// CUDA kernel for chunk generation
__global__ void generateTerrainKernel(
    uint16_t* blocks,           // Output block array [chunk_size^3]
    uint8_t* metadata,          // Output metadata array
    float* heightmap,           // Output heightmap [chunk_size^2]
    uint8_t* biome_map,         // Output biome map [chunk_size^2]
    TerrainParams params
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int idy = blockIdx.y * blockDim.y + threadIdx.y;
    int idz = blockIdx.z * blockDim.z + threadIdx.z;
    
    if (idx >= params.chunk_size || idy >= params.chunk_size || idz >= params.chunk_size) {
        return;
    }
    
    // World coordinates
    float world_x = (params.chunk_x * params.chunk_size + idx) + 0.5f;
    float world_y = (params.chunk_y * params.chunk_size + idy) + 0.5f;
    float world_z = (params.chunk_z * params.chunk_size + idz) + 0.5f;
    
    // Linear index
    int index = idy * params.chunk_size * params.chunk_size + idz * params.chunk_size + idx;
    int heightmap_index = idz * params.chunk_size + idx;
    
    // Initialize to air
    blocks[index] = BLOCK_AIR;
    metadata[index] = 0;
    
    // Generate height using FBM
    float height_noise = fbm(world_x * params.terrain_scale, 
                            world_z * params.terrain_scale, 0.0f,
                            4, 0.5f, 1.0f);
    float terrain_height = params.sea_level + height_noise * params.height_variation;
    
    // Generate biome data
    float temperature = fbm(world_x * params.temperature_scale, 
                           world_z * params.temperature_scale, 1000.0f,
                           3, 0.6f, 1.0f);
    float humidity = fbm(world_x * params.humidity_scale, 
                        world_z * params.humidity_scale, 2000.0f,
                        3, 0.6f, 1.0f);
    
    int biome = getBiome(temperature, humidity);
    
    // Store heightmap and biome (only for surface layer)
    if (idy == 0) {
        heightmap[heightmap_index] = terrain_height;
        biome_map[heightmap_index] = biome;
    }
    
    // Generate caves
    float cave_noise1 = fbm(world_x * 0.02f, world_y * 0.02f, world_z * 0.02f, 3, 0.5f, 1.0f);
    float cave_noise2 = fbm(world_x * 0.03f, world_y * 0.03f, world_z * 0.03f, 3, 0.5f, 1.0f);
    bool is_cave = (cave_noise1 > params.cave_threshold && cave_noise2 > params.cave_threshold);
    
    // Determine block type based on height and biome
    if (world_y <= params.bedrock_height) {
        // Bedrock layer
        blocks[index] = BLOCK_BEDROCK;
    } else if (world_y <= terrain_height && !is_cave) {
        // Solid terrain
        if (world_y > terrain_height - 1.0f) {
            // Surface block
            switch (biome) {
                case 0: blocks[index] = BLOCK_GRASS; break;  // Plains
                case 1: blocks[index] = BLOCK_SAND; break;   // Desert
                case 2: blocks[index] = BLOCK_GRASS; break;  // Forest
                case 3: blocks[index] = BLOCK_STONE; break;  // Mountains
                case 4: blocks[index] = BLOCK_DIRT; break;   // Swamp
                case 5: blocks[index] = BLOCK_DIRT; break;   // Tundra
                default: blocks[index] = BLOCK_GRASS; break;
            }
        } else if (world_y > terrain_height - params.dirt_depth) {
            // Dirt layer
            blocks[index] = BLOCK_DIRT;
        } else {
            // Stone layer
            blocks[index] = BLOCK_STONE;
        }
    } else if (world_y <= params.sea_level) {
        // Water level
        blocks[index] = BLOCK_WATER;
    }
    // else remains air
}

// CUDA kernel for ore generation
__global__ void generateOresKernel(
    uint16_t* blocks,
    TerrainParams params,
    float ore_frequency,
    int ore_type,
    float depth_min,
    float depth_max
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int idy = blockIdx.y * blockDim.y + threadIdx.y;
    int idz = blockIdx.z * blockDim.z + threadIdx.z;
    
    if (idx >= params.chunk_size || idy >= params.chunk_size || idz >= params.chunk_size) {
        return;
    }
    
    float world_x = (params.chunk_x * params.chunk_size + idx) + 0.5f;
    float world_y = (params.chunk_y * params.chunk_size + idy) + 0.5f;
    float world_z = (params.chunk_z * params.chunk_size + idz) + 0.5f;
    
    int index = idy * params.chunk_size * params.chunk_size + idz * params.chunk_size + idx;
    
    // Only replace stone blocks
    if (blocks[index] != BLOCK_STONE) {
        return;
    }
    
    // Check depth range
    if (world_y < depth_min || world_y > depth_max) {
        return;
    }
    
    // Generate ore using noise
    float ore_noise = fbm(world_x * 0.1f, world_y * 0.1f, world_z * 0.1f, 2, 0.8f, 1.0f);
    
    if (ore_noise > (1.0f - ore_frequency)) {
        blocks[index] = ore_type;
    }
}

// Host wrapper functions
extern "C" {

void launchTerrainGeneration(
    uint16_t* d_blocks,
    uint8_t* d_metadata,
    float* d_heightmap,
    uint8_t* d_biome_map,
    const TerrainParams& params,
    cudaStream_t stream
) {
    dim3 blockSize(8, 8, 8);
    dim3 gridSize(
        (params.chunk_size + blockSize.x - 1) / blockSize.x,
        (params.chunk_size + blockSize.y - 1) / blockSize.y,
        (params.chunk_size + blockSize.z - 1) / blockSize.z
    );
    
    generateTerrainKernel<<<gridSize, blockSize, 0, stream>>>(
        d_blocks, d_metadata, d_heightmap, d_biome_map, params
    );
}

void launchOreGeneration(
    uint16_t* d_blocks,
    const TerrainParams& params,
    float ore_frequency,
    int ore_type,
    float depth_min,
    float depth_max,
    cudaStream_t stream
) {
    dim3 blockSize(8, 8, 8);
    dim3 gridSize(
        (params.chunk_size + blockSize.x - 1) / blockSize.x,
        (params.chunk_size + blockSize.y - 1) / blockSize.y,
        (params.chunk_size + blockSize.z - 1) / blockSize.z
    );
    
    generateOresKernel<<<gridSize, blockSize, 0, stream>>>(
        d_blocks, params, ore_frequency, ore_type, depth_min, depth_max
    );
}

} // extern "C"