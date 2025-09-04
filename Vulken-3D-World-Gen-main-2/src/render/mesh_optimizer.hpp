#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <unordered_map>
#include <future>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/geometric.hpp>
#include <glm/common.hpp>
#include "../vk/memory_manager.hpp"

namespace voxelvk {

/**
 * Vertex format for optimized voxel rendering
 */
struct OptimizedVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    uint32_t materialID;    // Packed material index
    uint32_t aoLighting;    // Ambient occlusion + lighting (packed)
    
    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions();
    
    bool operator==(const OptimizedVertex& other) const;
};

/**
 * Mesh data with optimized layout
 */
struct OptimizedMesh {
    std::vector<OptimizedVertex> vertices;
    std::vector<uint32_t> indices;
    
    // GPU resources
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VMAAllocation vertexAllocation{};
    VMAAllocation indexAllocation{};
    
    // Mesh metadata
    glm::vec3 boundingMin{0.0f};
    glm::vec3 boundingMax{0.0f};
    uint32_t materialCount = 1;
    
    // Statistics
    size_t originalVertexCount = 0;
    size_t originalTriangleCount = 0;
    float compressionRatio = 1.0f;
    
    bool isValid() const { return !vertices.empty() && !indices.empty(); }
    size_t getVertexDataSize() const { return vertices.size() * sizeof(OptimizedVertex); }
    size_t getIndexDataSize() const { return indices.size() * sizeof(uint32_t); }
    size_t getTotalDataSize() const { return getVertexDataSize() + getIndexDataSize(); }
    
    void release();
    bool uploadToGPU(const char* debugName = "OptimizedMesh");
};

/**
 * Greedy meshing algorithm for voxel chunks
 */
class GreedyMesher {
public:
    struct VoxelData {
        uint16_t blockID;
        uint8_t aoValues[4];  // AO values for each corner
        uint8_t lightLevel;   // Light level (0-15)
    };
    
    using ChunkData = std::array<std::array<std::array<VoxelData, 32>, 32>, 32>;
    
    struct MeshingSettings {
        bool enableAO = true;           // Ambient occlusion
        bool enableFaceCulling = true;  // Cull interior faces
        bool enableGreedyMerging = true;// Merge adjacent faces
        float aoStrength = 0.3f;        // AO darkening factor
        bool generateNormals = true;    // Calculate vertex normals
        
        static MeshingSettings HighQuality() { return {true, true, true, 0.3f, true}; }
        static MeshingSettings Performance() { return {false, true, true, 0.1f, false}; }
        static MeshingSettings Debug()       { return {false, false, false, 0.0f, true}; }
    };
    
    static OptimizedMesh generateMesh(const ChunkData& voxels, const MeshingSettings& settings = MeshingSettings::HighQuality());
    
private:
    struct QuadCandidate {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 size;
        uint16_t blockID;
        uint8_t aoValues[4];
        uint8_t lightLevel;
    };
    
    static std::vector<QuadCandidate> extractQuads(const ChunkData& voxels, const MeshingSettings& settings);
    static std::vector<QuadCandidate> mergeQuads(const std::vector<QuadCandidate>& quads);
    static OptimizedMesh quadCandidatesToMesh(const std::vector<QuadCandidate>& quads, const MeshingSettings& settings);
    
    static bool canMergeQuads(const QuadCandidate& a, const QuadCandidate& b);
    static uint8_t calculateAO(const ChunkData& voxels, int x, int y, int z, const glm::vec3& normal);
    static glm::vec2 getTextureCoords(uint16_t blockID, int faceIndex);
};

/**
 * Mesh optimization using industry-standard algorithms
 */
class MeshOptimizer {
public:
    struct OptimizationSettings {
        bool optimizeVertexCache = true;    // Optimize for GPU vertex cache
        bool optimizeOverdraw = true;       // Optimize for GPU overdraw reduction
        bool optimizeVertexFetch = true;    // Optimize vertex fetch efficiency
        float simplificationRatio = 1.0f;  // Mesh simplification (0.0-1.0)
        
        static OptimizationSettings Aggressive() { return {true, true, true, 0.8f}; }
        static OptimizationSettings Balanced()   { return {true, true, true, 1.0f}; }
        static OptimizationSettings Disabled()   { return {false, false, false, 1.0f}; }
    };
    
    struct OptimizationResult {
        size_t originalVertices = 0;
        size_t optimizedVertices = 0;
        size_t originalIndices = 0;
        size_t optimizedIndices = 0;
        float vertexCacheEfficiency = 0.0f;
        float overdrawReduction = 0.0f;
        float compressionRatio = 1.0f;
    };
    
    static OptimizedMesh optimize(const OptimizedMesh& inputMesh, const OptimizationSettings& settings = OptimizationSettings::Balanced());
    static OptimizationResult getLastOptimizationResult() { return s_lastResult; }
    
private:
    static OptimizationResult s_lastResult;
    
    static void optimizeVertexCache(OptimizedMesh& mesh);
    static void optimizeOverdraw(OptimizedMesh& mesh);
    static void optimizeVertexFetch(OptimizedMesh& mesh);
    static OptimizedMesh simplifyMesh(const OptimizedMesh& mesh, float ratio);
    
