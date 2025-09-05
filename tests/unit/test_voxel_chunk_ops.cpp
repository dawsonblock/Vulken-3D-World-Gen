#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <cstdint>

// Mock voxel chunk operations - in real implementation these would be from src/voxel/chunk_ops.hpp
namespace voxelvk {
    using VoxelType = uint16_t;
    constexpr int CHUNK_SIZE = 32;
    constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
    
    struct Chunk {
        std::array<VoxelType, CHUNK_VOLUME> voxels;
        
        Chunk() { voxels.fill(0); }
        
        VoxelType getVoxel(int x, int y, int z) const {
            if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
                return 0;
            }
            return voxels[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE];
        }
        
        void setVoxel(int x, int y, int z, VoxelType value) {
            if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
                return;
            }
            voxels[x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE] = value;
        }
        
        bool isEmpty() const {
            for (const auto& voxel : voxels) {
                if (voxel != 0) return false;
            }
            return true;
        }
        
        int countNonEmpty() const {
            int count = 0;
            for (const auto& voxel : voxels) {
                if (voxel != 0) count++;
            }
            return count;
        }
        
        void fill(VoxelType value) {
            voxels.fill(value);
        }
        
        void fillRegion(int x0, int y0, int z0, int x1, int y1, int z1, VoxelType value) {
            for (int z = z0; z <= z1; ++z) {
                for (int y = y0; y <= y1; ++y) {
                    for (int x = x0; x <= x1; ++x) {
                        setVoxel(x, y, z, value);
                    }
                }
            }
        }
        
        std::vector<uint8_t> compress() const {
            std::vector<uint8_t> compressed;
            VoxelType currentValue = voxels[0];
            int runLength = 1;
            
            for (size_t i = 1; i < voxels.size(); ++i) {
                if (voxels[i] == currentValue && runLength < 255) {
                    runLength++;
                } else {
                    // Write run-length encoded data
                    compressed.push_back(static_cast<uint8_t>(runLength));
                    compressed.push_back(static_cast<uint8_t>(currentValue & 0xFF));
                    compressed.push_back(static_cast<uint8_t>((currentValue >> 8) & 0xFF));
                    
                    currentValue = voxels[i];
                    runLength = 1;
                }
            }
            
            // Write final run
            compressed.push_back(static_cast<uint8_t>(runLength));
            compressed.push_back(static_cast<uint8_t>(currentValue & 0xFF));
            compressed.push_back(static_cast<uint8_t>((currentValue >> 8) & 0xFF));
            
            return compressed;
        }
        
        bool decompress(const std::vector<uint8_t>& compressed) {
            if (compressed.size() % 3 != 0) return false;
            
            size_t voxelIndex = 0;
            for (size_t i = 0; i < compressed.size(); i += 3) {
                uint8_t runLength = compressed[i];
                VoxelType value = compressed[i + 1] | (static_cast<VoxelType>(compressed[i + 2]) << 8);
                
                for (int j = 0; j < runLength && voxelIndex < voxels.size(); ++j) {
                    voxels[voxelIndex++] = value;
                }
            }
            
            return voxelIndex == voxels.size();
        }
    };
}

class VoxelChunkOpsTest : public ::testing::Test {
protected:
    voxelvk::Chunk chunk;
};

TEST_F(VoxelChunkOpsTest, InitialState) {
    EXPECT_TRUE(chunk.isEmpty());
    EXPECT_EQ(0, chunk.countNonEmpty());
    EXPECT_EQ(0, chunk.getVoxel(0, 0, 0));
    EXPECT_EQ(0, chunk.getVoxel(15, 15, 15));
}

TEST_F(VoxelChunkOpsTest, SetAndGetVoxel) {
    chunk.setVoxel(10, 15, 20, 42);
    EXPECT_EQ(42, chunk.getVoxel(10, 15, 20));
    EXPECT_FALSE(chunk.isEmpty());
    EXPECT_EQ(1, chunk.countNonEmpty());
    
    // Test bounds checking
    EXPECT_EQ(0, chunk.getVoxel(-1, 0, 0));
    EXPECT_EQ(0, chunk.getVoxel(0, -1, 0));
    EXPECT_EQ(0, chunk.getVoxel(0, 0, -1));
    EXPECT_EQ(0, chunk.getVoxel(voxelvk::CHUNK_SIZE, 0, 0));
    EXPECT_EQ(0, chunk.getVoxel(0, voxelvk::CHUNK_SIZE, 0));
    EXPECT_EQ(0, chunk.getVoxel(0, 0, voxelvk::CHUNK_SIZE));
}

