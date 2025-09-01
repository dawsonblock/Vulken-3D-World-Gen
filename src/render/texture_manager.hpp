#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include <future>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include "../vk/memory_manager.hpp"

namespace voxelvk {

/**
 * Texture format targets optimized for GPU consumption
 */
enum class TextureFormat {
    // Block compressed formats (optimal)
    BC7_UNORM,          // High quality color (UASTC source)
    BC5_UNORM,          // Normal maps (2-channel)
    BC4_UNORM,          // Single channel (roughness, metallic)
    BC1_UNORM,          // Low quality color (legacy)
    
    // Fallback uncompressed formats
    RGBA8_UNORM,        // Fallback for BC7
    RG8_UNORM,          // Fallback for BC5  
    R8_UNORM,           // Fallback for BC4
    
    // HDR formats
    RGBA16F,            // HDR color
    RG16F,              // HDR normal maps
    
    // Depth formats
    D32_SFLOAT,         // 32-bit depth
    D24_UNORM_S8_UINT   // 24-bit depth + 8-bit stencil
};

/**
 * Texture compression settings for build-time transcoding
 */
struct TextureCompressionSettings {
    TextureFormat targetFormat = TextureFormat::BC7_UNORM;
    uint32_t quality = 128;           // BasisU quality (0-255)
    bool generateMipmaps = true;      // Generate mip chain
    bool enableSRGB = false;          // sRGB color space
    uint32_t maxResolution = 4096;    // Maximum dimension
    
    // Compression presets
    static TextureCompressionSettings HighQuality()   { return {TextureFormat::BC7_UNORM, 255, true, false, 4096}; }
    static TextureCompressionSettings Balanced()      { return {TextureFormat::BC7_UNORM, 128, true, false, 2048}; }
    static TextureCompressionSettings Performance()   { return {TextureFormat::BC1_UNORM, 64,  true, false, 1024}; }
    static TextureCompressionSettings NormalMap()     { return {TextureFormat::BC5_UNORM, 192, true, false, 2048}; }
    static TextureCompressionSettings SingleChannel() { return {TextureFormat::BC4_UNORM, 128, true, false, 1024}; }
};

/**
 * Texture asset with GPU resources
 */
struct TextureAsset {
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VMAAllocation allocation{};
    
    // Metadata
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    size_t sizeBytes = 0;
    
    // Loading state
    bool isLoaded = false;
    bool isLoading = false;
    
    // Texture coordinates and atlas info
    float uScale = 1.0f;
    float vScale = 1.0f;
    float uOffset = 0.0f;
    float vOffset = 0.0f;
    
    bool isValid() const { return image != VK_NULL_HANDLE && imageView != VK_NULL_HANDLE; }
    void release();
};

/**
 * KTX2 texture loader with basis universal support
 */
class KTX2Loader {
public:
    struct LoadResult {
        bool success = false;
        std::vector<char> textureData;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mipLevels = 1;
        VkFormat format = VK_FORMAT_UNDEFINED;
        std::string errorMessage;
    };
    
    static LoadResult loadFromFile(const std::string& filepath);
    static LoadResult loadFromMemory(const void* data, size_t size);
    
    // Build-time texture transcoding utilities
    struct TranscodeResult {
        bool success = false;
        std::vector<char> compressedData;
        TextureFormat targetFormat;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mipLevels = 1;
        std::string errorMessage;
    };
    
    static TranscodeResult transcodeFromPNG(const std::string& pngPath, const TextureCompressionSettings& settings);
    static bool writeKTX2File(const TranscodeResult& result, const std::string& outputPath);
    
private:
    static VkFormat basisFormatToVulkan(uint32_t basisFormat);
    static bool initializeBasisU();
    static void shutdownBasisU();
};

/**
 * Production texture manager with streaming and caching
 */
class TextureManager {
public:
    TextureManager(VkDevice device, VkPhysicalDevice physicalDevice);
    ~TextureManager();
    
    // Initialization
    bool initialize(const std::string& textureDirectory = "assets/textures");
    void shutdown();
    
    // Texture loading (async)
    std::shared_ptr<TextureAsset> loadTexture(const std::string& name);
    std::shared_ptr<TextureAsset> loadTextureSync(const std::string& name);
    
    // Texture creation
    std::shared_ptr<TextureAsset> createTexture2D(
        uint32_t width, uint32_t height, 
        VkFormat format,
        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        const char* debugName = "Texture2D"
    );
    
    std::shared_ptr<TextureAsset> createRenderTarget(
        uint32_t width, uint32_t height,
        VkFormat format,
        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        const char* debugName = "RenderTarget"
    );
    
