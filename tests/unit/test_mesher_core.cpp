/**
 * Voxel Mesher Core Unit Tests
 * =============================
 * 
 * Tests for the core meshing algorithms that convert voxel data to renderable geometry.
 * Validates both basic meshing and greedy meshing optimizations.
 */

#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <unordered_map>
#include <algorithm>

// Mock meshing system (these would normally be in your engine)
namespace VoxelMesher {
    
    struct Vertex {
        float x, y, z;      // Position
        float nx, ny, nz;   // Normal
        float u, v;         // UV coordinates
        
        bool operator==(const Vertex& other) const {
            const float epsilon = 0.001f;
            return std::abs(x - other.x) < epsilon &&
                   std::abs(y - other.y) < epsilon &&
                   std::abs(z - other.z) < epsilon &&
                   std::abs(nx - other.nx) < epsilon &&
                   std::abs(ny - other.ny) < epsilon &&
                   std::abs(nz - other.nz) < epsilon;
        }
    };
    
    struct Face {
        std::array<Vertex, 4> vertices;  // Quad vertices
        uint8_t materialId;
        uint8_t faceDirection;  // 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    };
    
    struct GreedyQuad {
        int x, y, z;        // Position
        int width, height;  // Dimensions
        uint8_t materialId;
        uint8_t faceDirection;
    };
    
    struct MeshData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        size_t faceCount = 0;
    };
    
    constexpr int CHUNK_SIZE = 16;  // Smaller for testing
    using VoxelData = std::array<std::array<std::array<uint8_t, CHUNK_SIZE>, CHUNK_SIZE>, CHUNK_SIZE>;
    
    // Face normals for cube faces
    const std::array<std::array<float, 3>, 6> FACE_NORMALS = {{
        {{1, 0, 0}}, {{-1, 0, 0}},   // +X, -X
        {{0, 1, 0}}, {{0, -1, 0}},   // +Y, -Y  
        {{0, 0, 1}}, {{0, 0, -1}}    // +Z, -Z
    }};
    
    // Naive meshing algorithm - creates a face for every exposed voxel face
    class NaiveMesher {
    public:
        MeshData generateMesh(const VoxelData& voxels) {
            MeshData mesh;
            
            for (int x = 0; x < CHUNK_SIZE; x++) {
                for (int y = 0; y < CHUNK_SIZE; y++) {
                    for (int z = 0; z < CHUNK_SIZE; z++) {
                        uint8_t voxelType = voxels[x][y][z];
                        if (voxelType == 0) continue;  // Empty voxel
                        
                        // Check each face direction
                        for (int face = 0; face < 6; face++) {
                            if (shouldCreateFace(voxels, x, y, z, face)) {
                                createQuad(mesh, x, y, z, face, voxelType);
                            }
                        }
                    }
                }
            }
            
            return mesh;
        }
        
    private:
        bool shouldCreateFace(const VoxelData& voxels, int x, int y, int z, int faceDir) {
            // Calculate neighbor position
            int nx = x, ny = y, nz = z;
            switch (faceDir) {
                case 0: nx = x + 1; break;  // +X
                case 1: nx = x - 1; break;  // -X
                case 2: ny = y + 1; break;  // +Y
                case 3: ny = y - 1; break;  // -Y
                case 4: nz = z + 1; break;  // +Z
                case 5: nz = z - 1; break;  // -Z
            }
            
            // Check if neighbor is empty or out of bounds
            if (nx < 0 || nx >= CHUNK_SIZE || 
                ny < 0 || ny >= CHUNK_SIZE || 
                nz < 0 || nz >= CHUNK_SIZE) {
                return true;  // Exposed to air
            }
            
            return voxels[nx][ny][nz] == 0;  // Neighbor is empty
        }
        
        void createQuad(MeshData& mesh, int x, int y, int z, int faceDir, uint8_t material) {
            uint32_t baseIndex = static_cast<uint32_t>(mesh.vertices.size());
            
            // Create 4 vertices for the quad
            std::array<Vertex, 4> quadVerts = createQuadVertices(x, y, z, faceDir);
            
            // Add vertices to mesh
            for (const auto& vertex : quadVerts) {
                mesh.vertices.push_back(vertex);
            }
            
            // Add indices for two triangles (0,1,2) and (2,3,0)
            mesh.indices.insert(mesh.indices.end(), {
                baseIndex + 0, baseIndex + 1, baseIndex + 2,
                baseIndex + 2, baseIndex + 3, baseIndex + 0
            });
            
            mesh.faceCount++;
        }
        
