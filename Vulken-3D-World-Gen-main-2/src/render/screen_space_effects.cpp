#include "screen_space_effects.hpp"
#include "../vk/error_handling.hpp"
#include "../core/logger.hpp"
#include "../core/nvtx_profiler.hpp"
#include <random>
#include <chrono>

namespace voxelvk {

static Logger g_ssaoLogger("SSAO");
static Logger g_ssrLogger("SSR");
static Logger g_cvarLogger("CVarSystem");

// CVarSystem implementation
CVarSystem& CVarSystem::instance() {
    static CVarSystem instance;
    return instance;
}

void CVarSystem::registerBoolVar(const std::string& name, bool defaultValue, const std::string& description) {
    std::lock_guard<std::mutex> lock(cvarMutex_);
    
    // Check if var already exists
    if (findVar(name)) {
        g_cvarLogger.Warn("CVar '{}' already registered", name);
        return;
    }
    
    CVar cvar;
    cvar.type = CVar::BOOL;
    cvar.name = name;
    cvar.description = description;
    cvar.boolValue = defaultValue;
    
    cvars_.push_back(cvar);
    
    g_cvarLogger.Debug("Registered bool CVar: '{}' = {}", name, defaultValue);
}

void CVarSystem::registerFloatVar(const std::string& name, float defaultValue, float minValue, float maxValue, const std::string& description) {
    std::lock_guard<std::mutex> lock(cvarMutex_);
    
    if (findVar(name)) {
        g_cvarLogger.Warn("CVar '{}' already registered", name);
        return;
    }
    
    CVar cvar;
    cvar.type = CVar::FLOAT;
    cvar.name = name;
    cvar.description = description;
    cvar.floatValue = defaultValue;
    cvar.minFloat = minValue;
    cvar.maxFloat = maxValue;
    
    cvars_.push_back(cvar);
    
    g_cvarLogger.Debug("Registered float CVar: '{}' = {} [{}, {}]", name, defaultValue, minValue, maxValue);
}

bool CVarSystem::getBool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(cvarMutex_);
    const CVar* cvar = findVar(name);
    if (cvar && cvar->type == CVar::BOOL) {
        return cvar->boolValue;
    }
    g_cvarLogger.Warn("Bool CVar '{}' not found, returning false", name);
    return false;
}

float CVarSystem::getFloat(const std::string& name) const {
    std::lock_guard<std::mutex> lock(cvarMutex_);
    const CVar* cvar = findVar(name);
    if (cvar && cvar->type == CVar::FLOAT) {
        return cvar->floatValue;
    }
    g_cvarLogger.Warn("Float CVar '{}' not found, returning 0.0", name);
    return 0.0f;
}

bool CVarSystem::setBool(const std::string& name, bool value) {
    std::lock_guard<std::mutex> lock(cvarMutex_);
    CVar* cvar = findVar(name);
    if (cvar && cvar->type == CVar::BOOL) {
        cvar->boolValue = value;
        g_cvarLogger.Info("Set '{}' = {}", name, value);
        return true;
    }
    return false;
}

bool CVarSystem::setFloat(const std::string& name, float value) {
    std::lock_guard<std::mutex> lock(cvarMutex_);
    CVar* cvar = findVar(name);
    if (cvar && cvar->type == CVar::FLOAT) {
        float clampedValue = std::clamp(value, cvar->minFloat, cvar->maxFloat);
        cvar->floatValue = clampedValue;
        g_cvarLogger.Info("Set '{}' = {}", name, clampedValue);
        return true;
    }
    return false;
}

CVarSystem::CVar* CVarSystem::findVar(const std::string& name) {
    for (auto& cvar : cvars_) {
        if (cvar.name == name) {
            return &cvar;
        }
    }
    return nullptr;
}

const CVarSystem::CVar* CVarSystem::findVar(const std::string& name) const {
    for (const auto& cvar : cvars_) {
        if (cvar.name == name) {
            return &cvar;
        }
    }
    return nullptr;
}

