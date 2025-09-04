/**
 * Voxel Mathematics Unit Tests
 * ============================
 * 
 * Tests for core voxel mathematics, coordinate systems, and spatial algorithms.
 * These tests validate the fundamental math that drives the voxel engine.
 */

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <vector>

// Mock voxel math functions (these would normally be in your engine)
namespace VoxelMath {
    
    struct ChunkCoord {
        int x, y, z;
        
        bool operator==(const ChunkCoord& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
    };
    
    struct VoxelCoord {
        int x, y, z;
        
        bool operator==(const VoxelCoord& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
    };
    
    constexpr int CHUNK_SIZE = 64;
    constexpr int WORLD_HEIGHT = 256;
    
    // Convert world position to chunk coordinates
    ChunkCoord worldToChunk(const glm::vec3& worldPos) {
        return {
            static_cast<int>(std::floor(worldPos.x / CHUNK_SIZE)),
            static_cast<int>(std::floor(worldPos.y / CHUNK_SIZE)), 
            static_cast<int>(std::floor(worldPos.z / CHUNK_SIZE))
        };
    }
    
    // Convert world position to voxel coordinates within a chunk
    VoxelCoord worldToVoxel(const glm::vec3& worldPos) {
        int vx = static_cast<int>(std::floor(worldPos.x)) % CHUNK_SIZE;
        int vy = static_cast<int>(std::floor(worldPos.y)) % CHUNK_SIZE;
        int vz = static_cast<int>(std::floor(worldPos.z)) % CHUNK_SIZE;
        
        // Handle negative coordinates
        if (vx < 0) vx += CHUNK_SIZE;
        if (vy < 0) vy += CHUNK_SIZE;
        if (vz < 0) vz += CHUNK_SIZE;
        
        return {vx, vy, vz};
    }
    
    // Convert chunk + voxel coordinates to world position
    glm::vec3 chunkVoxelToWorld(const ChunkCoord& chunk, const VoxelCoord& voxel) {
        return glm::vec3(
            chunk.x * CHUNK_SIZE + voxel.x,
            chunk.y * CHUNK_SIZE + voxel.y,
            chunk.z * CHUNK_SIZE + voxel.z
        );
    }
    
    // Calculate Manhattan distance between two voxel positions
    int manhattanDistance(const VoxelCoord& a, const VoxelCoord& b) {
        return std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z);
    }
    
    // Check if a voxel coordinate is within chunk bounds
    bool isValidVoxelCoord(const VoxelCoord& voxel) {
        return voxel.x >= 0 && voxel.x < CHUNK_SIZE &&
               voxel.y >= 0 && voxel.y < CHUNK_SIZE &&
               voxel.z >= 0 && voxel.z < CHUNK_SIZE;
    }
    
    // Get neighboring voxel coordinates (6-connected)
    std::vector<VoxelCoord> getNeighbors(const VoxelCoord& voxel) {
        std::vector<VoxelCoord> neighbors;
        
        const std::array<VoxelCoord, 6> offsets = {{
            {1, 0, 0}, {-1, 0, 0},  // X axis
            {0, 1, 0}, {0, -1, 0},  // Y axis
            {0, 0, 1}, {0, 0, -1}   // Z axis
        }};
        
        for (const auto& offset : offsets) {
            VoxelCoord neighbor = {
                voxel.x + offset.x,
                voxel.y + offset.y,
                voxel.z + offset.z
            };
            
            if (isValidVoxelCoord(neighbor)) {
                neighbors.push_back(neighbor);
            }
        }
        
        return neighbors;
    }
}

class VoxelMathTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test data setup
    }
};

TEST_F(VoxelMathTest, ChunkSize) {
    EXPECT_EQ(VoxelMath::CHUNK_SIZE, 64);
    EXPECT_GT(VoxelMath::WORLD_HEIGHT, 0);
}