        std::array<Vertex, 4> createQuadVertices(int x, int y, int z, int faceDir) {
            std::array<Vertex, 4> vertices;
            float fx = static_cast<float>(x);
            float fy = static_cast<float>(y);
            float fz = static_cast<float>(z);
            
            const auto& normal = FACE_NORMALS[faceDir];
            
            // Define quad vertices based on face direction
            switch (faceDir) {
                case 0: // +X face
                    vertices = {{
                        {fx+1, fy+0, fz+0, normal[0], normal[1], normal[2], 0, 0},
                        {fx+1, fy+1, fz+0, normal[0], normal[1], normal[2], 1, 0},
                        {fx+1, fy+1, fz+1, normal[0], normal[1], normal[2], 1, 1},
                        {fx+1, fy+0, fz+1, normal[0], normal[1], normal[2], 0, 1}
                    }};
                    break;
                case 1: // -X face
                    vertices = {{
                        {fx+0, fy+0, fz+1, normal[0], normal[1], normal[2], 0, 0},
                        {fx+0, fy+1, fz+1, normal[0], normal[1], normal[2], 1, 0},
                        {fx+0, fy+1, fz+0, normal[0], normal[1], normal[2], 1, 1},
                        {fx+0, fy+0, fz+0, normal[0], normal[1], normal[2], 0, 1}
                    }};
                    break;
                case 2: // +Y face
                    vertices = {{
                        {fx+0, fy+1, fz+0, normal[0], normal[1], normal[2], 0, 0},
                        {fx+1, fy+1, fz+0, normal[0], normal[1], normal[2], 1, 0},
                        {fx+1, fy+1, fz+1, normal[0], normal[1], normal[2], 1, 1},
                        {fx+0, fy+1, fz+1, normal[0], normal[1], normal[2], 0, 1}
                    }};
                    break;
                case 3: // -Y face
                    vertices = {{
                        {fx+0, fy+0, fz+1, normal[0], normal[1], normal[2], 0, 0},
                        {fx+1, fy+0, fz+1, normal[0], normal[1], normal[2], 1, 0},
                        {fx+1, fy+0, fz+0, normal[0], normal[1], normal[2], 1, 1},
                        {fx+0, fy+0, fz+0, normal[0], normal[1], normal[2], 0, 1}
                    }};
                    break;
                case 4: // +Z face
                    vertices = {{
                        {fx+0, fy+0, fz+1, normal[0], normal[1], normal[2], 0, 0},
                        {fx+0, fy+1, fz+1, normal[0], normal[1], normal[2], 1, 0},
                        {fx+1, fy+1, fz+1, normal[0], normal[1], normal[2], 1, 1},
                        {fx+1, fy+0, fz+1, normal[0], normal[1], normal[2], 0, 1}
                    }};
                    break;
                case 5: // -Z face
                    vertices = {{
                        {fx+1, fy+0, fz+0, normal[0], normal[1], normal[2], 0, 0},
                        {fx+1, fy+1, fz+0, normal[0], normal[1], normal[2], 1, 0},
                        {fx+0, fy+1, fz+0, normal[0], normal[1], normal[2], 1, 1},
                        {fx+0, fy+0, fz+0, normal[0], normal[1], normal[2], 0, 1}
                    }};
                    break;
            }
            
            return vertices;
        }
    };
    
    // Greedy meshing algorithm - combines adjacent faces into larger quads
    class GreedyMesher {
    public:
        MeshData generateMesh(const VoxelData& voxels) {
            MeshData mesh;
            
            // Process each axis (X, Y, Z) separately
            for (int axis = 0; axis < 3; axis++) {
                processAxis(voxels, mesh, axis);
            }
            
            return mesh;
        }
        
        std::vector<GreedyQuad> generateQuads(const VoxelData& voxels) {
            std::vector<GreedyQuad> quads;
            
            for (int axis = 0; axis < 3; axis++) {
                auto axisQuads = processAxisForQuads(voxels, axis);
                quads.insert(quads.end(), axisQuads.begin(), axisQuads.end());
            }
            
            return quads;
        }
        
    private:
        void processAxis(const VoxelData& voxels, MeshData& mesh, int axis) {
            auto quads = processAxisForQuads(voxels, axis);
            
            for (const auto& quad : quads) {
                createQuadGeometry(mesh, quad);
            }
        }
        
