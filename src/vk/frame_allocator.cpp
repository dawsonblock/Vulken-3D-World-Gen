#include "frame_allocator.hpp"
#include "error_handling.hpp"
#include "../core/logger.hpp"
#include <algorithm>
#include <cstring>

namespace voxelvk {

static Logger g_frameAllocLogger("FrameAllocator");

PerFrameAllocator* PerFrameAllocator::s_instance = nullptr;

FrameArena::FrameArena(size_t totalSize, const char* debugName) 
    : totalSize_(totalSize), debugName_(debugName) {
    
    g_frameAllocLogger.Debug("Creating frame arena: {} ({} MB)", debugName, totalSize / (1024.0 * 1024.0));
    
    // Create persistent mapped buffer for arena
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = totalSize;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | 
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    allocation_ = MemoryManager::instance().createBuffer(
        bufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, MemoryCategory::UNIFORMS, debugName);
    
    if (allocation_.allocation == VK_NULL_HANDLE) {
        g_frameAllocLogger.Error("Failed to create frame arena buffer");
        return;
    }
    
    // Map buffer persistently
    mappedData_ = MemoryManager::instance().map(allocation_);
    if (!mappedData_) {
        g_frameAllocLogger.Error("Failed to map frame arena buffer");
        return;
    }
    
    // Clear the buffer
    std::memset(mappedData_, 0, totalSize);
    
    g_frameAllocLogger.Info("Frame arena '{}' created: {:.1f} MB", debugName, totalSize / (1024.0 * 1024.0));
}

FrameArena::~FrameArena() {
    if (buffer_ != VK_NULL_HANDLE) {
        if (mappedData_) {
            MemoryManager::instance().unmap(allocation_);
        }
        
        MemoryManager::instance().destroyBuffer(buffer_, allocation_);
        g_frameAllocLogger.Debug("Frame arena '{}' destroyed", debugName_);
    }
}

void* FrameArena::allocate(size_t size, size_t alignment) {
    if (!mappedData_ || size == 0) return nullptr;
    
    stats_.totalAllocations.fetch_add(1);
    
    // Align offset
    size_t currentOffset = currentOffset_.load();
    size_t alignedOffset = (currentOffset + alignment - 1) & ~(alignment - 1);
    size_t newOffset = alignedOffset + size;
    
    // Check for overflow
    if (newOffset > totalSize_) {
        stats_.overflowCount.fetch_add(1);
        g_frameAllocLogger.Warn("Frame arena '{}' overflow: requested {} bytes, {} bytes available", 
            debugName_, size, totalSize_ - currentOffset);
        return nullptr;
    }
    
    // Atomic update (may fail and retry if another thread allocates)
    while (!currentOffset_.compare_exchange_weak(currentOffset, newOffset)) {
        // Recalculate alignment with updated offset
        alignedOffset = (currentOffset + alignment - 1) & ~(alignment - 1);
        newOffset = alignedOffset + size;
        
        if (newOffset > totalSize_) {
            stats_.overflowCount.fetch_add(1);
            return nullptr;
        }
    }
    
    updatePeakUsage(newOffset);
    
    void* ptr = static_cast<char*>(mappedData_) + alignedOffset;
    return ptr;
}

void FrameArena::reset() {
    currentOffset_.store(0);
    stats_.resetCount.fetch_add(1);
    
    g_frameAllocLogger.Debug("Frame arena '{}' reset - peak usage: {:.1f} KB", 
        debugName_, stats_.peakUsage.load() / 1024.0);
}

void FrameArena::updatePeakUsage(size_t usage) {
    size_t peak = stats_.peakUsage.load();
    while (usage > peak && !stats_.peakUsage.compare_exchange_weak(peak, usage)) {
        // Retry if another thread updated peak
    }
}

PerFrameAllocator::PerFrameAllocator() {
    g_frameAllocLogger.Info("PerFrameAllocator initialized");
}

PerFrameAllocator::~PerFrameAllocator() {
    shutdown();
}

bool PerFrameAllocator::initialize(size_t arenaSize) {
    g_frameAllocLogger.Info("Initializing per-frame allocator with {} MB per frame", 
        arenaSize / (1024.0 * 1024.0));
    
    // Create frame arenas
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        std::string name = "FrameArena_" + std::to_string(i);
        frameArenas_[i] = std::make_unique<FrameArena>(arenaSize, name.c_str());
        
        if (!frameArenas_[i]->isValid()) {
            g_frameAllocLogger.Error("Failed to create frame arena {}", i);
            return false;
        }
    }
    