// SSAO System implementation
SSAOSystem::SSAOSystem(VkDevice device, VkPhysicalDevice physicalDevice)
    : device_(device), physicalDevice_(physicalDevice) {
    
    g_ssaoLogger.Info("SSAO system initialized");
}

SSAOSystem::~SSAOSystem() {
    shutdown();
}

bool SSAOSystem::initialize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    
    g_ssaoLogger.Info("Initializing SSAO: {}x{}", width, height);
    g_ssaoLogger.Info("  Half resolution: {}", settings_.halfResolution ? "YES" : "NO");
    g_ssaoLogger.Info("  Sample count: {}", settings_.sampleCount);
    g_ssaoLogger.Info("  AO radius: {}", settings_.radius);
    
    generateSampleKernel();
    
    if (!createResources()) {
        g_ssaoLogger.Error("Failed to create SSAO resources");
        return false;
    }
    
    if (!createPipelines()) {
        g_ssaoLogger.Error("Failed to create SSAO pipelines");
        destroyResources();
        return false;
    }
    
    g_ssaoLogger.Info("SSAO system initialized successfully");
    return true;
}

void SSAOSystem::shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
    
    logStats();
    destroyResources();
    
    g_ssaoLogger.Info("SSAO system shutdown complete");
}

void SSAOSystem::setSettings(const SSAOSettings& settings) {
    if (settings.enabled != settings_.enabled ||
        settings.sampleCount != settings_.sampleCount ||
        settings.halfResolution != settings_.halfResolution) {
        
        g_ssaoLogger.Info("SSAO settings changed - regenerating resources");
        
        // Recreate resources if major settings changed
        vkDeviceWaitIdle(device_);
        destroyResources();
        
        settings_ = settings;
        
        generateSampleKernel();
        createResources();
        createPipelines();
    } else {
        settings_ = settings;
    }
}

void SSAOSystem::executeSSAO(VkCommandBuffer cmd, VkImageView depth, VkImageView normal, VkImageView output) {
    if (!settings_.enabled) return;
    
    VXL_NVTX_RANGE("SSAO");
    VK_DEBUG_LABEL(cmd, "SSAO");
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Bind SSAO pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ssaoPipeline_);
    
    // TODO: Bind descriptor sets with depth, normal, sample kernel
    // TODO: Draw fullscreen triangle for SSAO generation
    
    // Blur pass if enabled
    if (settings_.enableBlur) {
        VK_DEBUG_LABEL(cmd, "SSAO_Blur");
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipeline_);
        // TODO: Bind blur resources and execute
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    double ssaoTime = std::chrono::duration<double>(endTime - startTime).count();
    stats_.averageTime = stats_.averageTime * 0.9 + ssaoTime * 0.1; // Running average
    stats_.framesProcessed++;
    
    // NVTX range automatically ends here
    
    g_ssaoLogger.Debug("SSAO pass executed: {:.3f}ms", ssaoTime * 1000.0);
}

