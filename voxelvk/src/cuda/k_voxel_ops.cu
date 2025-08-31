// CUDA kernel for voxel operations (place/break blocks, neighbor updates)
#include <cuda_runtime.h>
#include <cstdint>

// Block operation types
#define OP_PLACE_BLOCK 1
#define OP_BREAK_BLOCK 2
#define OP_UPDATE_LIGHT 3
#define OP_UPDATE_NEIGHBORS 4

// Voxel operation structure
struct VoxelOp {
    int32_t x, y, z;           // World coordinates
    uint16_t old_block;        // Previous block type
    uint16_t new_block;        // New block type
    uint8_t old_metadata;      // Previous metadata
    uint8_t new_metadata;      // New metadata
    uint32_t operation_type;   // Type of operation
    uint32_t player_id;        // Player who made the change
    uint64_t timestamp;        // When the operation occurred
};

// Chunk update parameters
struct ChunkUpdateParams {
    int32_t chunk_size;
    int32_t num_chunks_x, num_chunks_y, num_chunks_z;
    int32_t world_min_x, world_min_y, world_min_z;
    int32_t world_max_x, world_max_y, world_max_z;
};

// Convert world coordinates to chunk coordinates
__device__ void worldToChunk(int32_t world_x, int32_t world_y, int32_t world_z,
                            int32_t chunk_size,
                            int32_t* chunk_x, int32_t* chunk_y, int32_t* chunk_z,
                            int32_t* local_x, int32_t* local_y, int32_t* local_z) {
    *chunk_x = world_x >= 0 ? world_x / chunk_size : (world_x - chunk_size + 1) / chunk_size;
    *chunk_y = world_y >= 0 ? world_y / chunk_size : (world_y - chunk_size + 1) / chunk_size;
    *chunk_z = world_z >= 0 ? world_z / chunk_size : (world_z - chunk_size + 1) / chunk_size;
    
    *local_x = world_x >= 0 ? world_x % chunk_size : chunk_size - 1 + ((world_x + 1) % chunk_size);
    *local_y = world_y >= 0 ? world_y % chunk_size : chunk_size - 1 + ((world_y + 1) % chunk_size);
    *local_z = world_z >= 0 ? world_z % chunk_size : chunk_size - 1 + ((world_z + 1) % chunk_size);
}

// Get chunk index from chunk coordinates
__device__ int32_t getChunkIndex(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z,
                                int32_t num_chunks_x, int32_t num_chunks_y, int32_t num_chunks_z) {
    if (chunk_x < 0 || chunk_x >= num_chunks_x ||
        chunk_y < 0 || chunk_y >= num_chunks_y ||
        chunk_z < 0 || chunk_z >= num_chunks_z) {
        return -1;
    }
    return chunk_y * num_chunks_x * num_chunks_z + chunk_z * num_chunks_x + chunk_x;
}

// Get block index within chunk
__device__ int32_t getBlockIndex(int32_t local_x, int32_t local_y, int32_t local_z, int32_t chunk_size) {
    return local_y * chunk_size * chunk_size + local_z * chunk_size + local_x;
}

// Check if coordinates are valid
__device__ bool isValidCoordinate(int32_t x, int32_t y, int32_t z, const ChunkUpdateParams& params) {
    return x >= params.world_min_x && x <= params.world_max_x &&
           y >= params.world_min_y && y <= params.world_max_y &&
           z >= params.world_min_z && z <= params.world_max_z;
}

// CUDA kernel for applying voxel operations
__global__ void applyVoxelOperationsKernel(
    VoxelOp* operations,        // Array of operations to apply
    int32_t num_operations,     // Number of operations
    uint16_t** chunk_blocks,    // Array of chunk block data pointers
    uint8_t** chunk_metadata,   // Array of chunk metadata pointers
    uint8_t* chunk_dirty_flags, // Dirty flags for each chunk
    ChunkUpdateParams params
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (idx >= num_operations) {
        return;
    }
    
    VoxelOp op = operations[idx];
    
    // Validate coordinates
    if (!isValidCoordinate(op.x, op.y, op.z, params)) {
        return;
    }
    
    // Convert to chunk coordinates
    int32_t chunk_x, chunk_y, chunk_z;
    int32_t local_x, local_y, local_z;
    worldToChunk(op.x, op.y, op.z, params.chunk_size,
                &chunk_x, &chunk_y, &chunk_z,
                &local_x, &local_y, &local_z);
    
    // Get chunk index
    int32_t chunk_index = getChunkIndex(chunk_x, chunk_y, chunk_z,
                                       params.num_chunks_x, params.num_chunks_y, params.num_chunks_z);
    if (chunk_index < 0 || chunk_blocks[chunk_index] == nullptr) {
        return;
    }
    
    // Get block index within chunk
    int32_t block_index = getBlockIndex(local_x, local_y, local_z, params.chunk_size);
    
    // Apply the operation
    switch (op.operation_type) {
        case OP_PLACE_BLOCK:
        case OP_BREAK_BLOCK:
            chunk_blocks[chunk_index][block_index] = op.new_block;
            chunk_metadata[chunk_index][block_index] = op.new_metadata;
            chunk_dirty_flags[chunk_index] = 1;
            break;
    }
}

