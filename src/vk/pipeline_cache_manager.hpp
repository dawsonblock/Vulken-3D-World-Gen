#pragma once
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>
#include <functional>

namespace voxelvk {

/**
 * Production pipeline cache with versioned persistence and hot-reload
 */
struct PipelineSpec {
    std::vector<uint32_t> vertexSpirv;
    std::vector<uint32_t> fragmentSpirv;
    std::vector<uint32_t> computeSpirv;
    std::vector<uint32_t> geometrySpirv;
    std::vector<uint32_t> tessControlSpirv;
    std::vector<uint32_t> tessEvaluationSpirv;
    
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;
    
    // Vertex input
    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    
    // Viewport/scissor state
    uint32_t viewportCount = 1;
    uint32_t scissorCount = 1;
    
    // Rasterization state
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkBool32 depthBiasEnable = VK_FALSE;
    float lineWidth = 1.0f;
    
    // Multisampling
    VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkBool32 sampleShadingEnable = VK_FALSE;
    
    // Depth/stencil
    VkBool32 depthTestEnable = VK_TRUE;
    VkBool32 depthWriteEnable = VK_TRUE;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
    VkBool32 stencilTestEnable = VK_FALSE;
    
    // Color blending
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
    
    // Dynamic state
    std::vector<VkDynamicState> dynamicStates;
    
    // Pipeline layout
    VkPipelineLayout layout = VK_NULL_HANDLE;
    
    uint64_t getHash() const;
};

struct ComputePipelineSpec {
    std::vector<uint32_t> computeSpirv;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    
    uint64_t getHash() const;
};

/**
 * Pipeline cache header for version management
 */
struct PipelineCacheHeader {
    static constexpr uint32_t MAGIC = 0x56786C56; // "VxlV"
    static constexpr uint32_t VERSION = 1;
    
    uint32_t magic = MAGIC;
    uint32_t version = VERSION;
    uint32_t buildHash = 0;      // Hash of build configuration
    uint32_t driverVersion = 0;  // Driver version
    uint32_t vendorID = 0;       // GPU vendor
    uint32_t deviceID = 0;       // GPU device
    char buildUuid[16] = {0};    // Build UUID for invalidation
    
    bool isValid(uint32_t currentBuildHash, const char* currentUuid) const;
};

class PipelineCacheManager {
public:
    PipelineCacheManager(VkDevice device, VkPhysicalDevice physicalDevice);
    ~PipelineCacheManager();
    
    // Initialization
    bool initialize(const std::string& cacheDirectory = "pipeline_cache");
    void shutdown();
    
    // Pipeline creation with caching
    VkPipeline getOrCreateGraphicsPipeline(const std::string& name, const PipelineSpec& spec);
    VkPipeline getOrCreateComputePipeline(const std::string& name, const ComputePipelineSpec& spec);
    
    // Cache management
    bool loadFromDisk();
    bool saveToDisk();
    void invalidateCache();
    size_t getCacheSize() const;
    
    // Hot-reload support (Debug builds only)
    void enableHotReload(bool enable) { hotReloadEnabled_ = enable; }
    void checkForShaderChanges();
    void setShaderReloadCallback(std::function<void(const std::string&)> callback) {
        onShaderReload_ = callback;
    }
    
    // Statistics
    struct Stats {
        uint32_t cacheHits = 0;
        uint32_t cacheMisses = 0;
        uint32_t hotReloads = 0;
        uint32_t compilationErrors = 0;
        double totalCompileTime = 0.0;
    };
    
    const Stats& getStats() const { return stats_; }
    void resetStats() { stats_ = {}; }
    
    // Build UUID management
    static std::string generateBuildUuid();
    static void setBuildUuid(const std::string& uuid) { s_buildUuid = uuid; }
    
private:
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    VkPhysicalDeviceProperties deviceProperties_;
    
    VkPipelineCache vkCache_ = VK_NULL_HANDLE;
    std::string cacheDirectory_;
    std::string cacheFilePath_;
    
    // Pipeline storage
    std::unordered_map<std::string, VkPipeline> graphicsPipelines_;
    std::unordered_map<std::string, VkPipeline> computePipelines_;
    std::unordered_map<uint64_t, VkPipeline> pipelineHashMap_;
    
    // Hot-reload support
    bool hotReloadEnabled_ = false;
    std::unordered_map<std::string, uint64_t> shaderTimestamps_;
    std::function<void(const std::string&)> onShaderReload_;
    
    // Thread safety
    mutable std::mutex cacheMutex_;
    
    // Statistics
    Stats stats_;
    
    static std::string s_buildUuid;
    
    // Internal methods
    VkPipeline createGraphicsPipeline(const PipelineSpec& spec);
    VkPipeline createComputePipeline(const ComputePipelineSpec& spec);
    
    VkShaderModule createShaderModule(const std::vector<uint32_t>& spirv, const std::string& debugName);
    void destroyShaderModule(VkShaderModule module);
    
    std::string getCacheFilePath() const;
    bool createCacheDirectory();
    
    uint64_t getFileTimestamp(const std::string& filePath);
    void updateShaderTimestamp(const std::string& shaderPath);
    
    uint32_t calculateBuildHash() const;
};

/**
 * SPIR-V shader loading utilities
 */
class SPIRVLoader {
public:
    static std::vector<uint32_t> loadFromFile(const std::string& filePath);
    static std::vector<uint32_t> loadFromMemory(const void* data, size_t size);
    
    // Validation
    static bool validate(const std::vector<uint32_t>& spirv);
    static std::string getValidationError();
    
    // Reflection (optional, for debugging)
    static void reflectShader(const std::vector<uint32_t>& spirv, const std::string& debugName);
    
private:
    static std::string s_lastValidationError;
};

} // namespace voxelvk