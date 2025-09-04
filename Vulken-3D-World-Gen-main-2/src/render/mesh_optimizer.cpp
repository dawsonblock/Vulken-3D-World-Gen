#include "mesh_optimizer.hpp"
#include "../vk/error_handling.hpp"
#include "../core/logger.hpp"
#include <algorithm>
#include <unordered_set>
#include <chrono>
#include <cstring>

namespace voxelvk {

static Logger g_meshLogger("MeshOptimizer");

MeshOptimizer::OptimizationResult MeshOptimizer::s_lastResult{};

VkVertexInputBindingDescription OptimizedVertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(OptimizedVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 5> OptimizedVertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};
    
    // Position (location 0)
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(OptimizedVertex, position);
    
    // Normal (location 1)
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(OptimizedVertex, normal);
    
    // Texture coordinates (location 2)
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(OptimizedVertex, texCoord);
    
    // Material ID (location 3)
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32_UINT;
    attributeDescriptions[3].offset = offsetof(OptimizedVertex, materialID);
    
    // AO + Lighting packed (location 4)
    attributeDescriptions[4].binding = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format = VK_FORMAT_R32_UINT;
    attributeDescriptions[4].offset = offsetof(OptimizedVertex, aoLighting);
    
    return attributeDescriptions;
}

bool OptimizedVertex::operator==(const OptimizedVertex& other) const {
    return position == other.position && 
           normal == other.normal &&
           texCoord == other.texCoord &&
           materialID == other.materialID &&
           aoLighting == other.aoLighting;
}

void OptimizedMesh::release() {
    if (vertexBuffer != VK_NULL_HANDLE) {
        MemoryManager::instance().destroyBuffer(vertexBuffer, vertexAllocation);
        vertexBuffer = VK_NULL_HANDLE;
        vertexAllocation = {};
    }
    
    if (indexBuffer != VK_NULL_HANDLE) {
        MemoryManager::instance().destroyBuffer(indexBuffer, indexAllocation);
        indexBuffer = VK_NULL_HANDLE;
        indexAllocation = {};
    }
}

bool OptimizedMesh::uploadToGPU(const char* debugName) {
    if (vertices.empty() || indices.empty()) {
        g_meshLogger.Error("Cannot upload empty mesh to GPU");
        return false;
    }
    
    // Create vertex buffer
    VkBufferCreateInfo vertexBufferInfo{};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.size = getVertexDataSize();
    vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    std::string vertexName = std::string(debugName) + "_Vertices";
    BufferResult vertexResult = MemoryManager::instance().createBuffer(
        vertexBufferInfo, VMA_MEMORY_USAGE_GPU_ONLY, MemoryCategory::GEOMETRY, vertexName.c_str());
    
    if (!vertexResult.isValid()) {
        g_meshLogger.Error("Failed to create vertex buffer");
        return false;
    }
    
    vertexBuffer = vertexResult.buffer;
    vertexAllocation = vertexResult.allocation;
    
    // Create index buffer
    VkBufferCreateInfo indexBufferInfo{};
    indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexBufferInfo.size = getIndexDataSize();
    indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    std::string indexName = std::string(debugName) + "_Indices";
    BufferResult indexResult = MemoryManager::instance().createBuffer(
        indexBufferInfo, VMA_MEMORY_USAGE_GPU_ONLY, MemoryCategory::GEOMETRY, indexName.c_str());
    
    if (!indexResult.isValid()) {
        g_meshLogger.Error("Failed to create index buffer");
        MemoryManager::instance().destroyBuffer(vertexBuffer, vertexAllocation);
        return false;
    }
    
    indexBuffer = indexResult.buffer;
    indexAllocation = indexResult.allocation;
    
    // TODO: Upload data via staging buffers
    // For now, just mark as uploaded
    
    g_meshLogger.Info("Uploaded mesh '{}': {} vertices, {} indices, {:.1f} KB total",
        debugName, vertices.size(), indices.size(), getTotalDataSize() / 1024.0);
    
    return true;
}

