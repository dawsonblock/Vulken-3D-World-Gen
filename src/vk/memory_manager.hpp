#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>
#include <atomic>
#include "device_caps.hpp"

namespace voxelvk {

/**
 * VRAM Budget Categories for organized memory management
 */
enum class MemoryCategory {
    GEOMETRY,       // Vertex buffers, index buffers, mesh data
    TEXTURES,       // Diffuse, normal, material textures
    RENDER_TARGETS, // Framebuffers, depth buffers, intermediate textures
    UNIFORMS,       // UBOs, SSBOs, descriptor sets
    STAGING,        // Upload/download staging buffers
    WEATHER,        // Weather-specific resources (clouds, precipitation)
    COMPUTE,        // Compute shader resources
    CACHE           // Pipeline cache, shader modules
};

/**
 * Memory budget configuration for different quality levels
 */
struct MemoryBudgetConfig {
    // Total VRAM budget (MB)
    size_t totalBudgetMB = 2560;  // 2.5GB default for 1080p
    
    // Per-category budgets (percentages of total)
    float geometryPercent = 0.25f;      // 640MB - vertex/index data
    float texturesPercent = 0.40f;      // 1024MB - texture assets
    float renderTargetsPercent = 0.15f; // 384MB - framebuffers
    float uniformsPercent = 0.05f;      // 128MB - uniform data
    float stagingPercent = 0.05f;       // 128MB - upload staging
    float weatherPercent = 0.05f;       // 128MB - weather effects
    float computePercent = 0.03f;       // 77MB - compute resources
    float cachePercent = 0.02f;         // 51MB - pipeline cache
    
    // Quality presets
    static MemoryBudgetConfig Low()    { return {1536, 0.30f, 0.35f, 0.15f, 0.08f, 0.05f, 0.03f, 0.02f, 0.02f}; }  // 1.5GB
    static MemoryBudgetConfig Medium() { return {2560, 0.25f, 0.40f, 0.15f, 0.05f, 0.05f, 0.05f, 0.03f, 0.02f}; }  // 2.5GB
    static MemoryBudgetConfig High()   { return {4096, 0.20f, 0.45f, 0.15f, 0.05f, 0.05f, 0.05f, 0.03f, 0.02f}; }  // 4GB
    static MemoryBudgetConfig Ultra()  { return {6144, 0.18f, 0.50f, 0.15f, 0.04f, 0.04f, 0.05f, 0.02f, 0.02f}; }  // 6GB
    
    size_t getCategoryBudget(MemoryCategory category) const;
    void logBudgets() const;
};

/**
 * Memory allocation tracking per category
 */
struct MemoryStats {
    std::atomic<size_t> bytesAllocated{0};
    std::atomic<size_t> bytesFreed{0};
    std::atomic<size_t> allocationCount{0};
    std::atomic<size_t> deallocationCount{0};
    std::atomic<size_t> peakUsage{0};
    std::atomic<size_t> currentUsage{0};
    
    void recordAllocation(size_t bytes);
    void recordDeallocation(size_t bytes);
    void updatePeakUsage(size_t current);
    double getFragmentationRatio() const;
};

/**
 * VMA-based memory allocator with budget enforcement
 */
struct VMAAllocation {
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo info{};
    MemoryCategory category = MemoryCategory::GEOMETRY;
    size_t size = 0;
    const char* debugName = nullptr;
    
    void* getMappedData() const { return info.pMappedData; }
    VkDeviceSize getOffset() const { return info.offset; }
    VkDeviceMemory getMemory() const { return info.deviceMemory; }
};

/**
 * Professional GPU memory manager with VMA backend
 */
class MemoryManager {
public:
    MemoryManager(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, const DeviceCaps& caps);
    ~MemoryManager();
    
    // Initialization
    bool initialize(const MemoryBudgetConfig& config = MemoryBudgetConfig::Medium());
    void shutdown();
    
    // Buffer allocation
    VMAAllocation createBuffer(
        const VkBufferCreateInfo& bufferInfo,
        VmaMemoryUsage memoryUsage,
        MemoryCategory category,
        const char* debugName = nullptr
    );
    
    // Image allocation
    VMAAllocation createImage(
        const VkImageCreateInfo& imageInfo,
        VmaMemoryUsage memoryUsage,
        MemoryCategory category,
        const char* debugName = nullptr
    );
    
    // Deallocation
    void destroyBuffer(VkBuffer buffer, const VMAAllocation& allocation);
    void destroyImage(VkImage image, const VMAAllocation& allocation);
    
