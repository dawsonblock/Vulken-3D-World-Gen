#include "texture_manager.hpp"
#include "../vk/error_handling.hpp"
#include "../core/logger.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <glm/vec3.hpp>
#include <glm/geometric.hpp>

namespace voxelvk {

static Logger g_textureLogger("TextureManager");

void TextureAsset::release() {
    // Note: device is managed by TextureManager, not stored in TextureAsset
    // The actual Vulkan resource destruction is handled by TextureManager
    
    // Reset local state
    image = VK_NULL_HANDLE;
    imageView = VK_NULL_HANDLE;
    sampler = VK_NULL_HANDLE;
    allocation = {};
    
    width = 0;
    height = 0;
    mipLevels = 1;
    format = VK_FORMAT_UNDEFINED;
    sizeBytes = 0;
    
    isLoaded = false;
    isLoading = false;
}

TextureManager::TextureManager(VkDevice device, VkPhysicalDevice physicalDevice) 
    : device_(device), physicalDevice_(physicalDevice) {
    
    g_textureLogger.Info("TextureManager initialized");
}

TextureManager::~TextureManager() {
    shutdown();
}

bool TextureManager::initialize(const std::string& textureDirectory) {
    textureDirectory_ = textureDirectory;
    
    g_textureLogger.Info("Initializing texture manager");
    g_textureLogger.Info("  Texture directory: {}", textureDirectory);
    g_textureLogger.Info("  Max texture memory: {:.1f} MB", maxTextureMemory_ / (1024.0 * 1024.0));
    g_textureLogger.Info("  Streaming distance: {:.1f} units", streamingDistance_);
    
    // Verify texture directory exists
    if (!std::filesystem::exists(textureDirectory_)) {
        g_textureLogger.Warn("Texture directory does not exist: {}", textureDirectory_);
        g_textureLogger.Info("Creating texture directory...");
        
        try {
            std::filesystem::create_directories(textureDirectory_);
        } catch (const std::exception& e) {
            g_textureLogger.Error("Failed to create texture directory: {}", e.what());
            return false;
        }
    }
    
    g_textureLogger.Info("Texture manager initialized successfully");
    return true;
}

void TextureManager::shutdown() {
    g_textureLogger.Info("Shutting down texture manager");
    
    // Log final statistics
    logStats();
    
    // Wait for any pending loading tasks
    for (auto& task : loadingTasks_) {
        if (task.valid()) {
            task.wait();
        }
    }
    loadingTasks_.clear();
    
    // Release all cached textures
    std::lock_guard<std::mutex> lock(textureMutex_);
    
    for (auto& [name, texture] : textureCache_) {
        if (texture) {
            texture->release();
        }
    }
    
    textureCache_.clear();
    atlasEntries_.clear();
    
    g_textureLogger.Info("Texture manager shutdown complete");
}

std::shared_ptr<TextureAsset> TextureManager::loadTexture(const std::string& name) {
    std::lock_guard<std::mutex> lock(textureMutex_);
    
    // Check cache first
    auto it = textureCache_.find(name);
    if (it != textureCache_.end()) {
        stats_.totalTextures++;
        if (it->second->isLoaded) {
            stats_.cachedTextures++;
            g_textureLogger.Debug("Texture '{}' loaded from cache", name);
        }
        return it->second;
    }
    
    // Create new texture asset
    auto texture = std::make_shared<TextureAsset>();
    texture->isLoading = true;
    textureCache_[name] = texture;
    
    // Launch async loading task
    auto future = std::async(std::launch::async, [this, name, texture]() {
        loadTextureAsync(name, texture);
    });
    
    loadingTasks_.push_back(std::move(future));
    
    // Clean up completed tasks
    loadingTasks_.erase(
        std::remove_if(loadingTasks_.begin(), loadingTasks_.end(),
            [](const std::future<void>& task) {
                return task.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            }),
        loadingTasks_.end()
    );
    
    return texture;
}

std::shared_ptr<TextureAsset> TextureManager::loadTextureSync(const std::string& name) {
    std::lock_guard<std::mutex> lock(textureMutex_);
    
    // Check cache first
    auto it = textureCache_.find(name);
    if (it != textureCache_.end() && it->second->isLoaded) {
        return it->second;
    }
    
    // Create and load synchronously
    auto texture = std::make_shared<TextureAsset>();
    texture->isLoading = true;
    
    loadTextureAsync(name, texture); // Actually synchronous in this call
    
    if (texture->isLoaded) {
        textureCache_[name] = texture;
    }
    
    return texture;
}

std::shared_ptr<TextureAsset> TextureManager::createTexture2D(
    uint32_t width, uint32_t height,
    VkFormat format,
    VkImageUsageFlags usage,
    const char* debugName) {
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    ImageResult allocation = MemoryManager::instance().createImage(
        imageInfo, VMA_MEMORY_USAGE_GPU_ONLY, MemoryCategory::TEXTURES, debugName);
    
    if (!allocation.isValid()) {
        g_textureLogger.Error("Failed to create texture image");
        return nullptr;
    }
    
    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = allocation.image; // Extract image from allocation
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    VkImageView imageView;
    VkResult result = vkCreateImageView(device_, &viewInfo, nullptr, &imageView);
    if (result != VK_SUCCESS) {
        CHECK_VK_OBJECT(result, VkErrorCategory::RESOURCE_CREATION, "texture_image_view");
        MemoryManager::instance().destroyImage(allocation.image, allocation.allocation);
        return nullptr;
    }
    
    // Create texture asset
    auto texture = std::make_shared<TextureAsset>();
    texture->image = allocation.image;
    texture->imageView = imageView;
    texture->allocation = allocation.allocation;
    texture->width = width;
    texture->height = height;
    texture->format = format;
    texture->sizeBytes = allocation.allocation.size;
    texture->isLoaded = true;
    
    VK_OBJECT_NAME(device_, imageView, VK_OBJECT_TYPE_IMAGE_VIEW, debugName);
    
    g_textureLogger.Info("Created texture '{}': {}x{}, {:.1f} KB", 
        debugName, width, height, allocation.allocation.size / 1024.0);
    
    return texture;
}

std::shared_ptr<TextureAsset> TextureManager::createCloudTexture(uint32_t resolution) {
    return createTexture2D(
        resolution, resolution,
        VK_FORMAT_R16G16B16A16_SFLOAT, // High precision for cloud accumulation
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        "CloudTexture"
    );
}

std::shared_ptr<TextureAsset> TextureManager::createNoiseTexture(uint32_t resolution) {
    return createTexture2D(
        resolution, resolution,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        "NoiseTexture"
    );
}

size_t TextureManager::getTextureMemoryUsage() const {
    std::lock_guard<std::mutex> lock(textureMutex_);
    
    size_t totalMemory = 0;
    for (const auto& [name, texture] : textureCache_) {
        if (texture && texture->isLoaded) {
            totalMemory += texture->sizeBytes;
        }
    }
    
    return totalMemory;
}

bool TextureManager::evictUnusedTextures() {
    std::lock_guard<std::mutex> lock(textureMutex_);
    
    size_t bytesFreed = 0;
    
    // Simple LRU eviction (in a real implementation, you'd track access times)
    auto it = textureCache_.begin();
    while (it != textureCache_.end()) {
        auto& texture = it->second;
        
        // Check if texture is only referenced by the cache (use_count == 1)
        if (texture && texture->isLoaded && texture.use_count() == 1) {
            bytesFreed += texture->sizeBytes;
            texture->release();
            it = textureCache_.erase(it);
            g_textureLogger.Debug("Evicted unused texture: {}", it->first);
        } else {
            ++it;
        }
    }
    
    if (bytesFreed > 0) {
        g_textureLogger.Info("Texture eviction freed {:.1f} KB", bytesFreed / 1024.0);
    }
    
    return bytesFreed > 0;
}

TextureManager::Stats TextureManager::getStats() const {
    std::lock_guard<std::mutex> lock(textureMutex_);
    
    stats_.totalTextures = textureCache_.size();
    stats_.loadedTextures = 0;
    stats_.memoryUsage = 0;
    
    for (const auto& [name, texture] : textureCache_) {
        if (texture && texture->isLoaded) {
            stats_.loadedTextures++;
            stats_.memoryUsage += texture->sizeBytes;
        }
    }
    
    stats_.memoryBudget = maxTextureMemory_;
    stats_.cacheHitRatio = stats_.totalTextures > 0 ? 
        static_cast<double>(stats_.cachedTextures) / stats_.totalTextures : 0.0;
    
    return stats_;
}

void TextureManager::logStats() const {
    auto stats = getStats();
    
    g_textureLogger.Info("=== Texture Manager Statistics ===");
    g_textureLogger.Info("  Total textures: {}", stats.totalTextures);
    g_textureLogger.Info("  Loaded textures: {}", stats.loadedTextures);
    g_textureLogger.Info("  Memory usage: {:.1f} MB / {:.1f} MB ({:.1f}%)",
        stats.memoryUsage / (1024.0*1024.0),
        stats.memoryBudget / (1024.0*1024.0),
        (stats.memoryUsage / static_cast<double>(stats.memoryBudget)) * 100.0);
    g_textureLogger.Info("  Cache hit ratio: {:.1f}%", stats.cacheHitRatio * 100.0);
    g_textureLogger.Info("================================");
}

// Placeholder implementations
void TextureManager::loadTextureAsync(const std::string& name, std::shared_ptr<TextureAsset> texture) {
    // In a real implementation, this would:
    // 1. Load KTX2 file from disk
    // 2. Decompress BasisU data  
    // 3. Upload to GPU via staging buffer
    // 4. Create image view and sampler
    
    g_textureLogger.Debug("Loading texture '{}' asynchronously", name);
    
    // Simulate loading time
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    texture->width = 512;  // Default size
    texture->height = 512;
    texture->format = VK_FORMAT_R8G8B8A8_UNORM;
    texture->isLoading = false;
    texture->isLoaded = true;
    
    g_textureLogger.Debug("Texture '{}' loaded successfully", name);
}

VkSampler TextureManager::createTextureSampler(bool enableMipmaps, bool enableAnisotropy) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    
    if (enableAnisotropy) {
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 16.0f; // Maximum anisotropy
    } else {
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
    }
    
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    
    if (enableMipmaps) {
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.mipLodBias = 0.0f;
    } else {
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        samplerInfo.mipLodBias = 0.0f;
    }
    
