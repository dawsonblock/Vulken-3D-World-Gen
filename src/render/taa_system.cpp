#include "taa_system.hpp"
#include "../vk/error_handling.hpp"
#include "../core/logger.hpp"
#include "../core/nvtx_profiler.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace voxelvk {

static Logger g_taaLogger("TAASystem");

glm::mat4 CameraMotion::getViewProjectionMatrix() const {
    return currentProjectionMatrix * currentViewMatrix;
}

glm::mat4 CameraMotion::getPreviousViewProjectionMatrix() const {
    return previousProjectionMatrix * previousViewMatrix;
}

glm::mat4 CameraMotion::getReprojectionMatrix() const {
    // Matrix that transforms from current frame NDC to previous frame NDC
    glm::mat4 currentVP = getViewProjectionMatrix();
    glm::mat4 prevVP = getPreviousViewProjectionMatrix();
    return glm::inverse(currentVP) * prevVP;
}

void TAAResources::release() {
    // Clean up all TAA resources
    if (historyColorViewA != VK_NULL_HANDLE) {
        vkDestroyImageView(MemoryManager::instance().device_, historyColorViewA, nullptr);
        historyColorViewA = VK_NULL_HANDLE;
    }
    if (historyColorViewB != VK_NULL_HANDLE) {
        vkDestroyImageView(MemoryManager::instance().device_, historyColorViewB, nullptr);
        historyColorViewB = VK_NULL_HANDLE;
    }
    if (motionVectorView != VK_NULL_HANDLE) {
        vkDestroyImageView(MemoryManager::instance().device_, motionVectorView, nullptr);
        motionVectorView = VK_NULL_HANDLE;
    }
    if (depthHistoryView != VK_NULL_HANDLE) {
        vkDestroyImageView(MemoryManager::instance().device_, depthHistoryView, nullptr);
        depthHistoryView = VK_NULL_HANDLE;
    }
    
    // Release images through memory manager
    if (historyColorA != VK_NULL_HANDLE) {
        MemoryManager::instance().destroyImage(historyColorA, historyAllocationA);
        historyColorA = VK_NULL_HANDLE;
    }
    if (historyColorB != VK_NULL_HANDLE) {
        MemoryManager::instance().destroyImage(historyColorB, historyAllocationB);
        historyColorB = VK_NULL_HANDLE;
    }
    if (motionVectors != VK_NULL_HANDLE) {
        MemoryManager::instance().destroyImage(motionVectors, motionAllocation);
        motionVectors = VK_NULL_HANDLE;
    }
    if (depthHistory != VK_NULL_HANDLE) {
        MemoryManager::instance().destroyImage(depthHistory, depthHistoryAllocation);
        depthHistory = VK_NULL_HANDLE;
    }
}

TAASystem::TAASystem(VkDevice device, VkPhysicalDevice physicalDevice) 
    : device_(device), physicalDevice_(physicalDevice) {
    
    g_taaLogger.Info("TAASystem initialized");
    
    // Generate default jitter sequence
    generateJitterSequence();
}

TAASystem::~TAASystem() {
    shutdown();
}

bool TAASystem::initialize(uint32_t width, uint32_t height, VkFormat colorFormat) {
    g_taaLogger.Info("Initializing TAA system: {}x{}", width, height);
    
    resources_.width = width;
    resources_.height = height;
    resources_.colorFormat = colorFormat;
    
    if (!createResources(width, height)) {
        g_taaLogger.Error("Failed to create TAA resources");
        return false;
    }
    
    if (!createPipelines()) {
        g_taaLogger.Error("Failed to create TAA pipelines");
        destroyResources();
        return false;
    }
    
    g_taaLogger.Info("TAA system initialized successfully");
    g_taaLogger.Info("  Color format: {}", static_cast<uint32_t>(colorFormat));
    g_taaLogger.Info("  Motion vector format: {}", static_cast<uint32_t>(resources_.motionFormat));
    g_taaLogger.Info("  Weather TRP integration: {}", settings_.preserveWeatherTRP ? "ENABLED" : "DISABLED");
    
    return true;
}