    // Memory mapping
    void* map(const VMAAllocation& allocation);
    void unmap(const VMAAllocation& allocation);
    void flush(const VMAAllocation& allocation, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    
    // Budget management
    bool isWithinBudget(MemoryCategory category, size_t additionalBytes = 0) const;
    bool enforceMemoryPressure(); // Returns true if pressure relief successful
    void setEvictionCallback(MemoryCategory category, std::function<size_t()> callback);
    
    // Statistics and monitoring
    const MemoryStats& getStats(MemoryCategory category) const;
    size_t getTotalUsage() const;
    size_t getBudgetUsage(MemoryCategory category) const;
    float getBudgetUtilization(MemoryCategory category) const;
    
    // VMA statistics
    VmaBudget getVMABudget() const;
    VmaStatInfo getVMAStats() const;
    
    // Debugging and profiling
    void logMemoryReport() const;
    void dumpMemoryState(const std::string& filename) const;
    
    // Global instance (for convenience)
    static MemoryManager& instance() { return *s_instance; }
    static void setGlobalInstance(MemoryManager* instance) { s_instance = instance; }
    
private:
    VkInstance instance_;
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    const DeviceCaps& deviceCaps_;
    
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    MemoryBudgetConfig config_;
    
    // Per-category tracking
    std::array<MemoryStats, 8> categoryStats_; // One per MemoryCategory
    std::array<std::function<size_t()>, 8> evictionCallbacks_;
    
    // Allocation tracking
    std::unordered_map<VkBuffer, VMAAllocation> bufferAllocations_;
    std::unordered_map<VkImage, VMAAllocation> imageAllocations_;
    mutable std::mutex allocationsMutex_;
    
    static MemoryManager* s_instance;
    
    // Internal methods
    bool checkBudgetConstraint(MemoryCategory category, size_t bytes);
    void updateCategoryStats(MemoryCategory category, size_t bytes, bool isAllocation);
    size_t getCategoryBudgetBytes(MemoryCategory category) const;
    const char* categoryToString(MemoryCategory category) const;
    
    VmaAllocationCreateInfo createAllocationInfo(VmaMemoryUsage usage, MemoryCategory category);
    void labelVMAAllocation(const VMAAllocation& allocation, const char* name);
};

/**
 * Helper functions for common allocation patterns
 */
namespace memory {
    // Vertex buffer creation
    VMAAllocation createVertexBuffer(const void* data, size_t size, const char* name = "VertexBuffer");
    VMAAllocation createIndexBuffer(const void* data, size_t size, const char* name = "IndexBuffer");
    
    // Uniform buffer creation (per-frame ring)
    VMAAllocation createUniformBuffer(size_t size, const char* name = "UniformBuffer");
    VMAAllocation createDynamicUniformBuffer(size_t size, const char* name = "DynamicUniformBuffer");
    
    // Texture creation
    VMAAllocation createTexture2D(uint32_t width, uint32_t height, VkFormat format, 
                                  VkImageUsageFlags usage, const char* name = "Texture2D");
    VMAAllocation createRenderTarget(uint32_t width, uint32_t height, VkFormat format,
                                     const char* name = "RenderTarget");
    
    // Staging buffer for uploads
    VMAAllocation createStagingBuffer(size_t size, const char* name = "StagingBuffer");
    
    // Weather-specific allocations
    VMAAllocation createWeatherBuffer(size_t size, const char* name = "WeatherBuffer");
    VMAAllocation createParticleBuffer(size_t particleCount, const char* name = "ParticleBuffer");
}

/**
 * RAII memory allocation helper
 */
template<typename HandleType>
class MemoryResource {
public:
    MemoryResource() = default;
    MemoryResource(HandleType handle, VMAAllocation allocation) 
        : handle_(handle), allocation_(allocation), valid_(true) {}
    
    ~MemoryResource() { reset(); }
    
    // Move semantics
    MemoryResource(MemoryResource&& other) noexcept 
        : handle_(other.handle_), allocation_(other.allocation_), valid_(other.valid_) {
        other.valid_ = false;
    }
    
    MemoryResource& operator=(MemoryResource&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = other.handle_;
            allocation_ = other.allocation_;
            valid_ = other.valid_;
            other.valid_ = false;
        }
        return *this;
    }
    
    // No copy semantics
    MemoryResource(const MemoryResource&) = delete;
    MemoryResource& operator=(const MemoryResource&) = delete;
    
    HandleType get() const { return handle_; }
    const VMAAllocation& getAllocation() const { return allocation_; }
    bool isValid() const { return valid_; }
    
    void reset();
    
private:
    HandleType handle_{};
    VMAAllocation allocation_{};
    bool valid_ = false;
};

using BufferResource = MemoryResource<VkBuffer>;
using ImageResource = MemoryResource<VkImage>;

} // namespace voxelvk