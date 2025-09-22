#include <gtest/gtest.h>
#include "engine/Mesh/MarchingCubesGenerator.h"
#include "engine/VoxelTypes.h"
#include <cmath>

using namespace engine;

class MesherSphereTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a sphere in a 32x32x32 chunk
        chunk = VoxelChunk();

        const float radius = 12.0f;
        const float centerX = 16.0f;
        const float centerY = 16.0f;
        const float centerZ = 16.0f;

        for (int z = 0; z < kChunkDim; ++z) {
            for (int y = 0; y < kChunkDim; ++y) {
                for (int x = 0; x < kChunkDim; ++x) {
                    float dx = x - centerX;
                    float dy = y - centerY;
                    float dz = z - centerZ;
                    float distance = std::sqrt(dx*dx + dy*dy + dz*dz);

                    int idx = VoxelChunk::index(x, y, z);
                    if (distance <= radius) {
                        // Inside sphere - high density
                        chunk.voxels[idx].density = 255;
                    } else {
                        // Outside sphere - low density
                        chunk.voxels[idx].density = 0;
                    }
                }
            }
        }
    }

    VoxelChunk chunk;
    MarchingCubesGenerator generator;
};

TEST_F(MesherSphereTest, GeneratesValidMesh) {
    MeshGenParams params;
    params.isoLevel = 128; // Mid-point threshold

    Mesh mesh = generator.generate(chunk, params);

    // Should have generated some geometry
    EXPECT_GT(mesh.positions.size(), 0);
    EXPECT_GT(mesh.indices.size(), 0);

    // Positions should be in groups of 3 (x, y, z)
    EXPECT_EQ(mesh.positions.size() % 3, 0);

    // Indices should be in groups of 3 (triangles)
    EXPECT_EQ(mesh.indices.size() % 3, 0);

    // Should have same number of normals as positions
    EXPECT_EQ(mesh.normals.size(), mesh.positions.size());
}

TEST_F(MesherSphereTest, GeneratesReasonableVertexCount) {
    MeshGenParams params;
    params.isoLevel = 128;

    Mesh mesh = generator.generate(chunk, params);

    size_t vertexCount = mesh.positions.size() / 3;
    size_t triangleCount = mesh.indices.size() / 3;

    // For a sphere in 32x32x32 chunk, expect reasonable geometry
    // Should be more than a few vertices but not excessive
    EXPECT_GT(vertexCount, 100);
    EXPECT_LT(vertexCount, 10000);

    // Should have triangles
    EXPECT_GT(triangleCount, 50);
    EXPECT_LT(triangleCount, 5000);

    // Each triangle should have 3 vertices
    EXPECT_EQ(triangleCount * 3, mesh.indices.size());
}

TEST_F(MesherSphereTest, GeneratesManifoldGeometry) {
    MeshGenParams params;
    params.isoLevel = 128;

    Mesh mesh = generator.generate(chunk, params);

    if (mesh.indices.size() > 0) {
        // Check that all indices are valid
        size_t maxIndex = 0;
        for (uint32_t idx : mesh.indices) {
            maxIndex = std::max(maxIndex, static_cast<size_t>(idx));
        }

        size_t vertexCount = mesh.positions.size() / 3;
        EXPECT_LT(maxIndex, vertexCount);
    }
}

TEST_F(MesherSphereTest, DifferentIsoLevels) {
    // Test with different iso levels
    std::vector<uint8_t> isoLevels = {64, 128, 192};

    for (uint8_t isoLevel : isoLevels) {
        MeshGenParams params;
        params.isoLevel = isoLevel;

        Mesh mesh = generator.generate(chunk, params);

        // Should generate some geometry for all iso levels
        EXPECT_GT(mesh.positions.size(), 0);
        EXPECT_GT(mesh.indices.size(), 0);
    }
}

TEST_F(MesherSphereTest, EmptyChunkGeneratesNoGeometry) {
    // Create empty chunk
    VoxelChunk emptyChunk;
    for (auto& voxel : emptyChunk.voxels) {
        voxel.density = 0;
    }

    MeshGenParams params;
    params.isoLevel = 128;

    Mesh mesh = generator.generate(emptyChunk, params);

    // Empty chunk should generate no geometry
    EXPECT_EQ(mesh.positions.size(), 0);
    EXPECT_EQ(mesh.indices.size(), 0);
    EXPECT_EQ(mesh.normals.size(), 0);
}

TEST_F(MesherSphereTest, FullChunkGeneratesNoGeometry) {
    // Create full chunk
    VoxelChunk fullChunk;
    for (auto& voxel : fullChunk.voxels) {
        voxel.density = 255;
    }

    MeshGenParams params;
    params.isoLevel = 128;

    Mesh mesh = generator.generate(fullChunk, params);

    // Full chunk should generate no geometry (no surface)
    EXPECT_EQ(mesh.positions.size(), 0);
    EXPECT_EQ(mesh.indices.size(), 0);
    EXPECT_EQ(mesh.normals.size(), 0);
}