        std::vector<GreedyQuad> processAxisForQuads(const VoxelData& voxels, int axis) {
            std::vector<GreedyQuad> quads;
            
            // Create slice mask for this axis
            std::array<std::array<uint8_t, CHUNK_SIZE>, CHUNK_SIZE> mask;
            std::array<std::array<bool, CHUNK_SIZE>, CHUNK_SIZE> processed;
            
            // Process each slice along the axis
            for (int d = 0; d < CHUNK_SIZE; d++) {
                // Clear mask and processed flags
                for (int i = 0; i < CHUNK_SIZE; i++) {
                    for (int j = 0; j < CHUNK_SIZE; j++) {
                        mask[i][j] = 0;
                        processed[i][j] = false;
                    }
                }
                
                // Fill mask for this slice
                fillSliceMask(voxels, mask, axis, d);
                
                // Greedily merge faces in this slice
                for (int i = 0; i < CHUNK_SIZE; i++) {
                    for (int j = 0; j < CHUNK_SIZE; j++) {
                        if (processed[i][j] || mask[i][j] == 0) continue;
                        
                        uint8_t material = mask[i][j];
                        
                        // Find width of the quad
                        int width = 1;
                        while (i + width < CHUNK_SIZE && 
                               !processed[i + width][j] && 
                               mask[i + width][j] == material) {
                            width++;
                        }
                        
                        // Find height of the quad
                        int height = 1;
                        bool canExtendHeight = true;
                        while (j + height < CHUNK_SIZE && canExtendHeight) {
                            for (int w = 0; w < width; w++) {
                                if (processed[i + w][j + height] || 
                                    mask[i + w][j + height] != material) {
                                    canExtendHeight = false;
                                    break;
                                }
                            }
                            if (canExtendHeight) height++;
                        }
                        
                        // Mark processed region
                        for (int h = 0; h < height; h++) {
                            for (int w = 0; w < width; w++) {
                                processed[i + w][j + h] = true;
                            }
                        }
                        
                        // Create quad
                        GreedyQuad quad;
                        if (axis == 0) {
                            quad = {d, i, j, 1, width, height, material, 0};
                        } else if (axis == 1) {
                            quad = {i, d, j, width, 1, height, material, 2};
                        } else {
                            quad = {i, j, d, width, height, 1, material, 4};
                        }
                        
                        quads.push_back(quad);
                    }
                }
            }
            
            return quads;
        }
        
        void fillSliceMask(const VoxelData& voxels, 
                          std::array<std::array<uint8_t, CHUNK_SIZE>, CHUNK_SIZE>& mask,
                          int axis, int depth) {
            for (int i = 0; i < CHUNK_SIZE; i++) {
                for (int j = 0; j < CHUNK_SIZE; j++) {
                    uint8_t voxelType, neighborType;
                    
                    if (axis == 0) {  // X axis
                        voxelType = voxels[depth][i][j];
                        neighborType = (depth + 1 < CHUNK_SIZE) ? voxels[depth + 1][i][j] : 0;
                    } else if (axis == 1) {  // Y axis
                        voxelType = voxels[i][depth][j];
                        neighborType = (depth + 1 < CHUNK_SIZE) ? voxels[i][depth + 1][j] : 0;
                    } else {  // Z axis
                        voxelType = voxels[i][j][depth];
                        neighborType = (depth + 1 < CHUNK_SIZE) ? voxels[i][j][depth + 1] : 0;
                    }
                    
                    // Add face if voxel is solid and neighbor is empty
                    if (voxelType != 0 && neighborType == 0) {
                        mask[i][j] = voxelType;
                    }
                }
            }
        }
        
        void createQuadGeometry(MeshData& mesh, const GreedyQuad& quad) {
            // Convert greedy quad to actual geometry
            uint32_t baseIndex = static_cast<uint32_t>(mesh.vertices.size());
            
            // Create vertices based on quad dimensions and orientation
            auto vertices = createGreedyQuadVertices(quad);
            
            for (const auto& vertex : vertices) {
                mesh.vertices.push_back(vertex);
            }
            
            // Add indices
            mesh.indices.insert(mesh.indices.end(), {
                baseIndex + 0, baseIndex + 1, baseIndex + 2,
                baseIndex + 2, baseIndex + 3, baseIndex + 0
            });
            
            mesh.faceCount++;
        }
        
