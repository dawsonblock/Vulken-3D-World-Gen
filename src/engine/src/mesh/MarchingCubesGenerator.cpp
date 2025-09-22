#include "engine/Mesh/MarchingCubesGenerator.h"
#include "MarchingCubesTables.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace engine {

// Marching cubes edge table (256 cases)
static const std::array<int, 256> edgeTable = {
    0x0, 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c, 0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
    0x190, 0x99, 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c, 0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
    0x230, 0x339, 0x33, 0x13a, 0x636, 0x73f, 0x435, 0x53c, 0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
    0x3a0, 0x2a9, 0x1a3, 0xaa, 0x7a6, 0x6af, 0x5a5, 0x4ac, 0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
    0x460, 0x569, 0x663, 0x76a, 0x66, 0x16f, 0x265, 0x36c, 0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
    0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff, 0x3f5, 0x2fc, 0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
    0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55, 0x15c, 0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
    0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc, 0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
    0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc, 0xcc, 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
    0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c, 0x15c, 0x55, 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
    0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc, 0x2fc, 0x3f5, 0xff, 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
    0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c, 0x36c, 0x265, 0x16f, 0x66, 0x76a, 0x663, 0x569, 0x460,
    0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac, 0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa, 0x1a3, 0x2a9, 0x3a0,
    0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c, 0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33, 0x339, 0x230,
    0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c, 0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99, 0x190,
    0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c, 0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0
};

// Edge vertices for cube
static const std::array<std::array<int, 2>, 12> edgeVertices = {{
    {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
    {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
    {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}}
}};

// Cube vertices
static const std::array<std::array<float, 3>, 8> cubeVertices = {{
    {{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}},
    {{1.0f, 1.0f, 0.0f}}, {{0.0f, 1.0f, 0.0f}},
    {{0.0f, 0.0f, 1.0f}}, {{1.0f, 0.0f, 1.0f}},
    {{1.0f, 1.0f, 1.0f}}, {{0.0f, 1.0f, 1.0f}}
}};

Mesh MarchingCubesGenerator::generate(const VoxelChunk& chunk, const MeshGenParams& params) {
    Mesh mesh;

    // Reserve space for vertices and indices
    mesh.positions.reserve(chunk.voxels.size() * 3);
    mesh.normals.reserve(chunk.voxels.size() * 3);
    mesh.indices.reserve(chunk.voxels.size() * 3);

    const float isoLevel = params.isoLevel / 255.0f;

    // Process each voxel cube
    for (int z = 0; z < kChunkDim - 1; ++z) {
        for (int y = 0; y < kChunkDim - 1; ++y) {
            for (int x = 0; x < kChunkDim - 1; ++x) {
                // Get cube corner values
                std::array<float, 8> values;
                std::array<std::array<float, 3>, 8> positions;

                for (int i = 0; i < 8; ++i) {
                    int vx = x + static_cast<int>(cubeVertices[i][0]);
                    int vy = y + static_cast<int>(cubeVertices[i][1]);
                    int vz = z + static_cast<int>(cubeVertices[i][2]);

                    if (vx < kChunkDim && vy < kChunkDim && vz < kChunkDim) {
                        int idx = VoxelChunk::index(vx, vy, vz);
                        values[i] = chunk.voxels[idx].density / 255.0f;
                        positions[i] = {{static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vz)}};
                    } else {
                        values[i] = 0.0f;
                        positions[i] = {{static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vz)}};
                    }
                }

                // Determine cube index
                int cubeIndex = 0;
                for (int i = 0; i < 8; ++i) {
                    if (values[i] < isoLevel) {
                        cubeIndex |= (1 << i);
                    }
                }

                // Generate triangles for this cube
                const auto& edges = edgeTable[cubeIndex];
                if (edges == 0) continue;

                std::array<std::array<float, 3>, 12> edgePoints;

                // Calculate edge intersection points
                for (int i = 0; i < 12; ++i) {
                    if (edges & (1 << i)) {
                        int v1 = edgeVertices[i][0];
                        int v2 = edgeVertices[i][1];

                        float t = (isoLevel - values[v1]) / (values[v2] - values[v1]);
                        t = std::clamp(t, 0.0f, 1.0f);

                        edgePoints[i][0] = positions[v1][0] + t * (positions[v2][0] - positions[v1][0]);
                        edgePoints[i][1] = positions[v1][1] + t * (positions[v2][1] - positions[v1][1]);
                        edgePoints[i][2] = positions[v1][2] + t * (positions[v2][2] - positions[v1][2]);
                    }
                }

                // Generate triangles
                const auto& triangles = triTable[cubeIndex];
                for (int i = 0; i < 16 && triangles[i] != -1; i += 3) {
                    for (int j = 0; j < 3; ++j) {
                        int edgeIdx = triangles[i + j];
                        const auto& point = edgePoints[edgeIdx];

                        mesh.positions.push_back(point[0]);
                        mesh.positions.push_back(point[1]);
                        mesh.positions.push_back(point[2]);

                        // Simple normal calculation (can be improved)
                        mesh.normals.push_back(0.0f);
                        mesh.normals.push_back(0.0f);
                        mesh.normals.push_back(1.0f);

                        mesh.indices.push_back(static_cast<uint32_t>(mesh.positions.size() / 3 - 1));
                    }
                }
            }
        }
    }

    return mesh;
}

} // namespace engine
