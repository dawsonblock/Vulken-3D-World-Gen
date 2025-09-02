#pragma once
#include <vulkan/vulkan.h>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <vector>
#include "../vk/memory_manager.hpp"

namespace voxelvk {

/**
 * Temporal Anti-Aliasing system with camera motion vectors
 * Integrates with weather TRP to preserve atmospheric stability
 */
struct TAASettings {
    bool enabled = true;
    float feedbackMin = 0.88f;        // Minimum temporal feedback (0.88 = 88% history)
    float feedbackMax = 0.97f;        // Maximum temporal feedback  
    float motionThreshold = 0.01f;    // Motion threshold for feedback adjustment
    float ghostingReduction = 0.2f;   // Ghosting reduction strength
    bool enableNeighborhoodClamping = true;
    bool enableVarianceClipping = true;
    float varianceClipGamma = 1.0f;   // Variance clipping strength
    
    // Weather integration settings
    bool preserveWeatherTRP = true;   // Keep weather temporal reprojection separate
    float weatherBlendRatio = 0.3f;   // How much weather affects TAA (0=separate, 1=full blend)
    
    static TAASettings HighQuality()   { return {true, 0.95f, 0.98f, 0.005f, 0.15f, true, true, 0.8f, true, 0.25f}; }
    static TAASettings Balanced()      { return {true, 0.88f, 0.97f, 0.01f, 0.2f, true, true, 1.0f, true, 0.3f}; }
    static TAASettings Performance()   { return {true, 0.80f, 0.95f, 0.02f, 0.3f, true, false, 1.2f, true, 0.4f}; }
    static TAASettings Disabled()      { return {false, 0.0f, 0.0f, 0.0f, 0.0f, false, false, 0.0f, false, 0.0f}; }
};

/**
 * Camera motion data for TAA
 */
struct CameraMotion {
    glm::mat4 currentViewMatrix;
    glm::mat4 currentProjectionMatrix;
    glm::mat4 previousViewMatrix;
    glm::mat4 previousProjectionMatrix;
    glm::vec2 jitterOffset;           // Current frame jitter
    glm::vec2 previousJitterOffset;   // Previous frame jitter
    
    // Camera movement tracking
    float cameraVelocity = 0.0f;     // Linear velocity
    float cameraAngularVelocity = 0.0f; // Angular velocity
    bool isCameraStationary = true;
    
    glm::mat4 getViewProjectionMatrix() const;
    glm::mat4 getPreviousViewProjectionMatrix() const;
    glm::mat4 getReprojectionMatrix() const;
};

/**
 * TAA render targets and resources
 */
struct TAAResources {
    // History buffers (ping-pong)
    VkImage historyColorA = VK_NULL_HANDLE;
    VkImage historyColorB = VK_NULL_HANDLE;
    VkImageView historyColorViewA = VK_NULL_HANDLE;
    VkImageView historyColorViewB = VK_NULL_HANDLE;
    VMAAllocation historyAllocationA{};
    VMAAllocation historyAllocationB{};
    
    // Motion vector buffer
    VkImage motionVectors = VK_NULL_HANDLE;
    VkImageView motionVectorView = VK_NULL_HANDLE;
    VMAAllocation motionAllocation{};
    
    // Depth history for motion vector generation
    VkImage depthHistory = VK_NULL_HANDLE;
    VkImageView depthHistoryView = VK_NULL_HANDLE;
    VMAAllocation depthHistoryAllocation{};
    
    // Ping-pong state
    bool useHistoryA = true;
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat motionFormat = VK_FORMAT_R16G16_SFLOAT;
    
    bool isValid() const { return historyColorA != VK_NULL_HANDLE && motionVectors != VK_NULL_HANDLE; }
    void release(VkDevice device);
    VkImageView getCurrentHistoryView() const { return useHistoryA ? historyColorViewA : historyColorViewB; }
    VkImageView getPreviousHistoryView() const { return useHistoryA ? historyColorViewB : historyColorViewA; }
    void swapHistory() { useHistoryA = !useHistoryA; }
};

/**
 * TAA system implementation
 */
class TAASystem {
public:
    TAASystem(VkDevice device, VkPhysicalDevice physicalDevice);
    ~TAASystem();
    
    // Initialization
    bool initialize(uint32_t width, uint32_t height, VkFormat colorFormat = VK_FORMAT_R16G16B16A16_SFLOAT);
    void shutdown();
    
    // Resolution management
    bool resize(uint32_t newWidth, uint32_t newHeight);
    
    // Settings
    void setSettings(const TAASettings& settings) { settings_ = settings; }
    const TAASettings& getSettings() const { return settings_; }
    
