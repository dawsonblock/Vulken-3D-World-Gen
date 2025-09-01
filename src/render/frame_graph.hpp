#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include "../vk/device_caps.hpp"

namespace voxelvk {

/**
 * Resource types for frame graph dependency tracking
 */
enum class ResourceType {
    BUFFER,
    IMAGE,
    RENDER_TARGET,
    DEPTH_BUFFER
};

/**
 * Resource access patterns for automatic barrier generation
 */
enum class ResourceAccess {
    UNDEFINED,
    READ_ONLY,
    WRITE_ONLY,
    READ_WRITE,
    RENDER_TARGET,
    DEPTH_STENCIL_READ,
    DEPTH_STENCIL_WRITE,
    PRESENT
};

/**
 * Frame graph resource handle
 */
struct ResourceHandle {
    uint32_t id = 0;
    ResourceType type = ResourceType::BUFFER;
    std::string name;
    
    bool isValid() const { return id != 0; }
    
    static ResourceHandle invalid() { return {}; }
};

/**
 * Resource description for frame graph resources
 */
struct ResourceDesc {
    ResourceType type;
    std::string name;
    
    // Buffer description
    VkDeviceSize bufferSize = 0;
    VkBufferUsageFlags bufferUsage = 0;
    
    // Image description  
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags imageUsage = 0;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    
    // Persistence (external resources)
    bool isExternal = false;
    VkBuffer externalBuffer = VK_NULL_HANDLE;
    VkImage externalImage = VK_NULL_HANDLE;
};

/**
 * Pass resource access for dependency analysis
 */
struct PassResourceAccess {
    ResourceHandle resource;
    ResourceAccess access;
    VkPipelineStageFlags2 stage;
    VkAccessFlags2 accessMask;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    // For images with subresource access
    VkImageSubresourceRange subresourceRange{};
};

/**
 * Frame graph pass
 */
class FrameGraphPass {
public:
    FrameGraphPass(const std::string& name, uint32_t passID) 
        : name_(name), passID_(passID) {}
    