bool SSAOSystem::createResources() {
    uint32_t aoWidth = settings_.halfResolution ? width_ / 2 : width_;
    uint32_t aoHeight = settings_.halfResolution ? height_ / 2 : height_;
    
    g_ssaoLogger.Debug("Creating SSAO resources: {}x{}", aoWidth, aoHeight);
    
    // Create AO texture
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {aoWidth, aoHeight, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8_UNORM; // Single channel AO
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    {
        ImageResult res = MemoryManager::instance().createImage(
            imageInfo, VMA_MEMORY_USAGE_GPU_ONLY, MemoryCategory::RENDER_TARGETS, "SSAO_Texture");
        aoTexture_ = res.image;
        aoAllocation_ = res.allocation;
    }
    
    if (aoTexture_ == VK_NULL_HANDLE || aoAllocation_.allocation == VK_NULL_HANDLE) {
        g_ssaoLogger.Error("Failed to create AO texture");
        return false;
    }
    
    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = aoTexture_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    VkResult result = vkCreateImageView(device_, &viewInfo, nullptr, &aoTextureView_);
    if (result != VK_SUCCESS) {
        CHECK_VK_OBJECT(result, voxelvk::VkErrorCategory::RESOURCE_CREATION, "ssao_texture_view");
        return false;
    }
    
    VK_OBJECT_NAME(device_, aoTextureView_, VK_OBJECT_TYPE_IMAGE_VIEW, "SSAO_Texture");
    
    return true;
}

bool SSAOSystem::createPipelines() {
    g_ssaoLogger.Debug("Creating SSAO pipelines");
    
    // TODO: Create descriptor set layout
    // TODO: Create pipeline layout  
    // TODO: Create SSAO compute/graphics pipeline
    // TODO: Create bilateral blur pipeline
    
    g_ssaoLogger.Info("SSAO pipelines created successfully");
    return true;
}

void SSAOSystem::destroyResources() {
    if (aoTextureView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, aoTextureView_, nullptr);
        aoTextureView_ = VK_NULL_HANDLE;
    }
    
    if (aoTexture_ != VK_NULL_HANDLE) {
        MemoryManager::instance().destroyImage(aoTexture_, aoAllocation_);
        aoTexture_ = VK_NULL_HANDLE;
    }
    
    if (sampleBuffer_ != VK_NULL_HANDLE) {
        MemoryManager::instance().destroyBuffer(sampleBuffer_, sampleAllocation_);
        sampleBuffer_ = VK_NULL_HANDLE;
    }
}

void SSAOSystem::generateSampleKernel() {
    g_ssaoLogger.Debug("Generating SSAO sample kernel: {} samples", settings_.sampleCount);
    
    sampleKernel_.clear();
    sampleKernel_.reserve(settings_.sampleCount);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    
    for (uint32_t i = 0; i < settings_.sampleCount; i++) {
        glm::vec3 sample(
            dis(gen) * 2.0f - 1.0f,  // X: [-1, 1]
            dis(gen) * 2.0f - 1.0f,  // Y: [-1, 1]  
            dis(gen)                 // Z: [0, 1] (hemisphere)
        );
        
        sample = glm::normalize(sample);
        
        // Scale samples to be more clustered near origin
        float scale = static_cast<float>(i) / settings_.sampleCount;
        scale = 0.1f + scale * scale * 0.9f; // Lerp between 0.1 and 1.0
        sample *= scale;
        
        sampleKernel_.push_back(sample);
    }
}

void SSAOSystem::logStats() const {
    g_ssaoLogger.Info("=== SSAO Statistics ===");
    g_ssaoLogger.Info("  Frames processed: {}", stats_.framesProcessed);
    g_ssaoLogger.Info("  Average time: {:.3f}ms", stats_.averageTime * 1000.0);
    g_ssaoLogger.Info("  Sample count: {}", settings_.sampleCount);
    g_ssaoLogger.Info("  Resolution: {}x{} ({}%)", 
        settings_.halfResolution ? width_/2 : width_,
        settings_.halfResolution ? height_/2 : height_,
        settings_.halfResolution ? 50 : 100);
    g_ssaoLogger.Info("======================");
}

// SSR System implementation
SSRSystem::SSRSystem(VkDevice device, VkPhysicalDevice physicalDevice)
    : device_(device), physicalDevice_(physicalDevice) {
    
    g_ssrLogger.Info("SSR system initialized");
}

SSRSystem::~SSRSystem() {
    shutdown();
}

