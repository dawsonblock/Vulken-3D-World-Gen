#include "frame_graph.hpp"
#include "../vk/error_handling.hpp"
#include "../core/logger.hpp"
#include "../core/nvtx_profiler.hpp"
#include <algorithm>
#include <fstream>
#include <chrono>

namespace voxelvk {

static Logger g_frameGraphLogger("FrameGraph");

FrameGraph::FrameGraph(VkDevice device, const DeviceCaps& deviceCaps)
    : device_(device), deviceCaps_(deviceCaps) {
    
    g_frameGraphLogger.Info("FrameGraph initialized");
    g_frameGraphLogger.Info("  Synchronization2 support: {}", hasSynchronization2() ? "YES" : "NO");
}

FrameGraph::~FrameGraph() {
    shutdown();
}

bool FrameGraph::initialize(uint32_t framesInFlight) {
    framesInFlight_ = framesInFlight;
    
    g_frameGraphLogger.Info("Initializing production frame graph");
    g_frameGraphLogger.Info("  Frames in flight: {}", framesInFlight_);
    g_frameGraphLogger.Info("  GPU timing: {}", gpuTimingEnabled_ ? "ENABLED" : "DISABLED");
    g_frameGraphLogger.Info("  Performance budget: {:.2f}ms (target: {:.0f} FPS)", 
        maxFrameTimeMs_, 1000.0 / maxFrameTimeMs_);
    
    if (gpuTimingEnabled_) {
        if (!createGPUTimingResources()) {
            g_frameGraphLogger.Warn("Failed to create GPU timing resources");
            gpuTimingEnabled_ = false;
        }
    }
    
    g_frameGraphLogger.Info("Frame graph initialized successfully");
    return true;
}

void FrameGraph::shutdown() {
    g_frameGraphLogger.Info("Shutting down frame graph");
    
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        
        // Destroy GPU timing resources
        for (auto pool : timestampPools_) {
            if (pool != VK_NULL_HANDLE) {
                vkDestroyQueryPool(device_, pool, nullptr);
            }
        }
        timestampPools_.clear();
    }
    
    // Clear all data
    passes_.clear();
    resources_.clear();
    buffers_.clear();
    images_.clear();
    compiledPasses_.clear();
    
    isCompiled_ = false;
    
    g_frameGraphLogger.Info("Frame graph shutdown complete");
}

void FrameGraph::beginFrame(uint32_t frameIndex) {
    currentFrameIndex_ = frameIndex % framesInFlight_;
    
    // Reset frame-specific data
    passTimings_.clear();
    passTimings_.resize(passes_.size(), 0.0);
    
    g_frameGraphLogger.Debug("Frame {} begun (flight index: {})", frameIndex, currentFrameIndex_);
}

void FrameGraph::endFrame() {
    if (gpuTimingEnabled_) {
        collectTimingResults();
    }
    
    // Log performance if over budget
    double totalFrameTime = 0.0;
    for (double timing : passTimings_) {
        totalFrameTime += timing;
    }
    
    if (totalFrameTime > maxFrameTimeMs_) {
        g_frameGraphLogger.Warn("Frame over budget: {:.2f}ms (target: {:.2f}ms)", 
            totalFrameTime, maxFrameTimeMs_);
    }
    
    g_frameGraphLogger.Debug("Frame ended - total GPU time: {:.2f}ms", totalFrameTime);
}

FrameGraphPass& FrameGraph::addPass(const std::string& name) {
    auto pass = std::make_unique<FrameGraphPass>(name, nextPassID_++);
    FrameGraphPass& passRef = *pass;
    passes_.push_back(std::move(pass));
    
    // Mark as needing recompilation
    isCompiled_ = false;
    
    g_frameGraphLogger.Debug("Added pass: '{}' (ID: {})", name, passRef.getPassID());
    return passRef;
}

ResourceHandle FrameGraph::createResource(const ResourceDesc& desc) {
    ResourceHandle handle;
    handle.id = nextResourceID_++;
    handle.type = desc.type;
    handle.name = desc.name;
    
    resources_[handle.id] = desc;
    
    // Mark as needing recompilation
    isCompiled_ = false;
    
    g_frameGraphLogger.Debug("Created resource: '{}' (ID: {}, Type: {})", 
        desc.name, handle.id, static_cast<int>(desc.type));
    
    return handle;
}