    static float calculateVertexCacheEfficiency(const std::vector<uint32_t>& indices);
    static float calculateOverdrawRatio(const OptimizedMesh& mesh);
};

/**
 * Bindless material table for efficient rendering
 */
struct MaterialProperties {
    glm::vec3 albedo = {1.0f, 1.0f, 1.0f};
    float roughness = 0.8f;
    float metallic = 0.0f;
    float emission = 0.0f;
    glm::vec2 textureScale = {1.0f, 1.0f};
    
    // Texture indices (bindless)
    uint32_t albedoTexture = 0;
    uint32_t normalTexture = 0;
    uint32_t materialTexture = 0;  // Packed roughness/metallic/AO
    uint32_t emissionTexture = 0;
};

class MaterialManager {
public:
    MaterialManager(VkDevice device);
    
    bool initialize(uint32_t maxMaterials = 256);
    void shutdown();
    
    // Material registration
    uint32_t registerMaterial(const MaterialProperties& material, const std::string& name);
    bool updateMaterial(uint32_t materialID, const MaterialProperties& material);
    
    // Bindless table management
    VkBuffer getMaterialBuffer() const { return materialBuffer_; }
    VkDescriptorSet getBindlessDescriptorSet() const { return bindlessDescriptorSet_; }
    
    // Statistics
    uint32_t getMaterialCount() const { return static_cast<uint32_t>(materials_.size()); }
    size_t getMemoryUsage() const;
    
    void logStats() const;
    
private:
    VkDevice device_;
    
    std::vector<MaterialProperties> materials_;
    std::unordered_map<std::string, uint32_t> materialNameMap_;
    
    // GPU resources
    VkBuffer materialBuffer_ = VK_NULL_HANDLE;
    VMAAllocation materialAllocation_{};
    void* materialMappedData_ = nullptr;
    
    VkDescriptorSetLayout bindlessLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet bindlessDescriptorSet_ = VK_NULL_HANDLE;
    
    uint32_t maxMaterials_ = 256;
    bool needsUpload_ = true;
    
    bool createMaterialBuffer();
    bool createBindlessDescriptors();
    void uploadMaterialData();
};

/**
 * Chunk meshing system for voxel world
 */
class ChunkMesher {
public:
    struct ChunkMeshData {
        OptimizedMesh solidMesh;      // Opaque geometry
        OptimizedMesh transparentMesh; // Transparent geometry (water, glass)
        
        // Collision mesh (simplified)
        std::vector<glm::vec3> collisionVertices;
        std::vector<uint32_t> collisionIndices;
        
        // Metadata
        glm::vec3 chunkPosition{0.0f};
        uint32_t visibleFaces = 0;
        uint32_t materialVariations = 0;
        
        bool isEmpty() const { return solidMesh.vertices.empty() && transparentMesh.vertices.empty(); }
        size_t getTotalMemoryUsage() const;
    };
    
    ChunkMesher(MaterialManager& materialManager);
    
    // Generate mesh from voxel data
    ChunkMeshData generateChunkMesh(
        const GreedyMesher::ChunkData& voxelData,
        const glm::vec3& chunkPosition,
        const GreedyMesher::MeshingSettings& settings = GreedyMesher::MeshingSettings::HighQuality()
    );
    
    // Async meshing for streaming
    std::future<ChunkMeshData> generateChunkMeshAsync(
        const GreedyMesher::ChunkData& voxelData,
        const glm::vec3& chunkPosition,
        const GreedyMesher::MeshingSettings& settings = GreedyMesher::MeshingSettings::HighQuality()
    );
    
    // Statistics
    struct MeshingStats {
        size_t totalChunksMeshed = 0;
        size_t totalVerticesGenerated = 0;
        size_t totalTrianglesGenerated = 0;
        double totalMeshingTime = 0.0;
        float averageCompressionRatio = 0.0f;
    };
    
    const MeshingStats& getStats() const { return stats_; }
    void resetStats() { stats_ = MeshingStats{}; }
    void logStats() const;
    
private:
    MaterialManager& materialManager_;
    MeshingStats stats_{};
    
    void processSolidVoxels(const GreedyMesher::ChunkData& voxels, ChunkMeshData& meshData, const GreedyMesher::MeshingSettings& settings);
    void processTransparentVoxels(const GreedyMesher::ChunkData& voxels, ChunkMeshData& meshData, const GreedyMesher::MeshingSettings& settings);
    void generateCollisionMesh(const GreedyMesher::ChunkData& voxels, ChunkMeshData& meshData);
    
    uint32_t getMaterialForBlock(uint16_t blockID);
    bool isBlockTransparent(uint16_t blockID);
    bool isBlockSolid(uint16_t blockID);
};

} // namespace voxelvk

// Hash function for OptimizedVertex (for deduplication)
namespace std {
    template<>
    struct hash<voxelvk::OptimizedVertex> {
        size_t operator()(const voxelvk::OptimizedVertex& vertex) const {
            size_t h1 = hash<float>{}(vertex.position.x);
            size_t h2 = hash<float>{}(vertex.position.y);
            size_t h3 = hash<float>{}(vertex.position.z);
            size_t h4 = hash<uint32_t>{}(vertex.materialID);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
        }
    };
}