    // Resource access declaration
    ResourceHandle read(ResourceHandle resource, VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
    ResourceHandle write(ResourceHandle resource, VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    ResourceHandle create(const ResourceDesc& desc);
    
    // Execution callback
    using ExecuteCallback = std::function<void(VkCommandBuffer, const std::unordered_map<ResourceHandle, VkBuffer>&, const std::unordered_map<ResourceHandle, VkImage>&)>;
    void setExecuteCallback(ExecuteCallback callback) { executeCallback_ = callback; }
    
    // Getters
    const std::string& getName() const { return name_; }
    uint32_t getPassID() const { return passID_; }
    const std::vector<PassResourceAccess>& getResourceAccesses() const { return resourceAccesses_; }
    const ExecuteCallback& getExecuteCallback() const { return executeCallback_; }
    
private:
    std::string name_;
    uint32_t passID_;
    std::vector<PassResourceAccess> resourceAccesses_;
    ExecuteCallback executeCallback_;
    
    void addResourceAccess(ResourceHandle resource, ResourceAccess access, VkPipelineStageFlags2 stage);
};

/**
 * Production frame graph with automatic synchronization
 */
class FrameGraph {
public:
    FrameGraph(VkDevice device, const DeviceCaps& deviceCaps);
    ~FrameGraph();
    
    // Setup
    bool initialize(uint32_t framesInFlight = 3);
    void shutdown();
    
    // Frame management
    void beginFrame(uint32_t frameIndex);
    void endFrame();
    
    // Pass building
    FrameGraphPass& addPass(const std::string& name);
    ResourceHandle createResource(const ResourceDesc& desc);
    ResourceHandle importResource(const std::string& name, VkBuffer buffer);
    ResourceHandle importResource(const std::string& name, VkImage image);
    
    // Execution
    void compile(); // Build dependency graph and barriers
    void execute(VkCommandBuffer commandBuffer);
    
    // Resource access
    VkBuffer getBuffer(ResourceHandle handle) const;
    VkImage getImage(ResourceHandle handle) const;
    
    // Statistics and debugging
    struct Stats {
        uint32_t totalPasses = 0;
        uint32_t totalResources = 0;
        uint32_t totalBarriers = 0;
        double compileTime = 0.0;
        double executeTime = 0.0;
    };
    
    const Stats& getStats() const { return stats_; }
    void logStats() const;
    void dumpDependencyGraph(const std::string& filename) const;
    
    // Performance monitoring
    void enableGPUTiming(bool enable) { gpuTimingEnabled_ = enable; }
    void setPerformanceBudget(double maxFrameTimeMs) { maxFrameTimeMs_ = maxFrameTimeMs; }
    bool isWithinPerformanceBudget() const;
    
private:
    VkDevice device_;
    const DeviceCaps& deviceCaps_;
    
    std::vector<std::unique_ptr<FrameGraphPass>> passes_;
    std::unordered_map<uint32_t, ResourceDesc> resources_;
    std::unordered_map<uint32_t, VkBuffer> buffers_;
    std::unordered_map<uint32_t, VkImage> images_;
    
    uint32_t nextResourceID_ = 1;
    uint32_t nextPassID_ = 1;
    uint32_t currentFrameIndex_ = 0;
    uint32_t framesInFlight_ = 3;
    
    // Compiled execution data
    struct CompiledPass {
        FrameGraphPass* pass;
        std::vector<VkMemoryBarrier2> memoryBarriers;
        std::vector<VkBufferMemoryBarrier2> bufferBarriers;
        std::vector<VkImageMemoryBarrier2> imageBarriers;
        VkDependencyInfo dependencyInfo;
    };
    
    std::vector<CompiledPass> compiledPasses_;
    bool isCompiled_ = false;
    
    // GPU timing
    bool gpuTimingEnabled_ = false;
    double maxFrameTimeMs_ = 8.33; // 120 FPS target
    std::vector<VkQueryPool> timestampPools_;
    std::vector<double> passTimings_;
    
    // Statistics
    mutable Stats stats_{};
    
    // Internal methods
    void compileDependencies();
    void generateBarriers();
    bool hasSynchronization2() const { return deviceCaps_.hasSynchronization2; }
    
    VkPipelineStageFlags2 accessToStage(ResourceAccess access) const;
    VkAccessFlags2 accessToFlags(ResourceAccess access) const;
    VkImageLayout accessToLayout(ResourceAccess access) const;
    
    bool createGPUTimingResources();
    void beginPassTiming(VkCommandBuffer cmd, uint32_t passIndex);
    void endPassTiming(VkCommandBuffer cmd, uint32_t passIndex);
    void collectTimingResults();
};

/**
 * Frame graph builder for convenient pass creation
 */
class FrameGraphBuilder {
public:
    FrameGraphBuilder(FrameGraph& frameGraph) : frameGraph_(frameGraph) {}
    
    // Weather system integration
    FrameGraphPass& addWeatherUpdatePass();
    FrameGraphPass& addSkyRenderPass(ResourceHandle colorTarget, ResourceHandle depthTarget);
    FrameGraphPass& addCloudRenderPass(ResourceHandle colorTarget);
    FrameGraphPass& addPrecipitationPass(ResourceHandle colorTarget, ResourceHandle depthTarget);
    FrameGraphPass& addTemporalAccumulationPass(ResourceHandle current, ResourceHandle history, ResourceHandle output);
    FrameGraphPass& addHeightFogPass(ResourceHandle sceneColor, ResourceHandle depth, ResourceHandle output);
    
    // Standard rendering passes
    FrameGraphPass& addGBufferPass(ResourceHandle albedo, ResourceHandle normal, ResourceHandle depth);
    FrameGraphPass& addLightingPass(ResourceHandle gbuffers, ResourceHandle output);
    FrameGraphPass& addPostProcessPass(ResourceHandle input, ResourceHandle output);
    
    // TAA integration
    FrameGraphPass& addTAAResolvePass(ResourceHandle current, ResourceHandle history, ResourceHandle motion, ResourceHandle output);
    
    // Screen space effects
    FrameGraphPass& addSSAOPass(ResourceHandle depth, ResourceHandle normal, ResourceHandle output);
    FrameGraphPass& addSSRPass(ResourceHandle gbuffers, ResourceHandle output);
    
private:
    FrameGraph& frameGraph_;
};

} // namespace voxelvk