void FrameGraph::compile() {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    g_frameGraphLogger.Info("Compiling frame graph: {} passes, {} resources", 
        passes_.size(), resources_.size());
    
    compiledPasses_.clear();
    compiledPasses_.reserve(passes_.size());
    
    // Compile dependencies and generate barriers
    compileDependencies();
    generateBarriers();
    
    isCompiled_ = true;
    
    auto endTime = std::chrono::high_resolution_clock::now();
    stats_.compileTime = std::chrono::duration<double>(endTime - startTime).count();
    
    stats_.totalPasses = static_cast<uint32_t>(passes_.size());
    stats_.totalResources = static_cast<uint32_t>(resources_.size());
    
    g_frameGraphLogger.Info("Frame graph compilation complete: {:.3f}ms", stats_.compileTime * 1000.0);
    g_frameGraphLogger.Info("  Generated {} memory barriers", stats_.totalBarriers);
}

void FrameGraph::execute(VkCommandBuffer commandBuffer) {
    if (!isCompiled_) {
        g_frameGraphLogger.Error("Frame graph not compiled - call compile() first");
        return;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    VXL_NVTX_RANGE("FrameGraph::Execute");
    
    // Execute all compiled passes
    for (size_t i = 0; i < compiledPasses_.size(); i++) {
        const auto& compiledPass = compiledPasses_[i];
        
        // Insert barriers if using synchronization2
        if (hasSynchronization2() && 
            (!compiledPass.memoryBarriers.empty() || 
             !compiledPass.bufferBarriers.empty() || 
             !compiledPass.imageBarriers.empty())) {
            
            VK_DEBUG_LABEL(commandBuffer, ("Barriers_" + compiledPass.pass->getName()).c_str());
            
            // Use synchronization2 for explicit dependency management
            auto vkCmdPipelineBarrier2 = (PFN_vkCmdPipelineBarrier2)
                vkGetDeviceProcAddr(device_, "vkCmdPipelineBarrier2");
            
            if (vkCmdPipelineBarrier2) {
                vkCmdPipelineBarrier2(commandBuffer, &compiledPass.dependencyInfo);
            } else {
                // Fallback to legacy barriers
                vkCmdPipelineBarrier(commandBuffer,
                    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                    0, 0, nullptr, 0, nullptr, 0, nullptr);
            }
        }
        
        // Begin GPU timing
        if (gpuTimingEnabled_) {
            beginPassTiming(commandBuffer, static_cast<uint32_t>(i));
        }
        
        // Execute pass
        {
            VXL_NVTX_RANGE(compiledPass.pass->getName().c_str());
            VK_DEBUG_LABEL(commandBuffer, compiledPass.pass->getName().c_str());
            
            if (compiledPass.pass->getExecuteCallback()) {
                compiledPass.pass->getExecuteCallback()(commandBuffer, buffers_, images_);
            }
            
            // NVTX range automatically ends here
        }
        
        // End GPU timing
        if (gpuTimingEnabled_) {
            endPassTiming(commandBuffer, static_cast<uint32_t>(i));
        }
    }
    
    // NVTX range automatically ends here
    
    auto endTime = std::chrono::high_resolution_clock::now();
    stats_.executeTime = std::chrono::duration<double>(endTime - startTime).count();
}

void FrameGraph::compileDependencies() {
    g_frameGraphLogger.Debug("Analyzing pass dependencies");
    
    // Simple dependency analysis - in a production system this would be more sophisticated
    for (auto& pass : passes_) {
        CompiledPass compiled;
        compiled.pass = pass.get();
        
        // Initialize dependency info for synchronization2
        compiled.dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        compiled.dependencyInfo.memoryBarrierCount = 0;
        compiled.dependencyInfo.bufferMemoryBarrierCount = 0;
        compiled.dependencyInfo.imageMemoryBarrierCount = 0;
        
        compiledPasses_.push_back(compiled);
    }
}

void FrameGraph::generateBarriers() {
    g_frameGraphLogger.Debug("Generating synchronization barriers");
    
    if (!hasSynchronization2()) {
        g_frameGraphLogger.Info("Using legacy pipeline barriers (sync2 not available)");
        return;
    }
    
    stats_.totalBarriers = 0;
    
    // Generate barriers between passes based on resource access
    for (size_t i = 1; i < compiledPasses_.size(); i++) {
        auto& compiled = compiledPasses_[i];
        auto& prevCompiled = compiledPasses_[i - 1];
        
        // Analyze resource dependencies between consecutive passes
        for (const auto& access : compiled.pass->getResourceAccesses()) {
            // Check if previous pass accessed this resource
            for (const auto& prevAccess : prevCompiled.pass->getResourceAccesses()) {
                if (access.resource.id == prevAccess.resource.id && 
                    (prevAccess.access == ResourceAccess::WRITE_ONLY || 
                     prevAccess.access == ResourceAccess::READ_WRITE)) {
                    
                    // Generate appropriate barrier
                    if (access.resource.type == ResourceType::IMAGE) {
                        VkImageMemoryBarrier2 barrier{};
                        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                        barrier.srcStageMask = prevAccess.stage;
                        barrier.srcAccessMask = prevAccess.accessMask;
                        barrier.dstStageMask = access.stage;
                        barrier.dstAccessMask = access.accessMask;
                        barrier.oldLayout = prevAccess.layout;
                        barrier.newLayout = access.layout;
                        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        // barrier.image would be set during execution
                        barrier.subresourceRange = access.subresourceRange;
                        
                        compiled.imageBarriers.push_back(barrier);
                        stats_.totalBarriers++;
                    }
                    
                    break;
                }
            }
        }
        
        // Update dependency info
        compiled.dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(compiled.imageBarriers.size());
        compiled.dependencyInfo.pImageMemoryBarriers = compiled.imageBarriers.data();
    }
    
    g_frameGraphLogger.Debug("Generated {} synchronization barriers", stats_.totalBarriers);
}

bool FrameGraph::createGPUTimingResources() {
    g_frameGraphLogger.Debug("Creating GPU timing resources");
    
    timestampPools_.resize(framesInFlight_);
    
    for (uint32_t i = 0; i < framesInFlight_; i++) {
        VkQueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = static_cast<uint32_t>(passes_.size() * 2); // Begin + end for each pass
        
        VkResult result = vkCreateQueryPool(device_, &queryPoolInfo, nullptr, &timestampPools_[i]);
        if (result != VK_SUCCESS) {
            // CHECK_VK_OBJECT(result, VkErrorCategory::RESOURCE_CREATION, "timestamp_query_pool");
            return false;
        }
        
        std::string name = "TimestampPool_" + std::to_string(i);
        VK_OBJECT_NAME(device_, timestampPools_[i], VK_OBJECT_TYPE_QUERY_POOL, name.c_str());
    }
    
    return true;
}

void FrameGraph::beginPassTiming(VkCommandBuffer cmd, uint32_t passIndex) {
    if (passIndex >= passes_.size()) return;
    
    VkQueryPool pool = timestampPools_[currentFrameIndex_];
    if (pool != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pool, passIndex * 2);
    }
}