    VkSampler sampler;
    VkResult result = vkCreateSampler(device_, &samplerInfo, nullptr, &sampler);
    
    if (result != VK_SUCCESS) {
        CHECK_VK_OBJECT(result, VkErrorCategory::RESOURCE_CREATION, "texture_sampler");
        return VK_NULL_HANDLE;
    }
    
    return sampler;
}

std::string TextureManager::getTexturePath(const std::string& name) const {
    // Try KTX2 first, then fallback to PNG
    std::string ktx2Path = textureDirectory_ + "/" + name + ".ktx2";
    if (std::filesystem::exists(ktx2Path)) {
        return ktx2Path;
    }
    
    std::string pngPath = textureDirectory_ + "/" + name + ".png";
    if (std::filesystem::exists(pngPath)) {
        return pngPath;
    }
    
    return "";
}

// KTX2Loader placeholder implementation
KTX2Loader::LoadResult KTX2Loader::loadFromFile(const std::string& filepath) {
    LoadResult result;
    
    if (!std::filesystem::exists(filepath)) {
        result.errorMessage = "File does not exist: " + filepath;
        return result;
    }
    
    g_textureLogger.Debug("Loading KTX2 file: {}", filepath);
    
    // In a real implementation, this would:
    // 1. Parse KTX2 header
    // 2. Decompress BasisU supercompressed data
    // 3. Transcode to target GPU format
    // 4. Generate mipmaps if needed
    
    // Placeholder implementation
    result.success = true;
    result.width = 512;
    result.height = 512;
    result.mipLevels = 1;
    result.format = VK_FORMAT_R8G8B8A8_UNORM;
    result.textureData.resize(512 * 512 * 4); // Dummy data
    
    g_textureLogger.Debug("KTX2 file loaded: {}x{}, {} bytes", 
        result.width, result.height, result.textureData.size());
    
    return result;
}