    // Create uniform buffers for frame data
    if (!createUniformBuffers(UNIFORM_BUFFER_SIZE)) {
        g_frameAllocLogger.Error("Failed to create uniform buffers");
        return false;
    }
    
    initialized_ = true;
    s_instance = this;
    
    g_frameAllocLogger.Info("Per-frame allocator initialized successfully");
    g_frameAllocLogger.Info("  {} frames in flight", FRAMES_IN_FLIGHT);
    g_frameAllocLogger.Info("  {:.1f} MB arena per frame", arenaSize / (1024.0 * 1024.0));
    g_frameAllocLogger.Info("  {:.1f} MB uniform buffer per frame", UNIFORM_BUFFER_SIZE / (1024.0 * 1024.0));
    
    return true;
}

void PerFrameAllocator::shutdown() {
    if (!initialized_) return;
    
    g_frameAllocLogger.Info("Shutting down per-frame allocator");
    
    // Log final statistics
    logFrameStats();
    
    // Destroy uniform buffers
    destroyUniformBuffers();
    
    // Destroy frame arenas
    for (auto& arena : frameArenas_) {
        arena.reset();
    }
    
    initialized_ = false;
    s_instance = nullptr;
}

void PerFrameAllocator::beginFrame(uint32_t frameIndex) {
    currentFrameIndex_ = frameIndex % FRAMES_IN_FLIGHT;
    
    // Reset current frame arena
    frameArenas_[currentFrameIndex_]->reset();
    
    // Reset uniform buffer offset for this frame
    uniformOffsets_[currentFrameIndex_] = 0;
    
    g_frameAllocLogger.Debug("Frame {} began (arena index: {})", frameIndex, currentFrameIndex_);
}

void PerFrameAllocator::endFrame() {
    if (!initialized_) return;
    
    auto stats = getCurrentFrameStats();
    
    // Log warning if arena utilization is high
    if (stats.utilizationPercent > 80.0f) {
        g_frameAllocLogger.Warn("High frame arena utilization: {:.1f}% ({} allocations)",
            stats.utilizationPercent, stats.allocationCount);
    }
    
    g_frameAllocLogger.Debug("Frame ended - arena utilization: {:.1f}%", stats.utilizationPercent);
}

void* PerFrameAllocator::allocateTransient(size_t size, size_t alignment) {
    if (!initialized_) {
        g_frameAllocLogger.Error("PerFrameAllocator not initialized");
        return nullptr;
    }
    
    return frameArenas_[currentFrameIndex_]->allocate(size, alignment);
}

PerFrameAllocator::UniformAllocation PerFrameAllocator::allocateUniformData(size_t size) {
    if (!initialized_) {
        return {};
    }
    
    // Align size to uniform buffer offset alignment
    const VkDeviceSize alignment = 256; // Conservative uniform buffer alignment
    size_t alignedSize = (size + alignment - 1) & ~(alignment - 1);
    
    size_t& currentOffset = uniformOffsets_[currentFrameIndex_];
    
    // Check if we have enough space
    if (currentOffset + alignedSize > UNIFORM_BUFFER_SIZE) {
        g_frameAllocLogger.Error("Uniform buffer overflow for frame {}: requested {} bytes, {} bytes available",
            currentFrameIndex_, alignedSize, UNIFORM_BUFFER_SIZE - currentOffset);
        return {};
    }
    
    UniformAllocation result;
    result.data = static_cast<char*>(uniformMappedData_[currentFrameIndex_]) + currentOffset;
    result.buffer = uniformBuffers_[currentFrameIndex_];
    result.offset = currentOffset;
    result.size = alignedSize;
    
    currentOffset += alignedSize;
    
    g_frameAllocLogger.Debug("Allocated {} bytes from uniform buffer (frame {}, offset {})", 
        alignedSize, currentFrameIndex_, result.offset);
    
    return result;
}