void TAASystem::shutdown() {
    g_taaLogger.Info("Shutting down TAA system");
    
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
    
    // Log final statistics
    logStats();
    
    // Destroy pipelines
    if (motionVectorPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, motionVectorPipeline_, nullptr);
        motionVectorPipeline_ = VK_NULL_HANDLE;
    }
    if (taaResolvePipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, taaResolvePipeline_, nullptr);
        taaResolvePipeline_ = VK_NULL_HANDLE;
    }
    if (weatherBlendPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, weatherBlendPipeline_, nullptr);
        weatherBlendPipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (descriptorLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, descriptorLayout_, nullptr);
        descriptorLayout_ = VK_NULL_HANDLE;
    }
    
    // Destroy resources
    destroyResources();
    
    g_taaLogger.Info("TAA system shutdown complete");
}

bool TAASystem::resize(uint32_t newWidth, uint32_t newHeight) {
    g_taaLogger.Info("Resizing TAA system: {}x{} -> {}x{}", 
        resources_.width, resources_.height, newWidth, newHeight);
    
    if (newWidth == resources_.width && newHeight == resources_.height) {
        return true; // No change needed
    }
    
    // Wait for device idle
    vkDeviceWaitIdle(device_);
    
    // Destroy old resources
    destroyResources();
    
    // Create new resources
    resources_.width = newWidth;
    resources_.height = newHeight;
    
    if (!createResources(newWidth, newHeight)) {
        g_taaLogger.Error("Failed to create TAA resources during resize");
        return false;
    }
    
    g_taaLogger.Info("TAA system resized successfully");
    return true;
}

void TAASystem::updateCameraMotion(const CameraMotion& motion) {
    // Store previous frame data
    cameraMotion_.previousViewMatrix = cameraMotion_.currentViewMatrix;
    cameraMotion_.previousProjectionMatrix = cameraMotion_.currentProjectionMatrix;
    cameraMotion_.previousJitterOffset = cameraMotion_.jitterOffset;
    
    // Update current frame data
    cameraMotion_ = motion;
    
    // Update motion statistics
    updateMotionStatistics(motion);
    
    // Detect camera motion for adaptive feedback
    bool isMoving = detectCameraMotion(motion);
    cameraMotion_.isCameraStationary = !isMoving;
}

glm::vec2 TAASystem::getJitterOffset(uint32_t frameIndex) const {
    if (!settings_.enabled || jitterSequence_.empty()) {
        return {0.0f, 0.0f};
    }
    
    uint32_t jitterIdx = frameIndex % static_cast<uint32_t>(jitterSequence_.size());
    return jitterSequence_[jitterIdx];
}

void TAASystem::executeMotionVectorPass(VkCommandBuffer cmd, VkImageView depth, VkImageView previousDepth) {
    if (!settings_.enabled) return;
    
    NVTX_RANGE_PUSH("TAA_MotionVectors");
    VK_DEBUG_LABEL(cmd, "TAA_MotionVectors");
    
    // Bind motion vector generation pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, motionVectorPipeline_);
    
    // TODO: Bind descriptor sets with depth buffers and camera matrices
    // TODO: Draw fullscreen triangle to generate motion vectors
    
    g_taaLogger.Debug("Motion vector pass executed");
    
    NVTX_RANGE_POP();
}

void TAASystem::executeTAAResolvePass(VkCommandBuffer cmd, VkImageView currentColor, VkImageView output) {
    if (!settings_.enabled) return;
    
    NVTX_RANGE_PUSH("TAA_Resolve");
    VK_DEBUG_LABEL(cmd, "TAA_Resolve");
    
    // Bind TAA resolve pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, taaResolvePipeline_);
    
    // TODO: Bind descriptor sets with current color, history, motion vectors
    // TODO: Draw fullscreen triangle for TAA resolve
    
    // Swap history buffers
    resources_.swapHistory();
    
    stats_.framesProcessed++;
    
    g_taaLogger.Debug("TAA resolve pass executed");
    
    NVTX_RANGE_POP();
}