// Placeholder transcode from PNG using dummy data to enable build-time pipeline
KTX2Loader::TranscodeResult KTX2Loader::transcodeFromPNG(const std::string& pngPath, const TextureCompressionSettings& settings) {
    TranscodeResult out;
    out.targetFormat = settings.targetFormat;
    try {
        if (!std::filesystem::exists(pngPath)) {
            out.success = false;
            out.errorMessage = "PNG not found: " + pngPath;
            return out;
        }
        // Simulate transcoding by reading file size and generating compressed data at ~25%
        size_t fileSize = std::filesystem::file_size(pngPath);
        size_t compressedSize = std::max<size_t>(fileSize / 4, 1024);
        out.compressedData.resize(compressedSize, 0);
        out.width = 512;
        out.height = 512;
        out.mipLevels = settings.generateMipmaps ? 1u : 1u;
        out.success = true;
        return out;
    } catch (const std::exception& e) {
        out.success = false;
        out.errorMessage = e.what();
        return out;
    }
}

bool KTX2Loader::writeKTX2File(const TranscodeResult& result, const std::string& outputPath) {
    try {
        std::filesystem::create_directories(std::filesystem::path(outputPath).parent_path());
        std::ofstream ofs(outputPath, std::ios::binary);
        if (!ofs) return false;
        ofs.write(result.compressedData.data(), static_cast<std::streamsize>(result.compressedData.size()));
        return ofs.good();
    } catch (...) {
        return false;
    }
}