PerFrameAllocator::FrameStats PerFrameAllocator::getFrameStats(uint32_t frameIndex) const {
    if (frameIndex >= FRAMES_IN_FLIGHT) return {};
    
    const auto& arena = frameArenas_[frameIndex];
    if (!arena) return {};
    
    const auto& arenaStats = arena->getStats();
    
    FrameStats stats;
    stats.totalAllocated = arena->getUsedSize();
    stats.peakUsage = arenaStats.peakUsage.load();
    stats.allocationCount = arenaStats.totalAllocations.load();
    stats.utilizationPercent = arena->getUtilization() * 100.0f;
    
    return stats;
}

PerFrameAllocator::FrameStats PerFrameAllocator::getCurrentFrameStats() const {
    return getFrameStats(currentFrameIndex_);
}

void PerFrameAllocator::logFrameStats() const {
    g_frameAllocLogger.Info("=== Per-Frame Allocator Statistics ===");
    
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        auto stats = getFrameStats(i);
        g_frameAllocLogger.Info("Frame {}: {:.1f} KB used ({:.1f}%), {} allocations, peak: {:.1f} KB",
            i, 
            stats.totalAllocated / 1024.0,
            stats.utilizationPercent,
            stats.allocationCount,
            stats.peakUsage / 1024.0);
    }
    
    // Uniform buffer usage
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        float uniformUtil = static_cast<float>(uniformOffsets_[i]) / UNIFORM_BUFFER_SIZE * 100.0f;
        g_frameAllocLogger.Info("Uniform Buffer {}: {:.1f} KB used ({:.1f}%)",
            i, uniformOffsets_[i] / 1024.0, uniformUtil);
    }
    
    g_frameAllocLogger.Info("=====================================");
}

bool PerFrameAllocator::createUniformBuffers(size_t bufferSize) {
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        std::string name = "FrameUniformBuffer_" + std::to_string(i);
        VMAAllocation allocation = MemoryManager::instance().createBuffer(
            bufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, MemoryCategory::UNIFORMS, name.c_str());
        
        if (allocation.allocation == VK_NULL_HANDLE) {
            g_frameAllocLogger.Error("Failed to create uniform buffer {}", i);
            return false;
        }
        
        // Map buffer persistently
        void* mappedData = MemoryManager::instance().map(allocation);
        if (!mappedData) {
            g_frameAllocLogger.Error("Failed to map uniform buffer {}", i);
            return false;
        }
        
        uniformBuffers_[i] = VK_NULL_HANDLE; // Buffer handle from allocation
        uniformAllocations_[i] = allocation;
        uniformMappedData_[i] = mappedData;
        uniformOffsets_[i] = 0;
    }
    
    return true;
}

void PerFrameAllocator::destroyUniformBuffers() {
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        if (uniformAllocations_[i].allocation != VK_NULL_HANDLE) {
            if (uniformMappedData_[i]) {
                MemoryManager::instance().unmap(uniformAllocations_[i]);
            }
            
            MemoryManager::instance().destroyBuffer(uniformBuffers_[i], uniformAllocations_[i]);
            
            uniformBuffers_[i] = VK_NULL_HANDLE;
            uniformAllocations_[i] = {};
            uniformMappedData_[i] = nullptr;
            uniformOffsets_[i] = 0;
        }
    }
}

} // namespace voxelvk