// CUDA kernel for batch block updates
__global__ void batchBlockUpdateKernel(
    int32_t* positions,         // [3 * num_updates] - x, y, z coordinates
    uint16_t* block_types,      // [num_updates] - new block types
    uint8_t* metadata_values,   // [num_updates] - new metadata values
    int32_t num_updates,
    uint16_t** chunk_blocks,
    uint8_t** chunk_metadata,
    uint8_t* chunk_dirty_flags,
    ChunkUpdateParams params
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (idx >= num_updates) {
        return;
    }
    
    int32_t world_x = positions[idx * 3 + 0];
    int32_t world_y = positions[idx * 3 + 1];
    int32_t world_z = positions[idx * 3 + 2];
    
    if (!isValidCoordinate(world_x, world_y, world_z, params)) {
        return;
    }
    
    // Convert to chunk coordinates
    int32_t chunk_x, chunk_y, chunk_z;
    int32_t local_x, local_y, local_z;
    worldToChunk(world_x, world_y, world_z, params.chunk_size,
                &chunk_x, &chunk_y, &chunk_z,
                &local_x, &local_y, &local_z);
    
    int32_t chunk_index = getChunkIndex(chunk_x, chunk_y, chunk_z,
                                       params.num_chunks_x, params.num_chunks_y, params.num_chunks_z);
    if (chunk_index < 0 || chunk_blocks[chunk_index] == nullptr) {
        return;
    }
    
    int32_t block_index = getBlockIndex(local_x, local_y, local_z, params.chunk_size);
    
    // Update block
    chunk_blocks[chunk_index][block_index] = block_types[idx];
    chunk_metadata[chunk_index][block_index] = metadata_values[idx];
    chunk_dirty_flags[chunk_index] = 1;
}

// CUDA kernel for neighbor block updates (used for physics/lighting)
__global__ void updateNeighborBlocksKernel(
    int32_t* changed_positions, // [3 * num_changed] - positions that changed
    int32_t num_changed,
    uint16_t** chunk_blocks,
    uint8_t** chunk_metadata,
    uint8_t* update_flags,      // Output: which blocks need updates
    ChunkUpdateParams params
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (idx >= num_changed) {
        return;
    }
    
    int32_t center_x = changed_positions[idx * 3 + 0];
    int32_t center_y = changed_positions[idx * 3 + 1];
    int32_t center_z = changed_positions[idx * 3 + 2];
    
    // Check all 6 neighbors
    int32_t offsets[6][3] = {
        {-1, 0, 0}, {1, 0, 0},   // Left, Right
        {0, -1, 0}, {0, 1, 0},   // Down, Up
        {0, 0, -1}, {0, 0, 1}    // Back, Front
    };
    
    for (int i = 0; i < 6; i++) {
        int32_t neighbor_x = center_x + offsets[i][0];
        int32_t neighbor_y = center_y + offsets[i][1];
        int32_t neighbor_z = center_z + offsets[i][2];
        
        if (!isValidCoordinate(neighbor_x, neighbor_y, neighbor_z, params)) {
            continue;
        }
        
        // Convert to chunk coordinates
        int32_t chunk_x, chunk_y, chunk_z;
        int32_t local_x, local_y, local_z;
        worldToChunk(neighbor_x, neighbor_y, neighbor_z, params.chunk_size,
                    &chunk_x, &chunk_y, &chunk_z,
                    &local_x, &local_y, &local_z);
        
        int32_t chunk_index = getChunkIndex(chunk_x, chunk_y, chunk_z,
                                           params.num_chunks_x, params.num_chunks_y, params.num_chunks_z);
        if (chunk_index < 0) {
            continue;
        }
        
        int32_t block_index = getBlockIndex(local_x, local_y, local_z, params.chunk_size);
        
        // Mark for update (this could trigger lighting updates, physics, etc.)
        int32_t global_index = chunk_index * (params.chunk_size * params.chunk_size * params.chunk_size) + block_index;
        update_flags[global_index] = 1;
    }
}