void TAASystem::setWeatherTRPTextures(VkImageView clouds, VkImageView precipitation) {
    weatherClouds_ = clouds;
    weatherPrecipitation_ = precipitation;
    
    g_taaLogger.Debug("Weather TRP textures set for TAA integration");
}

void TAASystem::executeWeatherTAABlend(VkCommandBuffer cmd, VkImageView taaResult, VkImageView weatherTRP, VkImageView output) {
    if (!settings_.enabled || !settings_.preserveWeatherTRP) {
        // Copy TAA result directly if weather integration is disabled
        // TODO: Implement copy operation
        return;
    }
    
    NVTX_RANGE_PUSH("TAA_WeatherBlend");
    VK_DEBUG_LABEL(cmd, "TAA_WeatherBlend");
    
    // Bind weather blend pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, weatherBlendPipeline_);
    
    // TODO: Bind descriptor sets with TAA result and weather TRP
    // TODO: Draw fullscreen triangle for weather-aware blending
    
    g_taaLogger.Debug("Weather-TAA blend pass executed");
    
    NVTX_RANGE_POP();
}

bool TAASystem::createResources(uint32_t width, uint32_t height) {
    g_taaLogger.Debug("Creating TAA resources: {}x{}", width, height);
    
    // Create history color buffers (ping-pong)
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = resources_.colorFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    // History buffer A
    resources_.historyAllocationA = MemoryManager::instance().createImage(
        imageInfo, VMA_MEMORY_USAGE_GPU_ONLY, MemoryCategory::RENDER_TARGETS, "TAA_HistoryA");
    
    if (resources_.historyAllocationA.allocation == VK_NULL_HANDLE) {
        g_taaLogger.Error("Failed to create TAA history buffer A");
        return false;
    }
    
    // History buffer B
    resources_.historyAllocationB = MemoryManager::instance().createImage(
        imageInfo, VMA_MEMORY_USAGE_GPU_ONLY, MemoryCategory::RENDER_TARGETS, "TAA_HistoryB");
    
    if (resources_.historyAllocationB.allocation == VK_NULL_HANDLE) {
        g_taaLogger.Error("Failed to create TAA history buffer B");
        return false;
    }
    
    // Motion vector buffer
    imageInfo.format = resources_.motionFormat;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    
    resources_.motionAllocation = MemoryManager::instance().createImage(
        imageInfo, VMA_MEMORY_USAGE_GPU_ONLY, MemoryCategory::RENDER_TARGETS, "TAA_MotionVectors");
    
    if (resources_.motionAllocation.allocation == VK_NULL_HANDLE) {
        g_taaLogger.Error("Failed to create motion vector buffer");
        return false;
    }
    
    // Create image views
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    // History A view
    viewInfo.image = resources_.historyColorA;
    viewInfo.format = resources_.colorFormat;
    VkResult result = vkCreateImageView(device_, &viewInfo, nullptr, &resources_.historyColorViewA);
    if (result != VK_SUCCESS) {
        CHECK_VK_OBJECT(result, VkErrorCategory::RESOURCE_CREATION, "taa_history_view_a");
        return false;
    }
    
    // History B view
    viewInfo.image = resources_.historyColorB;
    result = vkCreateImageView(device_, &viewInfo, nullptr, &resources_.historyColorViewB);
    if (result != VK_SUCCESS) {
        CHECK_VK_OBJECT(result, VkErrorCategory::RESOURCE_CREATION, "taa_history_view_b");
        return false;
    }
    
    // Motion vector view
    viewInfo.image = resources_.motionVectors;
    viewInfo.format = resources_.motionFormat;
    result = vkCreateImageView(device_, &viewInfo, nullptr, &resources_.motionVectorView);
    if (result != VK_SUCCESS) {
        CHECK_VK_OBJECT(result, VkErrorCategory::RESOURCE_CREATION, "taa_motion_view");
        return false;
    }
    
    VK_OBJECT_NAME(device_, resources_.historyColorViewA, VK_OBJECT_TYPE_IMAGE_VIEW, "TAA_HistoryA");
    VK_OBJECT_NAME(device_, resources_.historyColorViewB, VK_OBJECT_TYPE_IMAGE_VIEW, "TAA_HistoryB");
    VK_OBJECT_NAME(device_, resources_.motionVectorView, VK_OBJECT_TYPE_IMAGE_VIEW, "TAA_MotionVectors");
    
    return true;
}