TEST_F(VoxelMathTest, WorldToChunkConversion) {
    using namespace VoxelMath;
    
    // Test origin
    auto chunk = worldToChunk(glm::vec3(0, 0, 0));
    EXPECT_EQ(chunk, ChunkCoord({0, 0, 0}));
    
    // Test positive coordinates
    chunk = worldToChunk(glm::vec3(64, 128, 192));
    EXPECT_EQ(chunk, ChunkCoord({1, 2, 3}));
    
    // Test negative coordinates
    chunk = worldToChunk(glm::vec3(-64, -128, -1));
    EXPECT_EQ(chunk, ChunkCoord({-1, -2, -1}));
    
    // Test boundary conditions
    chunk = worldToChunk(glm::vec3(63.9f, 63.9f, 63.9f));
    EXPECT_EQ(chunk, ChunkCoord({0, 0, 0}));
    
    chunk = worldToChunk(glm::vec3(64.1f, 64.1f, 64.1f));
    EXPECT_EQ(chunk, ChunkCoord({1, 1, 1}));
}

TEST_F(VoxelMathTest, WorldToVoxelConversion) {
    using namespace VoxelMath;
    
    // Test origin
    auto voxel = worldToVoxel(glm::vec3(0, 0, 0));
    EXPECT_EQ(voxel, VoxelCoord({0, 0, 0}));
    
    // Test within first chunk
    voxel = worldToVoxel(glm::vec3(32, 16, 48));
    EXPECT_EQ(voxel, VoxelCoord({32, 16, 48}));
    
    // Test boundary of chunk
    voxel = worldToVoxel(glm::vec3(63, 63, 63));
    EXPECT_EQ(voxel, VoxelCoord({63, 63, 63}));
    
    // Test next chunk boundary
    voxel = worldToVoxel(glm::vec3(64, 64, 64));
    EXPECT_EQ(voxel, VoxelCoord({0, 0, 0}));
    
    // Test negative coordinates
    voxel = worldToVoxel(glm::vec3(-1, -1, -1));
    EXPECT_EQ(voxel, VoxelCoord({63, 63, 63}));
}

TEST_F(VoxelMathTest, RoundTripConversion) {
    using namespace VoxelMath;
    
    // Test round-trip conversion maintains consistency
    std::vector<glm::vec3> testPositions = {
        {0, 0, 0},
        {32, 64, 96},
        {-32, -64, -96},
        {127, 255, 191}
    };
    
    for (const auto& originalPos : testPositions) {
        auto chunk = worldToChunk(originalPos);
        auto voxel = worldToVoxel(originalPos);
        auto reconstructed = chunkVoxelToWorld(chunk, voxel);
        
        // Should be within 1 unit (due to floor operations)
        EXPECT_NEAR(reconstructed.x, std::floor(originalPos.x), 1.0f);
        EXPECT_NEAR(reconstructed.y, std::floor(originalPos.y), 1.0f);  
        EXPECT_NEAR(reconstructed.z, std::floor(originalPos.z), 1.0f);
    }
}

TEST_F(VoxelMathTest, ManhattanDistance) {
    using namespace VoxelMath;
    
    VoxelCoord origin = {0, 0, 0};
    
    // Distance to self
    EXPECT_EQ(manhattanDistance(origin, origin), 0);
    
    // Distance along single axis
    EXPECT_EQ(manhattanDistance(origin, {5, 0, 0}), 5);
    EXPECT_EQ(manhattanDistance(origin, {0, 3, 0}), 3);
    EXPECT_EQ(manhattanDistance(origin, {0, 0, 7}), 7);
    
    // Distance along multiple axes
    EXPECT_EQ(manhattanDistance(origin, {3, 4, 0}), 7);
    EXPECT_EQ(manhattanDistance(origin, {1, 1, 1}), 3);
    
    // Negative coordinates
    EXPECT_EQ(manhattanDistance({5, 5, 5}, {2, 3, 1}), 9);
}