// CUDA kernel for flood fill operations (for connected component analysis)
__global__ void floodFillKernel(
    uint16_t** chunk_blocks,
    uint8_t* visited,           // Global visited array
    int32_t* queue,             // BFS queue
    int32_t* queue_size,        // Current queue size
    int32_t max_queue_size,
    uint16_t target_block_type,
    uint16_t replacement_block_type,
    ChunkUpdateParams params
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (idx >= *queue_size) {
        return;
    }
    
    // Get position from queue
    int32_t world_x = queue[idx * 3 + 0];
    int32_t world_y = queue[idx * 3 + 1];
    int32_t world_z = queue[idx * 3 + 2];
    
    // Convert to chunk coordinates
    int32_t chunk_x, chunk_y, chunk_z;
    int32_t local_x, local_y, local_z;
    worldToChunk(world_x, world_y, world_z, params.chunk_size,
                &chunk_x, &chunk_y, &chunk_z,
                &local_x, &local_y, &local_z);
    
    int32_t chunk_index = getChunkIndex(chunk_x, chunk_y, chunk_z,
                                       params.num_chunks_x, params.num_chunks_y, params.num_chunks_z);
    if (chunk_index < 0 || chunk_blocks[chunk_index] == nullptr) {
        return;
    }
    
    int32_t block_index = getBlockIndex(local_x, local_y, local_z, params.chunk_size);
    
    // Check if this block matches target type
    if (chunk_blocks[chunk_index][block_index] != target_block_type) {
        return;
    }
    
    // Replace the block
    chunk_blocks[chunk_index][block_index] = replacement_block_type;
    
    // Add neighbors to queue (this would need atomic operations for thread safety)
    // Implementation simplified for brevity
}

// Host wrapper functions
extern "C" {

void launchVoxelOperations(
    VoxelOp* d_operations,
    int32_t num_operations,
    uint16_t** d_chunk_blocks,
    uint8_t** d_chunk_metadata,
    uint8_t* d_chunk_dirty_flags,
    const ChunkUpdateParams& params,
    cudaStream_t stream
) {
    if (num_operations == 0) return;
    
    int blockSize = 256;
    int gridSize = (num_operations + blockSize - 1) / blockSize;
    
    applyVoxelOperationsKernel<<<gridSize, blockSize, 0, stream>>>(
        d_operations, num_operations,
        d_chunk_blocks, d_chunk_metadata, d_chunk_dirty_flags,
        params
    );
}

void launchBatchBlockUpdate(
    int32_t* d_positions,
    uint16_t* d_block_types,
    uint8_t* d_metadata_values,
    int32_t num_updates,
    uint16_t** d_chunk_blocks,
    uint8_t** d_chunk_metadata,
    uint8_t* d_chunk_dirty_flags,
    const ChunkUpdateParams& params,
    cudaStream_t stream
) {
    if (num_updates == 0) return;
    
    int blockSize = 256;
    int gridSize = (num_updates + blockSize - 1) / blockSize;
    
    batchBlockUpdateKernel<<<gridSize, blockSize, 0, stream>>>(
        d_positions, d_block_types, d_metadata_values, num_updates,
        d_chunk_blocks, d_chunk_metadata, d_chunk_dirty_flags,
        params
    );
}

void launchNeighborUpdate(
    int32_t* d_changed_positions,
    int32_t num_changed,
    uint16_t** d_chunk_blocks,
    uint8_t** d_chunk_metadata,
    uint8_t* d_update_flags,
    const ChunkUpdateParams& params,
    cudaStream_t stream
) {
    if (num_changed == 0) return;
    
    int blockSize = 256;
    int gridSize = (num_changed + blockSize - 1) / blockSize;
    
    updateNeighborBlocksKernel<<<gridSize, blockSize, 0, stream>>>(
        d_changed_positions, num_changed,
        d_chunk_blocks, d_chunk_metadata, d_update_flags,
        params
    );
}

} // extern "C"