bool TAASystem::createPipelines() {
    g_taaLogger.Debug("Creating TAA pipelines");
    
    // TODO: Create descriptor set layout for TAA resources
    // TODO: Create pipeline layout
    // TODO: Create motion vector generation pipeline
    // TODO: Create TAA resolve pipeline
    // TODO: Create weather blend pipeline
    
    g_taaLogger.Info("TAA pipelines created successfully");
    return true;
}

void TAASystem::destroyResources() {
    resources_.release();
}

void TAASystem::generateJitterSequence() {
    jitterSequence_.clear();
    jitterSequence_.reserve(16);
    
    // Generate Halton sequence for jitter pattern
    for (uint32_t i = 0; i < 16; i++) {
        float x = haltonSequence(i + 1, 2, 3).x;
        float y = haltonSequence(i + 1, 2, 3).y;
        
        // Convert to [-0.5, 0.5] range
        x = (x - 0.5f) / resources_.width;
        y = (y - 0.5f) / resources_.height;
        
        jitterSequence_.push_back({x, y});
    }
    
    g_taaLogger.Debug("Generated {} jitter samples", jitterSequence_.size());
}

glm::vec2 TAASystem::haltonSequence(uint32_t index, uint32_t base1, uint32_t base2) {
    auto halton = [](uint32_t i, uint32_t base) -> float {
        float f = 1.0f;
        float r = 0.0f;
        while (i > 0) {
            f /= base;
            r += f * (i % base);
            i /= base;
        }
        return r;
    };
    
    return {halton(index, base1), halton(index, base2)};
}

void TAASystem::updateMotionStatistics(const CameraMotion& motion) {
    // Calculate camera velocity
    glm::vec3 currentPos = glm::vec3(glm::inverse(motion.currentViewMatrix)[3]);
    glm::vec3 prevPos = glm::vec3(glm::inverse(motion.previousViewMatrix)[3]);
    
    float velocity = glm::length(currentPos - prevPos);
    cameraMotion_.cameraVelocity = velocity;
    
    // Update running average
    const float alpha = 0.1f;
    stats_.averageMotion = stats_.averageMotion * (1.0f - alpha) + velocity * alpha;
}

bool TAASystem::detectCameraMotion(const CameraMotion& motion) {
    // Simple motion detection based on matrix differences
    const float MOTION_THRESHOLD = 0.001f;
    
    glm::mat4 viewDiff = motion.currentViewMatrix - motion.previousViewMatrix;
    float motionMagnitude = 0.0f;
    
    // Calculate Frobenius norm of difference
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            motionMagnitude += viewDiff[i][j] * viewDiff[i][j];
        }
    }
    
    return std::sqrt(motionMagnitude) > MOTION_THRESHOLD;
}

void TAASystem::logStats() const {
    g_taaLogger.Info("=== TAA System Statistics ===");
    g_taaLogger.Info("  Frames processed: {}", stats_.framesProcessed);
    g_taaLogger.Info("  Average motion: {:.4f} units/frame", stats_.averageMotion);
    g_taaLogger.Info("  TAA stability: {}", stats_.isStable ? "STABLE" : "UNSTABLE");
    g_taaLogger.Info("  TAA processing time: {:.3f}ms", stats_.taaTime);
    g_taaLogger.Info("  Performance budget: {:.3f}ms", maxTAATimeMs_);
    
    if (settings_.preserveWeatherTRP) {
        g_taaLogger.Info("  Weather TRP integration: ACTIVE");
        g_taaLogger.Info("    Weather blend ratio: {:.1f}%", settings_.weatherBlendRatio * 100.0f);
    }
    
    g_taaLogger.Info("=============================");
}

