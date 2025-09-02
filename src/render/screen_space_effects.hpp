#pragma once
#include <vulkan/vulkan.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <vector>
#include <string>
#include "../vk/memory_manager.hpp"
#include <mutex>
// Frame graph integration types
#include "frame_graph.hpp"

namespace voxelvk {

/**
 * CVar system for runtime effect toggling
 */
class CVarSystem {
public:
    static CVarSystem& instance();
    
    // CVar registration and access
    void registerBoolVar(const std::string& name, bool defaultValue, const std::string& description = "");
    void registerFloatVar(const std::string& name, float defaultValue, float minValue, float maxValue, const std::string& description = "");
    void registerIntVar(const std::string& name, int defaultValue, int minValue, int maxValue, const std::string& description = "");
    
    bool getBool(const std::string& name) const;
    float getFloat(const std::string& name) const;
    int getInt(const std::string& name) const;
    
    bool setBool(const std::string& name, bool value);
    bool setFloat(const std::string& name, float value);
    bool setInt(const std::string& name, int value);
    
    // Console integration
    bool executeCommand(const std::string& command);
    std::vector<std::string> getMatchingVars(const std::string& prefix) const;
    
    void logAllVars() const;
    
private:
    struct CVar {
        enum Type { BOOL, FLOAT, INT };
        Type type;
        std::string name;
        std::string description;
        
        union {
            bool boolValue;
            float floatValue;
            int intValue;
        };
        
        float minFloat = 0.0f, maxFloat = 1.0f;
        int minInt = 0, maxInt = 100;
    };
    
    std::vector<CVar> cvars_;
    mutable std::mutex cvarMutex_;
    
    CVar* findVar(const std::string& name);
    const CVar* findVar(const std::string& name) const;
};

/**
 * Screen Space Ambient Occlusion (SSAO) implementation
 */
struct SSAOSettings {
    bool enabled = true;
    uint32_t sampleCount = 32;        // Number of samples per pixel
    float radius = 1.5f;              // AO sampling radius in world units
    float bias = 0.025f;              // Depth bias to prevent self-occlusion
    float strength = 1.0f;            // AO darkening strength
    float contrast = 1.0f;            // AO contrast enhancement
    
    // Performance settings
    bool halfResolution = true;       // Render at 50% resolution for performance
    bool enableBlur = true;           // Bilateral blur to reduce noise
    uint32_t blurRadius = 4;          // Blur kernel radius
    
    static SSAOSettings HighQuality()   { return {true, 64, 1.5f, 0.02f, 1.0f, 1.2f, false, true, 6}; }
    static SSAOSettings Balanced()      { return {true, 32, 1.5f, 0.025f, 1.0f, 1.0f, true, true, 4}; }
    static SSAOSettings Performance()   { return {true, 16, 1.2f, 0.03f, 0.8f, 0.8f, true, true, 2}; }
    static SSAOSettings Disabled()      { return {false, 0, 0.0f, 0.0f, 0.0f, 0.0f, false, false, 0}; }
};

class SSAOSystem {
public:
    SSAOSystem(VkDevice device, VkPhysicalDevice physicalDevice);
    ~SSAOSystem();
    
    bool initialize(uint32_t width, uint32_t height);
    void shutdown();
    bool resize(uint32_t newWidth, uint32_t newHeight);
    
    // Settings management
    void setSettings(const SSAOSettings& settings);
    const SSAOSettings& getSettings() const { return settings_; }
    
    // Execution
    void executeSSAO(VkCommandBuffer cmd, VkImageView depth, VkImageView normal, VkImageView output);
    
    // Resource access
    VkImageView getSSAOTexture() const { return aoTextureView_; }
    
    // Statistics
    struct Stats {
        uint32_t framesProcessed = 0;
        double averageTime = 0.0;
        float averageAO = 0.0f;
    };
    
    const Stats& getStats() const { return stats_; }
    void logStats() const;
    
private:
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    SSAOSettings settings_{};
    
    // GPU resources
    VkImage aoTexture_ = VK_NULL_HANDLE;
    VkImageView aoTextureView_ = VK_NULL_HANDLE;
    VMAAllocation aoAllocation_{};
    
    VkImage blurTexture_ = VK_NULL_HANDLE;
    VkImageView blurTextureView_ = VK_NULL_HANDLE;
    VMAAllocation blurAllocation_{};
    
    VkPipeline ssaoPipeline_ = VK_NULL_HANDLE;
    VkPipeline blurPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    
    // Sample kernel for SSAO
    std::vector<glm::vec3> sampleKernel_;
    VkBuffer sampleBuffer_ = VK_NULL_HANDLE;
    VMAAllocation sampleAllocation_{};
    
    uint32_t width_ = 0, height_ = 0;
    mutable Stats stats_{};
    
    bool createResources();
    bool createPipelines();
    void destroyResources();
    
    void generateSampleKernel();
    glm::vec3 generateRandomVector();
};

/**
 * Screen Space Reflections (SSR) implementation  
 */
struct SSRSettings {
    bool enabled = false;             // Disabled by default (expensive)
    uint32_t maxSteps = 32;           // Maximum ray marching steps
    float stepSize = 0.1f;            // Ray marching step size
    float maxDistance = 50.0f;        // Maximum reflection distance
    float thickness = 0.5f;           // Surface thickness for intersection
    