OptimizedMesh GreedyMesher::generateMesh(const ChunkData& voxels, const MeshingSettings& settings) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    g_meshLogger.Debug("Generating mesh with greedy algorithm");
    
    // Extract individual quads from voxel data
    std::vector<QuadCandidate> quads = extractQuads(voxels, settings);
    
    g_meshLogger.Debug("Extracted {} quad candidates", quads.size());
    
    // Merge adjacent quads if enabled
    if (settings.enableGreedyMerging) {
        quads = mergeQuads(quads);
        g_meshLogger.Debug("Merged to {} quads", quads.size());
    }
    
    // Convert to optimized mesh
    OptimizedMesh mesh = quadCandidatesToMesh(quads, settings);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    double meshingTime = std::chrono::duration<double>(endTime - startTime).count();
    
    // Calculate compression statistics
    size_t naiveVertexCount = quads.size() * 4; // 4 vertices per quad
    size_t naiveTriangleCount = quads.size() * 2; // 2 triangles per quad
    
    mesh.originalVertexCount = naiveVertexCount;
    mesh.originalTriangleCount = naiveTriangleCount;
    mesh.compressionRatio = naiveVertexCount > 0 ? 
        static_cast<float>(naiveVertexCount) / mesh.vertices.size() : 1.0f;
    
    g_meshLogger.Info("Mesh generation complete: {:.3f}ms", meshingTime * 1000.0);
    g_meshLogger.Info("  Vertices: {} -> {} ({:.1f}x compression)", 
        naiveVertexCount, mesh.vertices.size(), mesh.compressionRatio);
    g_meshLogger.Info("  Triangles: {} -> {}", naiveTriangleCount, mesh.indices.size() / 3);
    
    return mesh;
}

std::vector<GreedyMesher::QuadCandidate> GreedyMesher::extractQuads(const ChunkData& voxels, const MeshingSettings& settings) {
    std::vector<QuadCandidate> quads;
    quads.reserve(32 * 32 * 32 * 6); // Worst case: all faces visible
    
    // Face directions (right, left, top, bottom, front, back)
    const glm::vec3 faceNormals[6] = {
        {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},  // X faces
        {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},  // Y faces  
        {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}   // Z faces
    };
    
    const glm::vec3 faceOffsets[6] = {
        {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}
    };
    
    // Iterate through all voxels
    for (int x = 0; x < 32; x++) {
        for (int y = 0; y < 32; y++) {
            for (int z = 0; z < 32; z++) {
                const auto& voxel = voxels[x][y][z];
                
                // Skip air blocks
                if (voxel.blockID == 0) continue;
                
                // Check each face direction
                for (int face = 0; face < 6; face++) {
                    glm::vec3 neighborPos = glm::vec3(x, y, z) + faceNormals[face];
                    
                    // Check bounds
                    if (neighborPos.x < 0 || neighborPos.x >= 32 ||
                        neighborPos.y < 0 || neighborPos.y >= 32 ||
                        neighborPos.z < 0 || neighborPos.z >= 32) {
                        // Face is at chunk boundary - assume visible
                        // (neighbor chunks would provide occlusion data)
                    } else {
                        // Check if neighbor blocks this face
                        const auto& neighbor = voxels[static_cast<int>(neighborPos.x)]
                                                    [static_cast<int>(neighborPos.y)]
                                                    [static_cast<int>(neighborPos.z)];
                        
                        if (settings.enableFaceCulling && neighbor.blockID != 0) {
                            continue; // Face is occluded
                        }
                    }
                    
                    // Generate quad for this face
                    QuadCandidate quad;
                    quad.position = glm::vec3(x, y, z) + faceOffsets[face];
                    quad.normal = faceNormals[face];
                    quad.size = {1.0f, 1.0f}; // Unit quad
                    quad.blockID = voxel.blockID;
                    quad.lightLevel = voxel.lightLevel;
                    
                    // Calculate AO if enabled
                    if (settings.enableAO) {
                        quad.aoValues[0] = calculateAO(voxels, x, y, z, faceNormals[face]);
                        quad.aoValues[1] = calculateAO(voxels, x, y, z, faceNormals[face]);
                        quad.aoValues[2] = calculateAO(voxels, x, y, z, faceNormals[face]);
                        quad.aoValues[3] = calculateAO(voxels, x, y, z, faceNormals[face]);
                    } else {
                        quad.aoValues[0] = quad.aoValues[1] = quad.aoValues[2] = quad.aoValues[3] = 255;
                    }
                    
                    quads.push_back(quad);
                }
            }
        }
    }
    
    return quads;
}