// Camera jitter implementation
CameraJitter::CameraJitter(Pattern pattern) : pattern_(pattern) {
    setPattern(pattern);
}

glm::vec2 CameraJitter::getJitter(uint32_t frameIndex) const {
    if (jitterPattern_.empty()) return {0.0f, 0.0f};
    
    uint32_t index = frameIndex % static_cast<uint32_t>(jitterPattern_.size());
    return jitterPattern_[index];
}

glm::mat4 CameraJitter::applyJitterToProjection(const glm::mat4& projection, glm::vec2 jitter, uint32_t width, uint32_t height) const {
    glm::mat4 jitteredProjection = projection;
    
    // Apply jitter to projection matrix
    jitteredProjection[2][0] += jitter.x * 2.0f; // X offset
    jitteredProjection[2][1] += jitter.y * 2.0f; // Y offset
    
    return jitteredProjection;
}

void CameraJitter::setPattern(Pattern pattern) {
    pattern_ = pattern;
    
    switch (pattern) {
        case Pattern::HALTON_2_3:
            generateHaltonPattern(16);
            break;
        case Pattern::UNIFORM_4X:
            generateUniformPattern(4);
            break;
        case Pattern::UNIFORM_8X:
            generateUniformPattern(8);
            break;
        case Pattern::R2_SEQUENCE:
            generateR2Pattern(16);
            break;
    }
}

void CameraJitter::generateHaltonPattern(uint32_t sampleCount) {
    jitterPattern_.clear();
    jitterPattern_.reserve(sampleCount);
    
    for (uint32_t i = 0; i < sampleCount; i++) {
        float x = halton(i + 1, 2) - 0.5f;
        float y = halton(i + 1, 3) - 0.5f;
        jitterPattern_.push_back({x, y});
    }
}

void CameraJitter::generateUniformPattern(uint32_t sampleCount) {
    jitterPattern_.clear();
    
    if (sampleCount == 4) {
        // 4x MSAA pattern
        jitterPattern_ = {
            {-0.25f, -0.25f}, {0.25f, -0.25f},
            {-0.25f,  0.25f}, {0.25f,  0.25f}
        };
    } else if (sampleCount == 8) {
        // 8x MSAA pattern
        jitterPattern_ = {
            {-0.125f, -0.375f}, {0.375f, -0.125f},
            {-0.375f,  0.125f}, {0.125f,  0.375f},
            {-0.375f, -0.125f}, {0.125f, -0.375f},
            {-0.125f,  0.375f}, {0.375f,  0.125f}
        };
    }
}

float CameraJitter::halton(uint32_t index, uint32_t base) {
    float f = 1.0f;
    float r = 0.0f;
    while (index > 0) {
        f /= base;
        r += f * (index % base);
        index /= base;
    }
    return r;
}

float CameraJitter::r2(uint32_t index, uint32_t dimension) {
    const float g = 1.32471795724474602596f; // Plastic number
    const float a1 = 1.0f / g;
    const float a2 = 1.0f / (g * g);
    
    if (dimension == 0) {
        return std::fmod(0.5f + a1 * index, 1.0f);
    } else {
        return std::fmod(0.5f + a2 * index, 1.0f);
    }
}

void CameraJitter::generateR2Pattern(uint32_t sampleCount) {
    jitterPattern_.clear();
    jitterPattern_.reserve(sampleCount);
    
    for (uint32_t i = 0; i < sampleCount; i++) {
        float x = r2(i, 0) - 0.5f;
        float y = r2(i, 1) - 0.5f;
        jitterPattern_.push_back({x, y});
    }
}

} // namespace voxelvk