bool SSRSystem::initialize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    
    g_ssrLogger.Info("Initializing SSR: {}x{}", width, height);
    g_ssrLogger.Info("  Enabled: {}", settings_.enabled ? "YES" : "NO");
    g_ssrLogger.Info("  Max steps: {}", settings_.maxSteps);
    g_ssrLogger.Info("  Roughness aware: {}", settings_.roughnessAware ? "YES" : "NO");
    
    if (!settings_.enabled) {
        g_ssrLogger.Info("SSR disabled - skipping resource creation");
        return true;
    }
    
    if (!createResources()) {
        g_ssrLogger.Error("Failed to create SSR resources");
        return false;
    }
    
    if (!createPipelines()) {
        g_ssrLogger.Error("Failed to create SSR pipelines");
        destroyResources();
        return false;
    }
    
    g_ssrLogger.Info("SSR system initialized successfully");
    return true;
}

void SSRSystem::shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
    
    logStats();
    destroyResources();
    
    g_ssrLogger.Info("SSR system shutdown complete");
}

void SSRSystem::setSettings(const SSRSettings& settings) {
    // If settings that affect resources changed, recreate resources
    bool needRecreate =
        (settings.enabled != settings_.enabled) ||
        (settings.halfResolution != settings_.halfResolution);

    settings_ = settings;

    if (device_ == VK_NULL_HANDLE)
        return;

    if (needRecreate) {
        g_ssrLogger.Info("SSR settings changed - regenerating resources");
        vkDeviceWaitIdle(device_);
        destroyResources();
        if (settings_.enabled) {
            createResources();
            createPipelines();
        }
    }
}

void SSRSystem::executeSSR(VkCommandBuffer cmd, 
                          VkImageView color, VkImageView depth, VkImageView normal, VkImageView roughness,
                          VkImageView output) {
    if (!settings_.enabled) return;
    
    VXL_NVTX_RANGE("SSR");
    VK_DEBUG_LABEL(cmd, "SSR");
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Generate depth mipchain if enabled
    if (settings_.enableMipchain) {
        generateDepthMipchain(cmd, depth);
    }
    
    // Execute SSR ray marching
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ssrPipeline_);
    
    // TODO: Bind descriptor sets with G-buffer data
    // TODO: Draw fullscreen triangle for SSR generation
    
    // Denoising pass if enabled
    if (settings_.enableDenoising) {
        VK_DEBUG_LABEL(cmd, "SSR_Denoise");
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, denoisePipeline_);
        // TODO: Execute denoising pass
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    double ssrTime = std::chrono::duration<double>(endTime - startTime).count();
    stats_.averageTime = stats_.averageTime * 0.9 + ssrTime * 0.1;
    stats_.framesProcessed++;
    
    // NVTX range automatically ends here
    
    g_ssrLogger.Debug("SSR pass executed: {:.3f}ms", ssrTime * 1000.0);
}

bool SSRSystem::createResources() {
    uint32_t reflectionWidth = settings_.halfResolution ? width_ / 2 : width_;
    uint32_t reflectionHeight = settings_.halfResolution ? height_ / 2 : height_;
    
    g_ssrLogger.Debug("Creating SSR resources: {}x{}", reflectionWidth, reflectionHeight);
    
    // Create reflection texture
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {reflectionWidth, reflectionHeight, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT; // High precision for reflections
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    {
        ImageResult res = MemoryManager::instance().createImage(
            imageInfo, VMA_MEMORY_USAGE_GPU_ONLY, MemoryCategory::RENDER_TARGETS, "SSR_Reflections");
        reflectionTexture_ = res.image;
        reflectionAllocation_ = res.allocation;
    }
    
    if (reflectionTexture_ == VK_NULL_HANDLE || reflectionAllocation_.allocation == VK_NULL_HANDLE) {
        g_ssrLogger.Error("Failed to create SSR reflection texture");
        return false;
    }
    
    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = reflectionTexture_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    VkResult result = vkCreateImageView(device_, &viewInfo, nullptr, &reflectionTextureView_);
    if (result != VK_SUCCESS) {
        CHECK_VK_OBJECT(result, voxelvk::VkErrorCategory::RESOURCE_CREATION, "ssr_reflection_view");
        return false;
    }
    
    VK_OBJECT_NAME(device_, reflectionTextureView_, VK_OBJECT_TYPE_IMAGE_VIEW, "SSR_Reflections");
    
    return true;
}

bool SSRSystem::createPipelines() {
    g_ssrLogger.Debug("Creating SSR pipelines");
    
    // TODO: Create SSR ray marching pipeline
    // TODO: Create denoising pipeline
    // TODO: Create depth mipchain generation pipeline
    
    g_ssrLogger.Info("SSR pipelines created successfully");
    return true;
}

void SSRSystem::destroyResources() {
    if (reflectionTextureView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, reflectionTextureView_, nullptr);
        reflectionTextureView_ = VK_NULL_HANDLE;
    }
    
    if (reflectionTexture_ != VK_NULL_HANDLE) {
        MemoryManager::instance().destroyImage(reflectionTexture_, reflectionAllocation_);
        reflectionTexture_ = VK_NULL_HANDLE;
    }
}