        std::array<Vertex, 4> createGreedyQuadVertices(const GreedyQuad& quad) {
            std::array<Vertex, 4> vertices;
            float fx = static_cast<float>(quad.x);
            float fy = static_cast<float>(quad.y);
            float fz = static_cast<float>(quad.z);
            float fw = static_cast<float>(quad.width);
            float fh = static_cast<float>(quad.height);
            
            const auto& normal = FACE_NORMALS[quad.faceDirection];
            
            // Create vertices based on face direction and quad dimensions
            switch (quad.faceDirection) {
                case 0: // +X face
                    vertices = {{
                        {fx+1, fy+0, fz+0, normal[0], normal[1], normal[2], 0, 0},
                        {fx+1, fy+fw, fz+0, normal[0], normal[1], normal[2], fw, 0},
                        {fx+1, fy+fw, fz+fh, normal[0], normal[1], normal[2], fw, fh},
                        {fx+1, fy+0, fz+fh, normal[0], normal[1], normal[2], 0, fh}
                    }};
                    break;
                // Add other face directions as needed...
                default:
                    // Fallback - create unit quad
                    vertices = {{
                        {fx, fy, fz, normal[0], normal[1], normal[2], 0, 0},
                        {fx+fw, fy, fz, normal[0], normal[1], normal[2], fw, 0},
                        {fx+fw, fy+fh, fz, normal[0], normal[1], normal[2], fw, fh},
                        {fx, fy+fh, fz, normal[0], normal[1], normal[2], 0, fh}
                    }};
                    break;
            }
            
            return vertices;
        }
    };
}

class MesherCoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test voxel data
        createTestVoxelData();
    }
    
    void createTestVoxelData() {
        // Clear all voxels
        for (int x = 0; x < VoxelMesher::CHUNK_SIZE; x++) {
            for (int y = 0; y < VoxelMesher::CHUNK_SIZE; y++) {
                for (int z = 0; z < VoxelMesher::CHUNK_SIZE; z++) {
                    testVoxels[x][y][z] = 0;
                }
            }
        }
    }
    
    VoxelMesher::VoxelData testVoxels;
    VoxelMesher::NaiveMesher naiveMesher;
    VoxelMesher::GreedyMesher greedyMesher;
};

TEST_F(MesherCoreTest, EmptyChunk) {
    // Empty chunk should produce no geometry
    auto mesh = naiveMesher.generateMesh(testVoxels);
    
    EXPECT_EQ(mesh.vertices.size(), 0);
    EXPECT_EQ(mesh.indices.size(), 0);
    EXPECT_EQ(mesh.faceCount, 0);
}

TEST_F(MesherCoreTest, SingleVoxel) {
    // Single voxel should create 6 faces (fully exposed)
    testVoxels[8][8][8] = 1;  // Place voxel in center
    
    auto mesh = naiveMesher.generateMesh(testVoxels);
    
    EXPECT_EQ(mesh.faceCount, 6);  // 6 faces for a cube
    EXPECT_EQ(mesh.vertices.size(), 24);  // 4 vertices per face
    EXPECT_EQ(mesh.indices.size(), 36);   // 6 indices per face
}

TEST_F(MesherCoreTest, TwoAdjacentVoxels) {
    // Two adjacent voxels should share a face (10 total faces)
    testVoxels[8][8][8] = 1;
    testVoxels[9][8][8] = 1;  // Adjacent in X direction
    
    auto mesh = naiveMesher.generateMesh(testVoxels);
    
    EXPECT_EQ(mesh.faceCount, 10);  // 6 + 6 - 2 shared faces
}

TEST_F(MesherCoreTest, SolidBlock) {
    // Solid 4x4x4 block should only have exterior faces
    for (int x = 4; x < 8; x++) {
        for (int y = 4; y < 8; y++) {
            for (int z = 4; z < 8; z++) {
                testVoxels[x][y][z] = 1;
            }
        }
    }
    
    auto mesh = naiveMesher.generateMesh(testVoxels);
    
    // Calculate expected faces: 6 faces * 4*4 area = 96 faces
    EXPECT_EQ(mesh.faceCount, 96);
}