void FrameGraph::endPassTiming(VkCommandBuffer cmd, uint32_t passIndex) {
    if (passIndex >= passes_.size()) return;
    
    VkQueryPool pool = timestampPools_[currentFrameIndex_];
    if (pool != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, passIndex * 2 + 1);
    }
}

void FrameGraph::collectTimingResults() {
    VkQueryPool pool = timestampPools_[currentFrameIndex_];
    if (pool == VK_NULL_HANDLE) return;
    
    uint32_t queryCount = static_cast<uint32_t>(passes_.size() * 2);
    std::vector<uint64_t> timestamps(queryCount);
    
    VkResult result = vkGetQueryPoolResults(device_, pool, 0, queryCount,
        timestamps.size() * sizeof(uint64_t), timestamps.data(),
        sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    
    if (result == VK_SUCCESS) {
        // Convert timestamps to milliseconds
        float timestampPeriod = deviceCaps_.properties.limits.timestampPeriod;
        
        for (size_t i = 0; i < passes_.size() && i * 2 + 1 < timestamps.size(); i++) {
            uint64_t startTime = timestamps[i * 2];
            uint64_t endTime = timestamps[i * 2 + 1];
            
            if (endTime > startTime) {
                double passTime = (endTime - startTime) * timestampPeriod / 1000000.0; // Convert to ms
                passTimings_[i] = passTime;
            }
        }
    }
    
    // Reset query pool for next frame
    vkCmdResetQueryPool(nullptr, pool, 0, queryCount); // Would need command buffer
}

bool FrameGraph::isWithinPerformanceBudget() const {
    double totalTime = 0.0;
    for (double timing : passTimings_) {
        totalTime += timing;
    }
    return totalTime <= maxFrameTimeMs_;
}

void FrameGraph::logStats() const {
    g_frameGraphLogger.Info("=== Frame Graph Statistics ===");
    g_frameGraphLogger.Info("  Total passes: {}", stats_.totalPasses);
    g_frameGraphLogger.Info("  Total resources: {}", stats_.totalResources);
    g_frameGraphLogger.Info("  Total barriers: {}", stats_.totalBarriers);
    g_frameGraphLogger.Info("  Compile time: {:.3f}ms", stats_.compileTime * 1000.0);
    g_frameGraphLogger.Info("  Execute time: {:.3f}ms", stats_.executeTime * 1000.0);
    
    if (gpuTimingEnabled_ && !passTimings_.empty()) {
        g_frameGraphLogger.Info("  Pass timings:");
        double totalGPUTime = 0.0;
        
        for (size_t i = 0; i < passTimings_.size(); i++) {
            totalGPUTime += passTimings_[i];
            g_frameGraphLogger.Info("    {}: {:.3f}ms", passes_[i]->getName(), passTimings_[i]);
        }
        
        g_frameGraphLogger.Info("  Total GPU time: {:.3f}ms", totalGPUTime);
        g_frameGraphLogger.Info("  Performance budget: {:.3f}ms ({})",
            maxFrameTimeMs_, isWithinPerformanceBudget() ? "WITHIN" : "OVER");
    }
    
    g_frameGraphLogger.Info("==============================");
}

VkPipelineStageFlags2 FrameGraph::accessToStage(ResourceAccess access) const {
    switch (access) {
        case ResourceAccess::READ_ONLY:
            return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        case ResourceAccess::WRITE_ONLY:
        case ResourceAccess::READ_WRITE:
            return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        case ResourceAccess::RENDER_TARGET:
            return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        case ResourceAccess::DEPTH_STENCIL_READ:
            return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
        case ResourceAccess::DEPTH_STENCIL_WRITE:
            return VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        case ResourceAccess::PRESENT:
            return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        default:
            return VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    }
}

VkAccessFlags2 FrameGraph::accessToFlags(ResourceAccess access) const {
    switch (access) {
        case ResourceAccess::READ_ONLY:
            return VK_ACCESS_2_SHADER_READ_BIT;
        case ResourceAccess::WRITE_ONLY:
            return VK_ACCESS_2_SHADER_WRITE_BIT;
        case ResourceAccess::READ_WRITE:
            return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
        case ResourceAccess::RENDER_TARGET:
            return VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        case ResourceAccess::DEPTH_STENCIL_READ:
            return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        case ResourceAccess::DEPTH_STENCIL_WRITE:
            return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case ResourceAccess::PRESENT:
            return VK_ACCESS_2_MEMORY_READ_BIT;
        default:
            return VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    }
}

VkImageLayout FrameGraph::accessToLayout(ResourceAccess access) const {
    switch (access) {
        case ResourceAccess::READ_ONLY:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case ResourceAccess::WRITE_ONLY:
        case ResourceAccess::READ_WRITE:
            return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceAccess::RENDER_TARGET:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case ResourceAccess::DEPTH_STENCIL_READ:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case ResourceAccess::DEPTH_STENCIL_WRITE:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case ResourceAccess::PRESENT:
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        default:
            return VK_IMAGE_LAYOUT_GENERAL;
    }
}

// FrameGraphPass implementation
ResourceHandle FrameGraphPass::read(ResourceHandle resource, VkPipelineStageFlags2 stage) {
    addResourceAccess(resource, ResourceAccess::READ_ONLY, stage);
    return resource;
}

ResourceHandle FrameGraphPass::write(ResourceHandle resource, VkPipelineStageFlags2 stage) {
    addResourceAccess(resource, ResourceAccess::WRITE_ONLY, stage);
    return resource;
}

void FrameGraphPass::addResourceAccess(ResourceHandle resource, ResourceAccess access, VkPipelineStageFlags2 stage) {
    PassResourceAccess passAccess;
    passAccess.resource = resource;
    passAccess.access = access;
    passAccess.stage = stage;
    // accessMask and layout would be determined by access type
    
    resourceAccesses_.push_back(passAccess);
}

} // namespace voxelvk