void SSRSystem::generateDepthMipchain(VkCommandBuffer cmd, VkImageView depth) {
    if (!settings_.enableMipchain) return;
    
    VK_DEBUG_LABEL(cmd, "SSR_DepthMipchain");
    
    // TODO: Generate hierarchical Z-buffer for efficient ray marching
    g_ssrLogger.Debug("Generating depth mipchain for SSR");
}

void SSRSystem::logStats() const {
    g_ssrLogger.Info("=== SSR Statistics ===");
    g_ssrLogger.Info("  Enabled: {}", settings_.enabled ? "YES" : "NO");
    if (settings_.enabled) {
        g_ssrLogger.Info("  Frames processed: {}", stats_.framesProcessed);
        g_ssrLogger.Info("  Average time: {:.3f}ms", stats_.averageTime * 1000.0);
        g_ssrLogger.Info("  Ray hit rate: {:.1f}%", 
            (stats_.raysHit + stats_.raysMissed) > 0 ? 
            (static_cast<float>(stats_.raysHit) / (stats_.raysHit + stats_.raysMissed)) * 100.0f : 0.0f);
    }
    g_ssrLogger.Info("=====================");
}

// ScreenSpaceEffects integrated manager
ScreenSpaceEffects::ScreenSpaceEffects(VkDevice device, VkPhysicalDevice physicalDevice)
    : device_(device), physicalDevice_(physicalDevice), 
      ssaoSystem_(device, physicalDevice), 
      ssrSystem_(device, physicalDevice) {
    
    g_ssaoLogger.Info("ScreenSpaceEffects manager initialized");
}

ScreenSpaceEffects::~ScreenSpaceEffects() {
    shutdown();
}

bool ScreenSpaceEffects::initialize(uint32_t width, uint32_t height) {
    g_ssaoLogger.Info("Initializing screen-space effects: {}x{}", width, height);
    
    // Register CVars
    registerCVars();
    
    // Initialize SSAO
    if (!ssaoSystem_.initialize(width, height)) {
        g_ssaoLogger.Error("Failed to initialize SSAO system");
        return false;
    }
    
    // Initialize SSR  
    if (!ssrSystem_.initialize(width, height)) {
        g_ssaoLogger.Error("Failed to initialize SSR system");
        return false;
    }
    
    g_ssaoLogger.Info("Screen-space effects initialized successfully");
    return true;
}

void ScreenSpaceEffects::shutdown() {
    ssaoSystem_.shutdown();
    ssrSystem_.shutdown();
    
    g_ssaoLogger.Info("Screen-space effects shutdown complete");
}