TEST_F(MesherCoreTest, GreedyMeshingEfficiency) {
    // Create a large flat surface that should be optimized
    for (int x = 0; x < 8; x++) {
        for (int z = 0; z < 8; z++) {
            testVoxels[x][8][z] = 1;  // Flat surface at y=8
        }
    }
    
    auto naiveMesh = naiveMesher.generateMesh(testVoxels);
    auto greedyMesh = greedyMesher.generateMesh(testVoxels);
    auto greedyQuads = greedyMesher.generateQuads(testVoxels);
    
    // Naive mesher should create many small faces
    EXPECT_GT(naiveMesh.faceCount, 64);  // Many individual faces
    
    // Greedy mesher should create fewer, larger faces
    EXPECT_LT(greedyMesh.faceCount, naiveMesh.faceCount);
    
    // Should create large quads instead of many small ones
    EXPECT_GT(greedyQuads.size(), 0);
    
    std::cout << "Naive faces: " << naiveMesh.faceCount 
              << ", Greedy faces: " << greedyMesh.faceCount 
              << ", Quads: " << greedyQuads.size() << std::endl;
}

TEST_F(MesherCoreTest, MaterialConsistency) {
    // Different materials should not be merged
    testVoxels[8][8][8] = 1;  // Material 1
    testVoxels[9][8][8] = 2;  // Material 2
    
    auto greedyQuads = greedyMesher.generateQuads(testVoxels);
    
    // Should create separate quads for different materials
    int material1Quads = 0;
    int material2Quads = 0;
    
    for (const auto& quad : greedyQuads) {
        if (quad.materialId == 1) material1Quads++;
        if (quad.materialId == 2) material2Quads++;
    }
    
    EXPECT_GT(material1Quads, 0);
    EXPECT_GT(material2Quads, 0);
}

TEST_F(MesherCoreTest, MeshVertexNormals) {
    // Test that vertex normals are correctly calculated
    testVoxels[8][8][8] = 1;
    
    auto mesh = naiveMesher.generateMesh(testVoxels);
    
    // Check that normals are valid unit vectors
    for (const auto& vertex : mesh.vertices) {
        float normalLength = std::sqrt(vertex.nx * vertex.nx + 
                                     vertex.ny * vertex.ny + 
                                     vertex.nz * vertex.nz);
        EXPECT_NEAR(normalLength, 1.0f, 0.001f);
    }
}

TEST_F(MesherCoreTest, MeshIndicesValidity) {
    // Test that mesh indices are valid
    testVoxels[8][8][8] = 1;
    
    auto mesh = naiveMesher.generateMesh(testVoxels);
    
    // All indices should be within vertex array bounds
    for (const auto& index : mesh.indices) {
        EXPECT_LT(index, mesh.vertices.size());
    }
    
    // Should have multiples of 3 indices (triangles)
    EXPECT_EQ(mesh.indices.size() % 3, 0);
}

// Performance test for meshing algorithms
TEST_F(MesherCoreTest, MeshingPerformance) {
    // Create complex voxel pattern
    for (int x = 0; x < VoxelMesher::CHUNK_SIZE; x++) {
        for (int y = 0; y < VoxelMesher::CHUNK_SIZE; y++) {
            for (int z = 0; z < VoxelMesher::CHUNK_SIZE; z++) {
                // Create a checkerboard pattern
                if ((x + y + z) % 2 == 0) {
                    testVoxels[x][y][z] = 1;
                }
            }
        }
    }
    
    const int NUM_ITERATIONS = 100;
    
    // Time naive meshing
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        auto mesh = naiveMesher.generateMesh(testVoxels);
        volatile size_t dummy = mesh.vertices.size();  // Prevent optimization
        (void)dummy;
    }
    auto naiveTime = std::chrono::high_resolution_clock::now() - start;
    
    // Time greedy meshing  
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        auto mesh = greedyMesher.generateMesh(testVoxels);
        volatile size_t dummy = mesh.vertices.size();  // Prevent optimization
        (void)dummy;
    }
    auto greedyTime = std::chrono::high_resolution_clock::now() - start;
    
    auto naiveMicros = std::chrono::duration_cast<std::chrono::microseconds>(naiveTime).count();
    auto greedyMicros = std::chrono::duration_cast<std::chrono::microseconds>(greedyTime).count();
    
    std::cout << "Naive meshing: " << naiveMicros << " μs, "
              << "Greedy meshing: " << greedyMicros << " μs" << std::endl;
    
    // Performance expectations (adjust based on actual hardware)
    EXPECT_LT(naiveMicros, 100000);   // Less than 100ms for 100 iterations
    EXPECT_LT(greedyMicros, 200000);  // Greedy might be slower but more efficient
}