    // Quality settings
    bool roughnessAware = true;       // Fade reflections based on surface roughness
    float maxRoughness = 0.6f;        // Maximum roughness for reflections
    bool enableDenoising = true;      // Spatial-temporal denoising
    uint32_t denoiseRadius = 2;       // Denoising kernel radius
    
    // Performance settings
    bool halfResolution = true;       // Render at 50% resolution
    bool enableMipchain = true;       // Use hierarchical Z-buffer
    uint32_t maxMipLevel = 6;         // Maximum mip level for tracing
    
    static SSRSettings HighQuality()   { return {true, 64, 0.05f, 100.0f, 0.3f, true, 0.8f, true, 3, false, true, 8}; }
    static SSRSettings Balanced()      { return {true, 32, 0.1f, 50.0f, 0.5f, true, 0.6f, true, 2, true, true, 6}; }
    static SSRSettings Performance()   { return {true, 16, 0.2f, 30.0f, 0.8f, true, 0.4f, false, 1, true, false, 4}; }
    static SSRSettings Disabled()      { return {false, 0, 0.0f, 0.0f, 0.0f, false, 0.0f, false, 0, false, false, 0}; }
};

class SSRSystem {
public:
    SSRSystem(VkDevice device, VkPhysicalDevice physicalDevice);
    ~SSRSystem();
    
    bool initialize(uint32_t width, uint32_t height);
    void shutdown();
    bool resize(uint32_t newWidth, uint32_t newHeight);
    
    // Settings management
    void setSettings(const SSRSettings& settings);
    const SSRSettings& getSettings() const { return settings_; }
    
    // Execution
    void executeSSR(VkCommandBuffer cmd, 
                   VkImageView color, VkImageView depth, VkImageView normal, VkImageView roughness,
                   VkImageView output);
    
    // Resource access
    VkImageView getSSRTexture() const { return reflectionTextureView_; }
    
    // Statistics
    struct Stats {
        uint32_t framesProcessed = 0;
        double averageTime = 0.0;
        float averageReflectionCoverage = 0.0f;
        uint32_t raysMissed = 0;
        uint32_t raysHit = 0;
    };
    
    const Stats& getStats() const { return stats_; }
    void logStats() const;
    
private:
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    SSRSettings settings_{};
    
    // GPU resources
    VkImage reflectionTexture_ = VK_NULL_HANDLE;
    VkImageView reflectionTextureView_ = VK_NULL_HANDLE;
    VMAAllocation reflectionAllocation_{};
    
    VkImage depthMipchain_ = VK_NULL_HANDLE;
    VkImageView depthMipchainView_ = VK_NULL_HANDLE;
    VMAAllocation depthMipchainAllocation_{};
    
    VkPipeline ssrPipeline_ = VK_NULL_HANDLE;
    VkPipeline denoisePipeline_ = VK_NULL_HANDLE;
    VkPipeline depthMipPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    
    uint32_t width_ = 0, height_ = 0;
    mutable Stats stats_{};
    
    bool createResources();
    bool createPipelines();
    void destroyResources();
    
    void generateDepthMipchain(VkCommandBuffer cmd, VkImageView depth);
};

/**
 * Integrated screen-space effects manager
 */
class ScreenSpaceEffects {
public:
    ScreenSpaceEffects(VkDevice device, VkPhysicalDevice physicalDevice);
    ~ScreenSpaceEffects();
    
    // Initialization
    bool initialize(uint32_t width, uint32_t height);
    void shutdown();
    bool resize(uint32_t newWidth, uint32_t newHeight);
    
    // Effect management
    SSAOSystem& getSSAO() { return ssaoSystem_; }
    SSRSystem& getSSR() { return ssrSystem_; }
    
    // CVar integration
    void registerCVars();
    void updateFromCVars();
    
    // Frame graph integration
    void addSSAOPass(class FrameGraphBuilder& builder, ResourceHandle depth, ResourceHandle normal, ResourceHandle output);
    void addSSRPass(class FrameGraphBuilder& builder, ResourceHandle gbuffers, ResourceHandle output);
    
    // Combined execution
    void executeScreenSpacePass(VkCommandBuffer cmd, 
                               VkImageView color, VkImageView depth, VkImageView normal, VkImageView roughness,
                               VkImageView output);
    
    // Performance monitoring
    struct PerformanceStats {
        double ssaoTime = 0.0;
        double ssrTime = 0.0;
        double totalTime = 0.0;
        bool withinBudget = true;
    };
    
    const PerformanceStats& getPerformanceStats() const { return perfStats_; }
    void setPerformanceBudget(double maxTimeMs) { maxScreenSpaceTimeMs_ = maxTimeMs; }
    
    void logPerformanceReport() const;
    
private:
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    
    SSAOSystem ssaoSystem_;
    SSRSystem ssrSystem_;
    
    double maxScreenSpaceTimeMs_ = 3.0; // 3ms budget for screen space effects
    mutable PerformanceStats perfStats_{};
    
    bool cvarRegistered_ = false;
};

} // namespace voxelvk