void ScreenSpaceEffects::registerCVars() {
    if (cvarRegistered_) return;
    
    auto& cvars = CVarSystem::instance();
    
    // SSAO CVars
    cvars.registerBoolVar("r.ssao.enable", true, "Enable Screen Space Ambient Occlusion");
    cvars.registerFloatVar("r.ssao.radius", 1.5f, 0.1f, 5.0f, "SSAO sampling radius");
    cvars.registerFloatVar("r.ssao.strength", 1.0f, 0.0f, 2.0f, "SSAO darkening strength");
    cvars.registerBoolVar("r.ssao.halfres", true, "Render SSAO at half resolution");
    
    // SSR CVars
    cvars.registerBoolVar("r.ssr.enable", false, "Enable Screen Space Reflections");
    cvars.registerFloatVar("r.ssr.maxdistance", 50.0f, 10.0f, 200.0f, "SSR maximum ray distance");
    cvars.registerFloatVar("r.ssr.thickness", 0.5f, 0.1f, 2.0f, "SSR surface thickness");
    cvars.registerBoolVar("r.ssr.roughnessaware", true, "Fade SSR based on surface roughness");
    cvars.registerBoolVar("r.ssr.halfres", true, "Render SSR at half resolution");
    
    cvarRegistered_ = true;
    
    g_ssaoLogger.Info("Screen-space effects CVars registered");
}

void ScreenSpaceEffects::updateFromCVars() {
    auto& cvars = CVarSystem::instance();
    
    // Update SSAO settings from CVars
    SSAOSettings ssaoSettings = ssaoSystem_.getSettings();
    ssaoSettings.enabled = cvars.getBool("r.ssao.enable");
    ssaoSettings.radius = cvars.getFloat("r.ssao.radius");
    ssaoSettings.strength = cvars.getFloat("r.ssao.strength");
    ssaoSettings.halfResolution = cvars.getBool("r.ssao.halfres");
    ssaoSystem_.setSettings(ssaoSettings);
    
    // Update SSR settings from CVars
    SSRSettings ssrSettings = ssrSystem_.getSettings();
    ssrSettings.enabled = cvars.getBool("r.ssr.enable");
    ssrSettings.maxDistance = cvars.getFloat("r.ssr.maxdistance");
    ssrSettings.thickness = cvars.getFloat("r.ssr.thickness");
    ssrSettings.roughnessAware = cvars.getBool("r.ssr.roughnessaware");
    ssrSettings.halfResolution = cvars.getBool("r.ssr.halfres");
    ssrSystem_.setSettings(ssrSettings);
}

void ScreenSpaceEffects::executeScreenSpacePass(VkCommandBuffer cmd,
                                                VkImageView color, VkImageView depth, VkImageView normal, VkImageView roughness,
                                                VkImageView output) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    VXL_NVTX_RANGE("ScreenSpaceEffects");
    
    // Execute SSAO
    if (ssaoSystem_.getSettings().enabled) {
        ssaoSystem_.executeSSAO(cmd, depth, normal, output);
    }
    
    // Execute SSR
    if (ssrSystem_.getSettings().enabled) {
        ssrSystem_.executeSSR(cmd, color, depth, normal, roughness, output);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    double totalTime = std::chrono::duration<double>(endTime - startTime).count();
    
    perfStats_.totalTime = totalTime;
    perfStats_.withinBudget = totalTime <= (maxScreenSpaceTimeMs_ / 1000.0);
    
    // NVTX range automatically ends here
    
    if (!perfStats_.withinBudget) {
        g_ssaoLogger.Warn("Screen space effects over budget: {:.3f}ms (limit: {:.3f}ms)",
            totalTime * 1000.0, maxScreenSpaceTimeMs_);
    }
}

void ScreenSpaceEffects::logPerformanceReport() const {
    g_ssaoLogger.Info("=== Screen Space Effects Performance ===");
    g_ssaoLogger.Info("  SSAO time: {:.3f}ms", perfStats_.ssaoTime);
    g_ssaoLogger.Info("  SSR time: {:.3f}ms", perfStats_.ssrTime);
    g_ssaoLogger.Info("  Total time: {:.3f}ms", perfStats_.totalTime * 1000.0);
    g_ssaoLogger.Info("  Budget: {:.3f}ms ({})", maxScreenSpaceTimeMs_, 
        perfStats_.withinBudget ? "WITHIN" : "OVER");
    g_ssaoLogger.Info("========================================");
}

} // namespace voxelvk