    // Weather-specific texture creation
    std::shared_ptr<TextureAsset> createCloudTexture(uint32_t resolution = 512);
    std::shared_ptr<TextureAsset> createNoiseTexture(uint32_t resolution = 256);
    std::shared_ptr<TextureAsset> createPrecipitationTexture(uint32_t particleCount = 10000);
    
    // Texture streaming and LOD
    void setStreamingDistance(float distance) { streamingDistance_ = distance; }
    void updateStreaming(const glm::vec3& viewerPosition);
    
    // Memory management
    size_t getTextureMemoryUsage() const;
    bool evictUnusedTextures(); // Returns bytes freed
    void setMaxTextureMemory(size_t maxBytes) { maxTextureMemory_ = maxBytes; }
    
    // Texture atlas support
    struct AtlasEntry {
        std::shared_ptr<TextureAsset> texture;
        float uMin, vMin, uMax, vMax;
    };
    
    bool createTextureAtlas(const std::vector<std::string>& textureNames, 
                           const std::string& atlasName,
                           uint32_t atlasSize = 2048);
    
    AtlasEntry getAtlasEntry(const std::string& textureName) const;
    
    // Statistics
    struct Stats {
        size_t totalTextures = 0;
        size_t loadedTextures = 0;
        size_t cachedTextures = 0;
        size_t memoryUsage = 0;
        size_t memoryBudget = 0;
        double cacheHitRatio = 0.0;
        size_t streamingLoads = 0;
    };
    
    Stats getStats() const;
    void logStats() const;
    
private:
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    
    std::string textureDirectory_;
    float streamingDistance_ = 1000.0f;
    size_t maxTextureMemory_ = 1024 * 1024 * 1024; // 1GB default
    
    // Texture cache
    std::unordered_map<std::string, std::shared_ptr<TextureAsset>> textureCache_;
    std::unordered_map<std::string, AtlasEntry> atlasEntries_;
    mutable std::mutex textureMutex_;
    
    // Loading thread pool for async operations
    std::vector<std::future<void>> loadingTasks_;
    
    // Statistics
    mutable Stats stats_{};
    
    // Internal methods
    std::shared_ptr<TextureAsset> createTextureFromKTX2(const KTX2Loader::LoadResult& loadResult, const std::string& debugName);
    VkSampler createTextureSampler(bool enableMipmaps, bool enableAnisotropy);
    
    std::string getTexturePath(const std::string& name) const;
    bool textureExists(const std::string& name) const;
    
    void updateCacheStats() const;
};

/**
 * Build-time texture processing pipeline
 */
class TextureBuildPipeline {
public:
    // Build-time PNG → KTX2 conversion
    static bool processTextureDirectory(
        const std::string& sourceDir,
        const std::string& outputDir,
        const TextureCompressionSettings& settings = TextureCompressionSettings::Balanced()
    );
    
    // Individual texture processing
    static bool processTexture(
        const std::string& sourcePath,
        const std::string& outputPath, 
        const TextureCompressionSettings& settings
    );
    
    // Batch processing with parallel transcoding
    static bool processTextureList(
        const std::vector<std::string>& sourcePaths,
        const std::string& outputDir,
        const TextureCompressionSettings& settings,
        int numThreads = 4
    );
    
private:
    static bool validateInputTexture(const std::string& path);
    static bool createOutputDirectory(const std::string& dir);
};

/**
 * Texture streaming system for large worlds
 */
class TextureStreamer {
public:
    TextureStreamer(TextureManager& textureManager);
    
    void setViewerPosition(const glm::vec3& position);
    void setStreamingRadius(float radius) { streamingRadius_ = radius; }
    
    void registerStreamingTexture(const std::string& name, const glm::vec3& worldPosition);
    void unregisterStreamingTexture(const std::string& name);
    
    void updateStreaming();
    
    struct StreamingStats {
        size_t texturesInRange = 0;
        size_t texturesLoaded = 0;
        size_t texturesUnloaded = 0;
        size_t bytesStreamed = 0;
    };
    
    const StreamingStats& getStats() const { return stats_; }
    
private:
    TextureManager& textureManager_;
    glm::vec3 viewerPosition_{0.0f};
    float streamingRadius_ = 1000.0f;
    
    struct StreamingTexture {
        std::string name;
        glm::vec3 position;
        bool isLoaded = false;
        std::shared_ptr<TextureAsset> asset;
    };
    
    std::unordered_map<std::string, StreamingTexture> streamingTextures_;
    StreamingStats stats_{};
    mutable std::mutex streamingMutex_;
};

} // namespace voxelvk