// Build-time texture processing
bool TextureBuildPipeline::processTextureDirectory(
    const std::string& sourceDir,
    const std::string& outputDir, 
    const TextureCompressionSettings& settings) {
    
    g_textureLogger.Info("Processing texture directory: {} -> {}", sourceDir, outputDir);
    
    if (!createOutputDirectory(outputDir)) {
        return false;
    }
    
    // Find all PNG files in source directory
    std::vector<std::string> pngFiles;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(sourceDir)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                
                if (extension == ".png" || extension == ".jpg" || extension == ".jpeg") {
                    pngFiles.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        g_textureLogger.Error("Failed to scan texture directory: {}", e.what());
        return false;
    }
    
    g_textureLogger.Info("Found {} texture files to process", pngFiles.size());
    
    // Process files in parallel
    return processTextureList(pngFiles, outputDir, settings);
}

bool TextureBuildPipeline::processTexture(
    const std::string& sourcePath,
    const std::string& outputPath,
    const TextureCompressionSettings& settings) {
    
    g_textureLogger.Debug("Processing texture: {} -> {}", sourcePath, outputPath);
    
    if (!validateInputTexture(sourcePath)) {
        return false;
    }
    
    // Transcode PNG to KTX2 with BasisU compression
    auto transcodeResult = KTX2Loader::transcodeFromPNG(sourcePath, settings);
    
    if (!transcodeResult.success) {
        g_textureLogger.Error("Failed to transcode {}: {}", sourcePath, transcodeResult.errorMessage);
        return false;
    }
    
    // Write KTX2 file
    if (!KTX2Loader::writeKTX2File(transcodeResult, outputPath)) {
        g_textureLogger.Error("Failed to write KTX2 file: {}", outputPath);
        return false;
    }
    
    // Calculate compression ratio
    size_t originalSize = std::filesystem::file_size(sourcePath);
    size_t compressedSize = transcodeResult.compressedData.size();
    float compressionRatio = static_cast<float>(originalSize) / compressedSize;
    
    g_textureLogger.Info("Texture processed: {:.1f}x compression ({} -> {} bytes)",
        compressionRatio, originalSize, compressedSize);
    
    return true;
}

bool TextureBuildPipeline::validateInputTexture(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        g_textureLogger.Error("Input texture does not exist: {}", path);
        return false;
    }
    
    auto fileSize = std::filesystem::file_size(path);
    if (fileSize == 0) {
        g_textureLogger.Error("Input texture is empty: {}", path);
        return false;
    }
    
    if (fileSize > 100 * 1024 * 1024) { // 100MB limit
        g_textureLogger.Warn("Large input texture ({:.1f} MB): {}", fileSize / (1024.0*1024.0), path);
    }
    
    return true;
}

bool TextureBuildPipeline::createOutputDirectory(const std::string& dir) {
    try {
        std::filesystem::create_directories(dir);
        return true;
    } catch (const std::exception& e) {
        g_textureLogger.Error("Failed to create output directory {}: {}", dir, e.what());
        return false;
    }
}

bool TextureBuildPipeline::processTextureList(
    const std::vector<std::string>& sourcePaths,
    const std::string& outputDir,
    const TextureCompressionSettings& settings,
    int numThreads) {
    if (sourcePaths.empty()) return true;
    numThreads = std::max(1, numThreads);

    std::atomic<bool> allOk{true};
    std::mutex idxMutex;
    size_t index = 0;

    auto worker = [&]() {
        while (true) {
            std::string src;
            {
                std::lock_guard<std::mutex> lock(idxMutex);
                if (index >= sourcePaths.size()) break;
                src = sourcePaths[index++];
            }
            std::string filename = std::filesystem::path(src).stem().string() + ".ktx2";
            std::string dst = std::filesystem::path(outputDir) / filename;
            bool ok = TextureBuildPipeline::processTexture(src, dst, settings);
            if (!ok) allOk.store(false);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(numThreads));
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back(worker);
    }
    for (auto& th : threads) th.join();
    return allOk.load();
}

} // namespace voxelvk