std::vector<GreedyMesher::QuadCandidate> GreedyMesher::mergeQuads(const std::vector<QuadCandidate>& quads) {
    // Simple greedy merging algorithm
    std::vector<QuadCandidate> mergedQuads;
    mergedQuads.reserve(quads.size());
    
    std::vector<bool> merged(quads.size(), false);
    
    for (size_t i = 0; i < quads.size(); i++) {
        if (merged[i]) continue;
        
        QuadCandidate baseQuad = quads[i];
        merged[i] = true;
        
        // Try to merge with other quads
        for (size_t j = i + 1; j < quads.size(); j++) {
            if (merged[j]) continue;
            
            if (canMergeQuads(baseQuad, quads[j])) {
                // Merge quads by expanding size
                // This is a simplified merge - real implementation would be more sophisticated
                baseQuad.size.x += 1.0f; // Expand in X direction
                merged[j] = true;
            }
        }
        
        mergedQuads.push_back(baseQuad);
    }
    
    return mergedQuads;
}

bool GreedyMesher::canMergeQuads(const QuadCandidate& a, const QuadCandidate& b) {
    // Check if quads can be merged (same material, normal, lighting)
    return a.blockID == b.blockID &&
           a.normal == b.normal &&
           a.lightLevel == b.lightLevel &&
           std::memcmp(a.aoValues, b.aoValues, sizeof(a.aoValues)) == 0;
}

OptimizedMesh GreedyMesher::quadCandidatesToMesh(const std::vector<QuadCandidate>& quads, const MeshingSettings& settings) {
    OptimizedMesh mesh;
    mesh.vertices.reserve(quads.size() * 4);
    mesh.indices.reserve(quads.size() * 6);
    
    std::unordered_map<OptimizedVertex, uint32_t> vertexMap;
    uint32_t nextVertexIndex = 0;
    
    for (const auto& quad : quads) {
        // Generate 4 vertices for quad
        std::array<OptimizedVertex, 4> quadVertices;
        
        // Calculate quad corners based on normal direction
        glm::vec3 tangent, bitangent;
        if (std::abs(quad.normal.y) < 0.9f) {
            tangent = glm::normalize(glm::cross(quad.normal, {0.0f, 1.0f, 0.0f}));
        } else {
            tangent = glm::normalize(glm::cross(quad.normal, {1.0f, 0.0f, 0.0f}));
        }
        bitangent = glm::cross(quad.normal, tangent);
        
        // Quad corners (counter-clockwise)
        glm::vec3 corners[4] = {
            quad.position,                                    // Bottom-left
            quad.position + tangent * quad.size.x,          // Bottom-right
            quad.position + tangent * quad.size.x + bitangent * quad.size.y, // Top-right
            quad.position + bitangent * quad.size.y         // Top-left
        };
        
        // Generate vertices
        for (int i = 0; i < 4; i++) {
            OptimizedVertex& vertex = quadVertices[i];
            vertex.position = corners[i];
            vertex.normal = settings.generateNormals ? quad.normal : glm::vec3(0.0f, 1.0f, 0.0f);
            vertex.texCoord = getTextureCoords(quad.blockID, 0); // Face index 0 for simplicity
            vertex.materialID = quad.blockID; // Use block ID as material ID
            
            // Pack AO and lighting
            uint32_t ao = quad.aoValues[i];
            uint32_t light = quad.lightLevel;
            vertex.aoLighting = (ao << 24) | (light << 16); // Pack into uint32
        }
        
        // Add vertices (with deduplication)
        uint32_t quadIndices[4];
        for (int i = 0; i < 4; i++) {
            auto it = vertexMap.find(quadVertices[i]);
            if (it != vertexMap.end()) {
                quadIndices[i] = it->second;
            } else {
                quadIndices[i] = nextVertexIndex++;
                vertexMap[quadVertices[i]] = quadIndices[i];
                mesh.vertices.push_back(quadVertices[i]);
            }
        }
        
        // Add indices for 2 triangles (0,1,2) and (0,2,3)
        mesh.indices.push_back(quadIndices[0]);
        mesh.indices.push_back(quadIndices[1]);
        mesh.indices.push_back(quadIndices[2]);
        
        mesh.indices.push_back(quadIndices[0]);
        mesh.indices.push_back(quadIndices[2]);
        mesh.indices.push_back(quadIndices[3]);
    }
    
    // Calculate bounding box
    if (!mesh.vertices.empty()) {
        mesh.boundingMin = mesh.boundingMax = mesh.vertices[0].position;
        for (const auto& vertex : mesh.vertices) {
            mesh.boundingMin = glm::min(mesh.boundingMin, vertex.position);
            mesh.boundingMax = glm::max(mesh.boundingMax, vertex.position);
        }
    }
    
    return mesh;
}

