#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <array>
#include <atomic>
#include <mutex>
#include "memory_manager.hpp"

namespace voxelvk {

/**
 * Transient per-frame arena allocator for zero-GC frame loops
 */
class FrameArena {
public:
    FrameArena(size_t totalSize, const char* debugName);
    ~FrameArena();
    
    // Arena allocation (linear allocator)
    void* allocate(size_t size, size_t alignment = 16);
    
    // Reset arena for next frame (O(1) operation)
    void reset();
    
    // Query state
    size_t getTotalSize() const { return totalSize_; }
    size_t getUsedSize() const { return currentOffset_.load(); }
    size_t getRemainingSize() const { return totalSize_ - getUsedSize(); }
    float getUtilization() const { return static_cast<float>(getUsedSize()) / totalSize_; }
    
    // Statistics
    struct Stats {
        std::atomic<size_t> totalAllocations{0};
        std::atomic<size_t> peakUsage{0};
        std::atomic<size_t> resetCount{0};
        std::atomic<size_t> overflowCount{0};
    };
    
    const Stats& getStats() const { return stats_; }
    void resetStats() { stats_ = Stats{}; }
    
    bool isValid() const { return buffer_ != VK_NULL_HANDLE && mappedData_ != nullptr; }
    
private:
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VMAAllocation allocation_{};
    void* mappedData_ = nullptr;
    size_t totalSize_ = 0;
    std::atomic<size_t> currentOffset_{0};
    
    mutable Stats stats_{};
    const char* debugName_;
    
    void updatePeakUsage(size_t usage);
};

/**
 * Triple-buffered frame allocator system for 3 frames in flight
 */
class PerFrameAllocator {
public:
    static constexpr uint32_t FRAMES_IN_FLIGHT = 3;
    
    PerFrameAllocator();
    ~PerFrameAllocator();
    
    // Initialization
    bool initialize(size_t arenaSize = 16 * 1024 * 1024); // 16MB default per frame
    void shutdown();
    
    // Frame management
    void beginFrame(uint32_t frameIndex);
    void endFrame();
    
    // Current frame allocations (zero-GC)
    void* allocateTransient(size_t size, size_t alignment = 16);
    
    template<typename T>
    T* allocateTransient(size_t count = 1) {
        return static_cast<T*>(allocateTransient(sizeof(T) * count, alignof(T)));
    }
    
    // Uniform buffer helpers for frame data
    struct UniformAllocation {
        void* data;
        VkBuffer buffer;
        VkDeviceSize offset;
        VkDeviceSize size;
    };
    
    UniformAllocation allocateUniformData(size_t size);
    
    // Frame statistics
    struct FrameStats {
        size_t totalAllocated = 0;
        size_t peakUsage = 0;
        size_t allocationCount = 0;
        float utilizationPercent = 0.0f;
    };
    
    FrameStats getFrameStats(uint32_t frameIndex) const;
    FrameStats getCurrentFrameStats() const;
    
    void logFrameStats() const;
    
    // Global instance
    static PerFrameAllocator& instance() { return *s_instance; }
    static void setGlobalInstance(PerFrameAllocator* instance) { s_instance = instance; }
    
private:
    std::array<std::unique_ptr<FrameArena>, FRAMES_IN_FLIGHT> frameArenas_;
    uint32_t currentFrameIndex_ = 0;
    bool initialized_ = false;
    
    // Uniform buffer for frame data (one per frame)
    std::array<VkBuffer, FRAMES_IN_FLIGHT> uniformBuffers_{};
    std::array<VMAAllocation, FRAMES_IN_FLIGHT> uniformAllocations_{};
    std::array<void*, FRAMES_IN_FLIGHT> uniformMappedData_{};
    std::array<size_t, FRAMES_IN_FLIGHT> uniformOffsets_{};
    
    static constexpr size_t UNIFORM_BUFFER_SIZE = 4 * 1024 * 1024; // 4MB per frame
    
    static PerFrameAllocator* s_instance;
    
    bool createUniformBuffers(size_t bufferSize);
    void destroyUniformBuffers();
};

/**
 * RAII helper for transient allocations
 */
template<typename T>
class TransientArray {
public:
    TransientArray(size_t count) 
        : data_(PerFrameAllocator::instance().template allocateTransient<T>(count))
        , count_(count) {}
    
    T* data() { return data_; }
    const T* data() const { return data_; }
    size_t size() const { return count_; }
    
    T& operator[](size_t index) { return data_[index]; }
    const T& operator[](size_t index) const { return data_[index]; }
    
    // Iterator support
    T* begin() { return data_; }
    T* end() { return data_ + count_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + count_; }
    
private:
    T* data_;
    size_t count_;
};

/**
 * Convenience macros for frame allocations
 */
#define ALLOC_FRAME(type, count) PerFrameAllocator::instance().allocateTransient<type>(count)
#define ALLOC_FRAME_ARRAY(type, count) TransientArray<type>(count)
#define ALLOC_FRAME_UNIFORM(size) PerFrameAllocator::instance().allocateUniformData(size)

} // namespace voxelvk