TEST_F(VoxelMathTest, VoxelCoordValidation) {
    using namespace VoxelMath;
    
    // Valid coordinates
    EXPECT_TRUE(isValidVoxelCoord({0, 0, 0}));
    EXPECT_TRUE(isValidVoxelCoord({32, 32, 32}));
    EXPECT_TRUE(isValidVoxelCoord({63, 63, 63}));
    
    // Invalid coordinates (out of bounds)
    EXPECT_FALSE(isValidVoxelCoord({-1, 0, 0}));
    EXPECT_FALSE(isValidVoxelCoord({64, 0, 0}));
    EXPECT_FALSE(isValidVoxelCoord({0, -1, 0}));
    EXPECT_FALSE(isValidVoxelCoord({0, 64, 0}));
    EXPECT_FALSE(isValidVoxelCoord({0, 0, -1}));
    EXPECT_FALSE(isValidVoxelCoord({0, 0, 64}));
}

TEST_F(VoxelMathTest, NeighborCalculation) {
    using namespace VoxelMath;
    
    // Center voxel should have 6 neighbors
    VoxelCoord center = {32, 32, 32};
    auto neighbors = getNeighbors(center);
    EXPECT_EQ(neighbors.size(), 6);
    
    // Verify all neighbors are valid and adjacent
    for (const auto& neighbor : neighbors) {
        EXPECT_TRUE(isValidVoxelCoord(neighbor));
        EXPECT_EQ(manhattanDistance(center, neighbor), 1);
    }
    
    // Corner voxel should have fewer neighbors
    VoxelCoord corner = {0, 0, 0};
    auto cornerNeighbors = getNeighbors(corner);
    EXPECT_EQ(cornerNeighbors.size(), 3);  // Only positive directions
    
    // Edge voxel
    VoxelCoord edge = {0, 32, 32};
    auto edgeNeighbors = getNeighbors(edge);
    EXPECT_EQ(edgeNeighbors.size(), 5);  // Missing -X direction
}

TEST_F(VoxelMathTest, ChunkBoundaryHandling) {
    using namespace VoxelMath;
    
    // Test coordinates right at chunk boundaries
    std::vector<float> testCoords = {-0.1f, -0.01f, 0.0f, 0.01f, 63.99f, 64.0f, 64.01f};
    
    for (float coord : testCoords) {
        glm::vec3 pos(coord, 0, 0);
        auto chunk = worldToChunk(pos);
        auto voxel = worldToVoxel(pos);
        
        // Verify consistency
        EXPECT_TRUE(isValidVoxelCoord(voxel));
        EXPECT_GE(voxel.x, 0);
        EXPECT_LT(voxel.x, CHUNK_SIZE);
    }
}

// Performance test for coordinate conversions
TEST_F(VoxelMathTest, ConversionPerformance) {
    using namespace VoxelMath;
    
    const int NUM_TESTS = 10000;
    std::vector<glm::vec3> testPositions;
    
    // Generate test data
    for (int i = 0; i < NUM_TESTS; i++) {
        testPositions.emplace_back(
            static_cast<float>(i % 1000 - 500),
            static_cast<float>(i % 256),
            static_cast<float>(i % 1000 - 500)
        );
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform conversions
    for (const auto& pos : testPositions) {
        auto chunk = worldToChunk(pos);
        auto voxel = worldToVoxel(pos);
        auto reconstructed = chunkVoxelToWorld(chunk, voxel);
        
        // Prevent optimization
        volatile float dummy = reconstructed.x + reconstructed.y + reconstructed.z;
        (void)dummy;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Converted " << NUM_TESTS << " coordinates in " 
              << duration.count() << " microseconds" << std::endl;
    
    // Performance expectation: should handle thousands of conversions per millisecond
    EXPECT_LT(duration.count(), 10000);  // Less than 10ms for 10k conversions
}