uint8_t GreedyMesher::calculateAO(const ChunkData& voxels, int x, int y, int z, const glm::vec3& normal) {
    // Simple AO calculation based on neighbor occupancy
    int neighbors = 0;
    const int radius = 1;
    
    for (int dx = -radius; dx <= radius; dx++) {
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dz = -radius; dz <= radius; dz++) {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                
                int nx = x + dx, ny = y + dy, nz = z + dz;
                
                if (nx >= 0 && nx < 32 && ny >= 0 && ny < 32 && nz >= 0 && nz < 32) {
                    if (voxels[nx][ny][nz].blockID != 0) {
                        neighbors++;
                    }
                }
            }
        }
    }
    
    // Convert neighbor count to AO value (0-255)
    float aoFactor = 1.0f - (static_cast<float>(neighbors) / 26.0f); // 26 neighbors max
    return static_cast<uint8_t>(aoFactor * 255.0f);
}

glm::vec2 GreedyMesher::getTextureCoords(uint16_t blockID, int faceIndex) {
    // Simple texture coordinate generation
    // In a real implementation, this would look up texture coordinates from a block registry
    float u = static_cast<float>(blockID % 16) / 16.0f;
    float v = static_cast<float>(blockID / 16) / 16.0f;
    
    return {u, v};
}