TEST_F(VoxelChunkOpsTest, FillChunk) {
    chunk.fill(255);
    EXPECT_FALSE(chunk.isEmpty());
    EXPECT_EQ(voxelvk::CHUNK_VOLUME, chunk.countNonEmpty());
    EXPECT_EQ(255, chunk.getVoxel(0, 0, 0));
    EXPECT_EQ(255, chunk.getVoxel(31, 31, 31));
}

TEST_F(VoxelChunkOpsTest, FillRegion) {
    chunk.fillRegion(5, 5, 5, 10, 10, 10, 100);
    
    // Check inside region
    EXPECT_EQ(100, chunk.getVoxel(5, 5, 5));
    EXPECT_EQ(100, chunk.getVoxel(10, 10, 10));
    EXPECT_EQ(100, chunk.getVoxel(7, 8, 9));
    
    // Check outside region
    EXPECT_EQ(0, chunk.getVoxel(4, 5, 5));
    EXPECT_EQ(0, chunk.getVoxel(11, 10, 10));
    EXPECT_EQ(0, chunk.getVoxel(0, 0, 0));
    
    // Calculate expected count: 6x6x6 = 216 voxels
    EXPECT_EQ(216, chunk.countNonEmpty());
}

TEST_F(VoxelChunkOpsTest, CompressionRoundtrip) {
    // Create a pattern: alternating blocks
    for (int z = 0; z < voxelvk::CHUNK_SIZE; ++z) {
        for (int y = 0; y < voxelvk::CHUNK_SIZE; ++y) {
            for (int x = 0; x < voxelvk::CHUNK_SIZE; ++x) {
                voxelvk::VoxelType value = ((x + y + z) % 2 == 0) ? 1 : 2;
                chunk.setVoxel(x, y, z, value);
            }
        }
    }
    
    // Compress
    auto compressed = chunk.compress();
    EXPECT_GT(compressed.size(), 0);
    
    // Decompress into new chunk
    voxelvk::Chunk decompressedChunk;
    ASSERT_TRUE(decompressedChunk.decompress(compressed));
    
    // Verify all voxels match
    for (int z = 0; z < voxelvk::CHUNK_SIZE; ++z) {
        for (int y = 0; y < voxelvk::CHUNK_SIZE; ++y) {
            for (int x = 0; x < voxelvk::CHUNK_SIZE; ++x) {
                EXPECT_EQ(chunk.getVoxel(x, y, z), decompressedChunk.getVoxel(x, y, z))
                    << "Mismatch at (" << x << ", " << y << ", " << z << ")";
            }
        }
    }
}

TEST_F(VoxelChunkOpsTest, EmptyChunkCompression) {
    auto compressed = chunk.compress();
    EXPECT_EQ(3, compressed.size()); // Should be one run of zeros
    
    voxelvk::Chunk decompressedChunk;
    ASSERT_TRUE(decompressedChunk.decompress(compressed));
    EXPECT_TRUE(decompressedChunk.isEmpty());
}

TEST_F(VoxelChunkOpsTest, FullChunkCompression) {
    chunk.fill(42);
    auto compressed = chunk.compress();
    EXPECT_EQ(3, compressed.size()); // Should be one run of 42s
    
    voxelvk::Chunk decompressedChunk;
    ASSERT_TRUE(decompressedChunk.decompress(compressed));
    EXPECT_EQ(voxelvk::CHUNK_VOLUME, decompressedChunk.countNonEmpty());
    EXPECT_EQ(42, decompressedChunk.getVoxel(0, 0, 0));
    EXPECT_EQ(42, decompressedChunk.getVoxel(31, 31, 31));
}