    // Camera management
    void updateCameraMotion(const CameraMotion& motion);
    glm::vec2 getJitterOffset(uint32_t frameIndex) const;
    
    // TAA execution (frame graph integration)
    void executeMotionVectorPass(VkCommandBuffer cmd, VkImageView depth, VkImageView previousDepth);
    void executeTAAResolvePass(VkCommandBuffer cmd, VkImageView currentColor, VkImageView output);
    
    // Weather integration
    void setWeatherTRPTextures(VkImageView clouds, VkImageView precipitation);
    void executeWeatherTAABlend(VkCommandBuffer cmd, VkImageView taaResult, VkImageView weatherTRP, VkImageView output);
    
    // Resource access
    const TAAResources& getResources() const { return resources_; }
    VkImageView getCurrentHistory() const { return resources_.getCurrentHistoryView(); }
    VkImageView getMotionVectors() const { return resources_.motionVectorView; }
    
    // Statistics and debugging
    struct Stats {
        uint32_t framesProcessed = 0;
        float averageMotion = 0.0f;
        float ghostingReduction = 0.0f;
        double taaTime = 0.0;
        bool isStable = true;
    };
    
    const Stats& getStats() const { return stats_; }
    void logStats() const;
    
    // Performance monitoring
    bool isStable() const { return stats_.isStable && settings_.enabled; }
    void setPerformanceBudget(double maxTAATimeMs) { maxTAATimeMs_ = maxTAATimeMs; }
    
private:
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    
    TAASettings settings_{};
    TAAResources resources_{};
    CameraMotion cameraMotion_{};
    
    // Halton sequence for jitter pattern
    std::vector<glm::vec2> jitterSequence_;
    uint32_t jitterIndex_ = 0;
    
    // GPU resources
    VkPipeline motionVectorPipeline_ = VK_NULL_HANDLE;
    VkPipeline taaResolvePipeline_ = VK_NULL_HANDLE;
    VkPipeline weatherBlendPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout_ = VK_NULL_HANDLE;
    
    // Performance monitoring
    double maxTAATimeMs_ = 2.0; // 2ms budget for TAA
    mutable Stats stats_{};
    
    // Weather TRP integration
    VkImageView weatherClouds_ = VK_NULL_HANDLE;
    VkImageView weatherPrecipitation_ = VK_NULL_HANDLE;
    
    // Internal methods
    bool createResources(uint32_t width, uint32_t height);
    bool createPipelines();
    void destroyResources();
    
    void generateJitterSequence();
    glm::vec2 haltonSequence(uint32_t index, uint32_t base1, uint32_t base2);
    
    void updateMotionStatistics(const CameraMotion& motion);
    bool detectCameraMotion(const CameraMotion& motion);
};

/**
 * Camera jitter helper for TAA integration
 */
class CameraJitter {
public:
    // Jitter patterns
    enum class Pattern {
        HALTON_2_3,     // Halton sequence with bases 2,3 (recommended)
        UNIFORM_4X,     // 4x MSAA pattern
        UNIFORM_8X,     // 8x MSAA pattern  
        R2_SEQUENCE     // R2 low-discrepancy sequence
    };
    
    CameraJitter(Pattern pattern = Pattern::HALTON_2_3);
    
    glm::vec2 getJitter(uint32_t frameIndex) const;
    glm::mat4 applyJitterToProjection(const glm::mat4& projection, glm::vec2 jitter, uint32_t width, uint32_t height) const;
    
    void setPattern(Pattern pattern);
    Pattern getPattern() const { return pattern_; }
    
private:
    Pattern pattern_;
    std::vector<glm::vec2> jitterPattern_;
    
    void generateHaltonPattern(uint32_t sampleCount = 16);
    void generateUniformPattern(uint32_t sampleCount);
    void generateR2Pattern(uint32_t sampleCount = 16);
    
    float halton(uint32_t index, uint32_t base);
    float r2(uint32_t index, uint32_t dimension);
};

/**
 * Motion vector generation utility
 */
class MotionVectorGenerator {
public:
    struct MotionVectorData {
        glm::mat4 mvpCurrent;
        glm::mat4 mvpPrevious;
        glm::vec2 jitterCurrent;
        glm::vec2 jitterPrevious;
    };
    
    static void generateObjectMotion(
        const std::vector<glm::mat4>& objectMatrices,
        const std::vector<glm::mat4>& previousObjectMatrices,
        const MotionVectorData& cameraData,
        std::vector<glm::vec2>& motionVectors
    );
    
    static glm::vec2 calculatePixelMotion(
        const glm::vec3& worldPosition,
        const glm::mat4& currentMVP,
        const glm::mat4& previousMVP,
        uint32_t screenWidth,
        uint32_t screenHeight
    );
};

} // namespace voxelvk