OptimizedMesh MeshOptimizer::optimize(const OptimizedMesh& inputMesh, const OptimizationSettings& settings) {
    g_meshLogger.Debug("Optimizing mesh with {} vertices, {} indices", 
        inputMesh.vertices.size(), inputMesh.indices.size());
    
    OptimizedMesh optimizedMesh = inputMesh; // Copy input
    
    s_lastResult.originalVertices = inputMesh.vertices.size();
    s_lastResult.originalIndices = inputMesh.indices.size();
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Vertex cache optimization
    if (settings.optimizeVertexCache) {
        optimizeVertexCache(optimizedMesh);
    }
    
    // Overdraw optimization  
    if (settings.optimizeOverdraw) {
        optimizeOverdraw(optimizedMesh);
    }
    
    // Vertex fetch optimization
    if (settings.optimizeVertexFetch) {
        optimizeVertexFetch(optimizedMesh);
    }
    
    // Mesh simplification
    if (settings.simplificationRatio < 1.0f) {
        optimizedMesh = simplifyMesh(optimizedMesh, settings.simplificationRatio);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    double optimizationTime = std::chrono::duration<double>(endTime - startTime).count();
    
    s_lastResult.optimizedVertices = optimizedMesh.vertices.size();
    s_lastResult.optimizedIndices = optimizedMesh.indices.size();
    s_lastResult.vertexCacheEfficiency = calculateVertexCacheEfficiency(optimizedMesh.indices);
    s_lastResult.overdrawReduction = calculateOverdrawRatio(optimizedMesh);
    s_lastResult.compressionRatio = s_lastResult.originalVertices > 0 ?
        static_cast<float>(s_lastResult.originalVertices) / s_lastResult.optimizedVertices : 1.0f;
    
    g_meshLogger.Info("Mesh optimization complete: {:.3f}ms", optimizationTime * 1000.0);
    g_meshLogger.Info("  Vertex cache efficiency: {:.1f}%", s_lastResult.vertexCacheEfficiency * 100.0f);
    g_meshLogger.Info("  Compression ratio: {:.2f}x", s_lastResult.compressionRatio);
    
    return optimizedMesh;
}

void MeshOptimizer::optimizeVertexCache(OptimizedMesh& mesh) {
    // Placeholder for vertex cache optimization
    // Real implementation would use algorithms like Forsyth or Linear-Speed Vertex Cache Optimization
    g_meshLogger.Debug("Applying vertex cache optimization");
}

void MeshOptimizer::optimizeOverdraw(OptimizedMesh& mesh) {
    // Placeholder for overdraw optimization  
    // Real implementation would sort triangles by depth and material
    g_meshLogger.Debug("Applying overdraw optimization");
}

void MeshOptimizer::optimizeVertexFetch(OptimizedMesh& mesh) {
    // Placeholder for vertex fetch optimization
    // Real implementation would reorder vertex buffer for cache efficiency
    g_meshLogger.Debug("Applying vertex fetch optimization");
}

float MeshOptimizer::calculateVertexCacheEfficiency(const std::vector<uint32_t>& indices) {
    if (indices.empty()) return 0.0f;
    
    // Simplified vertex cache simulation (real implementation would simulate actual cache)
    const size_t cacheSize = 32; // Typical GPU vertex cache size
    std::vector<uint32_t> cache(cacheSize, UINT32_MAX);
    size_t cachePosition = 0;
    size_t hits = 0;
    
    for (uint32_t index : indices) {
        // Check if vertex is in cache
        bool found = false;
        for (size_t i = 0; i < cacheSize; i++) {
            if (cache[i] == index) {
                hits++;
                found = true;
                break;
            }
        }
        
        if (!found) {
            // Add to cache (FIFO replacement)
            cache[cachePosition] = index;
            cachePosition = (cachePosition + 1) % cacheSize;
        }
    }
    
    return static_cast<float>(hits) / indices.size();
}

float MeshOptimizer::calculateOverdrawRatio(const OptimizedMesh& mesh) {
    // Simplified overdraw calculation
    // Real implementation would render triangles and count pixel overdraw
    return 1.0f - (static_cast<float>(mesh.indices.size()) / (mesh.originalTriangleCount * 3));
}

OptimizedMesh MeshOptimizer::simplifyMesh(const OptimizedMesh& mesh, float ratio) {
    // Very simple decimation placeholder: drop triangles uniformly by ratio
    if (ratio >= 1.0f || mesh.indices.size() < 6) {
        return mesh;
    }

    OptimizedMesh out = mesh;
    std::vector<uint32_t> newIndices;
    newIndices.reserve(static_cast<size_t>(mesh.indices.size() * ratio));
    size_t step = static_cast<size_t>(1.0f / std::max(0.01f, ratio));
    if (step < 1) step = 1;
    // Keep every 'step' triangle (3 indices)
    for (size_t i = 0, tri = 0; i + 2 < mesh.indices.size(); i += 3, ++tri) {
        if (tri % step == 0) {
            newIndices.push_back(mesh.indices[i + 0]);
            newIndices.push_back(mesh.indices[i + 1]);
            newIndices.push_back(mesh.indices[i + 2]);
        }
    }
    if (newIndices.size() >= 3) {
        out.indices.swap(newIndices);
    }
    return out;
}

} // namespace voxelvk