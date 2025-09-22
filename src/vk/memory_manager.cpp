#include "memory_manager.hpp"
#include "error_handling.hpp"
#include "../core/logger.hpp"
#include <algorithm>
#include <fstream>
#include <cstring>

namespace voxelvk {

static Logger g_memLogger("MemoryManager");

MemoryManager* MemoryManager::s_instance = nullptr;

size_t MemoryBudgetConfig::getCategoryBudget(MemoryCategory category) const {
    switch (category) {
        case MemoryCategory::GEOMETRY:       return static_cast<size_t>(totalBudgetMB * geometryPercent * 1024 * 1024);
        case MemoryCategory::TEXTURES:       return static_cast<size_t>(totalBudgetMB * texturesPercent * 1024 * 1024);
        case MemoryCategory::RENDER_TARGETS: return static_cast<size_t>(totalBudgetMB * renderTargetsPercent * 1024 * 1024);
        case MemoryCategory::UNIFORMS:       return static_cast<size_t>(totalBudgetMB * uniformsPercent * 1024 * 1024);
        case MemoryCategory::STAGING:        return static_cast<size_t>(totalBudgetMB * stagingPercent * 1024 * 1024);
        case MemoryCategory::WEATHER:        return static_cast<size_t>(totalBudgetMB * weatherPercent * 1024 * 1024);
        case MemoryCategory::COMPUTE:        return static_cast<size_t>(totalBudgetMB * computePercent * 1024 * 1024);
        case MemoryCategory::CACHE:          return static_cast<size_t>(totalBudgetMB * cachePercent * 1024 * 1024);
        default: return 0;
    }
}

void MemoryBudgetConfig::logBudgets() const {
    g_memLogger.Info("=== Memory Budget Configuration ===");
    g_memLogger.Info("Total Budget: {:.1f} MB", totalBudgetMB);
    g_memLogger.Info("  Geometry: {:.1f} MB ({:.1f}%)", getCategoryBudget(MemoryCategory::GEOMETRY) / (1024.0*1024.0), geometryPercent * 100);
    g_memLogger.Info("  Textures: {:.1f} MB ({:.1f}%)", getCategoryBudget(MemoryCategory::TEXTURES) / (1024.0*1024.0), texturesPercent * 100);
    g_memLogger.Info("  Render Targets: {:.1f} MB ({:.1f}%)", getCategoryBudget(MemoryCategory::RENDER_TARGETS) / (1024.0*1024.0), renderTargetsPercent * 100);
    g_memLogger.Info("  Uniforms: {:.1f} MB ({:.1f}%)", getCategoryBudget(MemoryCategory::UNIFORMS) / (1024.0*1024.0), uniformsPercent * 100);
    g_memLogger.Info("  Weather: {:.1f} MB ({:.1f}%)", getCategoryBudget(MemoryCategory::WEATHER) / (1024.0*1024.0), weatherPercent * 100);
    g_memLogger.Info("================================");
}

void MemoryStats::recordAllocation(size_t bytes) {
    bytesAllocated.fetch_add(bytes);
    allocationCount.fetch_add(1);
    size_t newUsage = currentUsage.fetch_add(bytes) + bytes;
    updatePeakUsage(newUsage);
}

void MemoryStats::recordDeallocation(size_t bytes) {
    bytesFreed.fetch_add(bytes);
    deallocationCount.fetch_add(1);
    currentUsage.fetch_sub(bytes);
}

void MemoryStats::updatePeakUsage(size_t current) {
    size_t peak = peakUsage.load();
    while (current > peak && !peakUsage.compare_exchange_weak(peak, current)) {
        // Retry if another thread updated peak
    }
}

double MemoryStats::getFragmentationRatio() const {
    size_t allocated = bytesAllocated.load();
    size_t freed = bytesFreed.load();
    if (allocated == 0) return 0.0;
    return static_cast<double>(freed) / allocated;
}

MemoryManager::MemoryManager(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, const DeviceCaps& caps)
    : instance_(instance), device_(device), physicalDevice_(physicalDevice), deviceCaps_(caps) {

    g_memLogger.Info("MemoryManager initializing with VMA backend");
}

MemoryManager::~MemoryManager() {
    shutdown();
}

bool MemoryManager::initialize(const MemoryBudgetConfig& config) {
    g_memLogger.Info("Initializing VMA-based memory management");

    config_ = config;
    config_.logBudgets();

    // VMA allocator setup
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.instance = instance_;
    allocatorInfo.device = device_;
    allocatorInfo.physicalDevice = physicalDevice_;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;

    // Enable budget tracking
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

    // Enable buffer device address if available
    if (deviceCaps_.hasBufferDeviceAddress) {
        allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        g_memLogger.Info("Buffer device address enabled");
    }

    VkResult result = vmaCreateAllocator(&allocatorInfo, &allocator_);
    if (result != VK_SUCCESS) {
        // CHECK_VK_OBJECT(result, VkErrorCategory::MEMORY_ALLOCATION, "vma_allocator");
        return false;
    }

    // Initialize category stats
    for (auto& stats : categoryStats_) {
        stats.bytesAllocated.store(0);
        stats.bytesFreed.store(0);
        stats.allocationCount.store(0);
        stats.deallocationCount.store(0);
        stats.peakUsage.store(0);
        stats.currentUsage.store(0);
    }

    // Set global instance
    s_instance = this;

    g_memLogger.Info("VMA allocator initialized successfully");
    g_memLogger.Info("Available device memory: {:.1f} MB",
        deviceCaps_.deviceLocalHeapSize / (1024.0 * 1024.0));

    return true;
}

void MemoryManager::shutdown() {
    if (allocator_ == VK_NULL_HANDLE) return;

    g_memLogger.Info("Shutting down VMA memory manager");

    // Log final memory report
    logMemoryReport();

    // Check for memory leaks
    std::lock_guard<std::mutex> lock(allocationsMutex_);

    if (!bufferAllocations_.empty()) {
        g_memLogger.Warn("Memory leak: {} buffer allocations not cleaned up", bufferAllocations_.size());
    }

    if (!imageAllocations_.empty()) {
        g_memLogger.Warn("Memory leak: {} image allocations not cleaned up", imageAllocations_.size());
    }

    // Destroy VMA allocator
    vmaDestroyAllocator(allocator_);
    allocator_ = VK_NULL_HANDLE;

    s_instance = nullptr;
}

BufferResult MemoryManager::createBuffer(
    const VkBufferCreateInfo& bufferInfo,
    VmaMemoryUsage memoryUsage,
    MemoryCategory category,
    const char* debugName) {

    // Check budget constraint
    if (!checkBudgetConstraint(category, bufferInfo.size)) {
        g_memLogger.Warn("Buffer allocation would exceed budget for category: {}", categoryToString(category));

        // Attempt memory pressure relief
        if (enforceMemoryPressure()) {
            g_memLogger.Info("Memory pressure relief successful, retrying allocation");
        } else {
            g_memLogger.Error("Failed to relieve memory pressure, allocation denied");
            return {};
        }
    }

    VmaAllocationCreateInfo allocInfo = createAllocationInfo(memoryUsage, category);

    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;

    VkResult result = vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo, &buffer, &allocation, &allocationInfo);
    if (result != VK_SUCCESS) {
        // CHECK_VK_OBJECT(result, VkErrorCategory::MEMORY_ALLOCATION, debugName ? debugName : "buffer");
        return {};
    }

    // Create allocation wrapper
    VMAAllocation wrapper;
    wrapper.allocation = allocation;
    wrapper.info = allocationInfo;
    wrapper.category = category;
    wrapper.size = bufferInfo.size;
    wrapper.debugName = debugName;

    // Track allocation
    {
        std::lock_guard<std::mutex> lock(allocationsMutex_);
        bufferAllocations_[buffer] = wrapper;
    }

    // Update statistics
    updateCategoryStats(category, bufferInfo.size, true);

    // Set debug name if available
    if (debugName) {
        VK_OBJECT_NAME(device_, buffer, VK_OBJECT_TYPE_BUFFER, debugName);
    }

    g_memLogger.Debug("Created buffer: {} bytes, category: {}", bufferInfo.size, categoryToString(category));

    BufferResult resultStruct;
    resultStruct.buffer = buffer;
    resultStruct.allocation = wrapper;
    return resultStruct;
}

ImageResult MemoryManager::createImage(
    const VkImageCreateInfo& imageInfo,
    VmaMemoryUsage memoryUsage,
    MemoryCategory category,
    const char* debugName) {

    VmaAllocationCreateInfo allocInfo = createAllocationInfo(memoryUsage, category);

    VkImage image;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;

    VkResult result = vmaCreateImage(allocator_, &imageInfo, &allocInfo, &image, &allocation, &allocationInfo);
    if (result != VK_SUCCESS) {
        // CHECK_VK_OBJECT(result, VkErrorCategory::MEMORY_ALLOCATION, debugName ? debugName : "image");
        return {};
    }

    // Create allocation wrapper
    VMAAllocation wrapper;
    wrapper.allocation = allocation;
    wrapper.info = allocationInfo;
    wrapper.category = category;
    wrapper.size = allocationInfo.size;
    wrapper.debugName = debugName;

    // Track allocation
    {
        std::lock_guard<std::mutex> lock(allocationsMutex_);
        imageAllocations_[image] = wrapper;
    }

    // Update statistics
    updateCategoryStats(category, allocationInfo.size, true);

    // Set debug name if available
    if (debugName) {
        VK_OBJECT_NAME(device_, image, VK_OBJECT_TYPE_IMAGE, debugName);
    }

    g_memLogger.Debug("Created image: {} bytes, category: {}", allocationInfo.size, categoryToString(category));

    ImageResult resultStruct;
    resultStruct.image = image;
    resultStruct.allocation = wrapper;
    return resultStruct;
}

void MemoryManager::destroyBuffer(VkBuffer buffer, const VMAAllocation& allocation) {
    if (buffer == VK_NULL_HANDLE || allocation.allocation == VK_NULL_HANDLE) return;

    // Remove from tracking
    {
        std::lock_guard<std::mutex> lock(allocationsMutex_);
        auto it = bufferAllocations_.find(buffer);
        if (it != bufferAllocations_.end()) {
            updateCategoryStats(it->second.category, it->second.size, false);
            bufferAllocations_.erase(it);
        }
    }

    // Destroy VMA allocation
    vmaDestroyBuffer(allocator_, buffer, allocation.allocation);

    g_memLogger.Debug("Destroyed buffer: {} bytes", allocation.size);
}

void MemoryManager::destroyImage(VkImage image, const VMAAllocation& allocation) {
    if (image == VK_NULL_HANDLE || allocation.allocation == VK_NULL_HANDLE) return;

    // Remove from tracking
    {
        std::lock_guard<std::mutex> lock(allocationsMutex_);
        auto it = imageAllocations_.find(image);
        if (it != imageAllocations_.end()) {
            updateCategoryStats(it->second.category, it->second.size, false);
            imageAllocations_.erase(it);
        }
    }

    // Destroy VMA allocation
    vmaDestroyImage(allocator_, image, allocation.allocation);

    g_memLogger.Debug("Destroyed image: {} bytes", allocation.size);
}

bool MemoryManager::isWithinBudget(MemoryCategory category, size_t additionalBytes) const {
    size_t currentUsage = categoryStats_[static_cast<size_t>(static_cast<int>(category))].currentUsage.load();
    size_t budget = getCategoryBudgetBytes(category);
    return (currentUsage + additionalBytes) <= budget;
}

bool MemoryManager::enforceMemoryPressure() {
    g_memLogger.Info("Enforcing memory pressure relief");

    size_t bytesFreed = 0;

    // Try eviction callbacks for each category
    for (int i = 0; i < static_cast<int>(evictionCallbacks_.size()); i++) {
        if (evictionCallbacks_[static_cast<size_t>(i)]) {
            size_t freed = evictionCallbacks_[static_cast<size_t>(i)]();
            bytesFreed += freed;

            if (freed > 0) {
                g_memLogger.Info("Eviction callback for {} freed {} bytes",
                    categoryToString(static_cast<MemoryCategory>(i)), freed);
            }
        }
    }

    if (bytesFreed > 0) {
        g_memLogger.Info("Memory pressure relief successful: {} bytes freed", bytesFreed);
        return true;
    }

    g_memLogger.Warn("Memory pressure relief failed: no bytes freed");
    return false;
}

void MemoryManager::setEvictionCallback(MemoryCategory category, std::function<size_t()> callback) {
    evictionCallbacks_[static_cast<size_t>(static_cast<int>(category))] = callback;
}

const MemoryStats& MemoryManager::getStats(MemoryCategory category) const {
    return categoryStats_[static_cast<size_t>(static_cast<int>(category))];
}

size_t MemoryManager::getTotalUsage() const {
    size_t total = 0;
    for (const auto& stats : categoryStats_) {
        total += stats.currentUsage.load();
    }
    return total;
}

float MemoryManager::getBudgetUtilization(MemoryCategory category) const {
    size_t usage = categoryStats_[static_cast<size_t>(static_cast<int>(category))].currentUsage.load();
    size_t budget = getCategoryBudgetBytes(category);
    if (budget == 0) return 0.0f;
    return static_cast<float>(usage) / budget;
}

size_t MemoryManager::getBudgetUsage(MemoryCategory category) const {
    return categoryStats_[static_cast<size_t>(static_cast<int>(category))].currentUsage.load();
}

VmaBudget MemoryManager::getVMABudget() const {
    VmaBudget budget{};
    // vmaGetBudget(allocator_, &budget); // Commented out - may not be available in this VMA version
    return budget;
}

// getVMAStats removed: not available in this VMA configuration

void MemoryManager::logMemoryReport() const {
    g_memLogger.Info("=== Memory Usage Report ===");

    size_t totalUsage = getTotalUsage();
    g_memLogger.Info("Total Usage: {:.1f} MB / {:.1f} MB ({:.1f}%)",
        totalUsage / (1024.0*1024.0),
        config_.totalBudgetMB,
        static_cast<float>((totalUsage / (1024.0*1024.0)) / static_cast<double>(config_.totalBudgetMB) * 100.0));

    const char* categoryNames[] = {
        "Geometry", "Textures", "RenderTargets", "Uniforms",
        "Staging", "Weather", "Compute", "Cache"
    };

    for (int i = 0; i < 8; i++) {
        const auto& stats = categoryStats_[static_cast<size_t>(i)];
        auto category = static_cast<MemoryCategory>(i);
        size_t usage = stats.currentUsage.load();
        size_t budget = getCategoryBudgetBytes(category);
        float utilization = getBudgetUtilization(category);

        g_memLogger.Info("  {}: {:.1f} MB / {:.1f} MB ({:.1f}%) - {} allocs",
            categoryNames[i],
            usage / (1024.0*1024.0),
            budget / (1024.0*1024.0),
            utilization * 100.0f,
            stats.allocationCount.load());
    }

    // VMA statistics
    // VmaStatInfo vmaStats = getVMAStats();
    // g_memLogger.Info("VMA Stats:");
    // g_memLogger.Info("  Blocks: {}, Allocations: {}", vmaStats.blockCount, vmaStats.allocationCount);
    // g_memLogger.Info("  Used: {:.1f} MB, Unused: {:.1f} MB",
    //     vmaStats.usedBytes / (1024.0*1024.0),
    //     vmaStats.unusedBytes / (1024.0*1024.0));

    g_memLogger.Info("===========================");
}

void* MemoryManager::map(const VMAAllocation& allocation) {
    if (allocation.allocation == VK_NULL_HANDLE) return nullptr;
    void* data = nullptr;
    VkResult res = vmaMapMemory(allocator_, allocation.allocation, &data);
    if (res != VK_SUCCESS) {
        return nullptr;
    }
    return data;
}

void MemoryManager::unmap(const VMAAllocation& allocation) {
    if (allocation.allocation == VK_NULL_HANDLE) return;
    vmaUnmapMemory(allocator_, allocation.allocation);
}

void MemoryManager::flush(const VMAAllocation& allocation, VkDeviceSize offset, VkDeviceSize size) {
    if (allocation.allocation == VK_NULL_HANDLE) return;
    vmaFlushAllocation(allocator_, allocation.allocation, offset, size);
}

void MemoryManager::dumpMemoryState(const std::string& filename) const {
    try {
        std::ofstream ofs(filename);
        if (!ofs) return;
        ofs << "TotalUsageBytes " << getTotalUsage() << "\n";
        for (int i = 0; i < 8; ++i) {
            auto cat = static_cast<MemoryCategory>(i);
            ofs << categoryToString(cat) << " " << categoryStats_[static_cast<size_t>(i)].currentUsage.load() << "\n";
        }
    } catch (...) {
        // ignore
    }
}

bool MemoryManager::checkBudgetConstraint(MemoryCategory category, size_t bytes) {
    size_t currentUsage = categoryStats_[static_cast<size_t>(static_cast<int>(category))].currentUsage.load();
    size_t budget = getCategoryBudgetBytes(category);

    if (currentUsage + bytes > budget) {
        float currentUtil = static_cast<float>(currentUsage) / budget * 100.0f;
        float newUtil = static_cast<float>(currentUsage + bytes) / budget * 100.0f;

        g_memLogger.Warn("Budget constraint violation for {}: {:.1f}% -> {:.1f}% (limit: 100%)",
            categoryToString(category), currentUtil, newUtil);
        return false;
    }

    return true;
}

void MemoryManager::updateCategoryStats(MemoryCategory category, size_t bytes, bool isAllocation) {
    int categoryIndex = static_cast<int>(category);
    if (isAllocation) {
        categoryStats_[static_cast<size_t>(categoryIndex)].recordAllocation(bytes);
    } else {
        categoryStats_[static_cast<size_t>(categoryIndex)].recordDeallocation(bytes);
    }
}

size_t MemoryManager::getCategoryBudgetBytes(MemoryCategory category) const {
    return config_.getCategoryBudget(category);
}

const char* MemoryManager::categoryToString(MemoryCategory category) const {
    switch (category) {
        case MemoryCategory::GEOMETRY: return "GEOMETRY";
        case MemoryCategory::TEXTURES: return "TEXTURES";
        case MemoryCategory::RENDER_TARGETS: return "RENDER_TARGETS";
        case MemoryCategory::UNIFORMS: return "UNIFORMS";
        case MemoryCategory::STAGING: return "STAGING";
        case MemoryCategory::WEATHER: return "WEATHER";
        case MemoryCategory::COMPUTE: return "COMPUTE";
        case MemoryCategory::CACHE: return "CACHE";
        default: return "UNKNOWN";
    }
}

VmaAllocationCreateInfo MemoryManager::createAllocationInfo(VmaMemoryUsage usage, MemoryCategory category) {
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = usage;

    // Category-specific allocation preferences
    switch (category) {
        case MemoryCategory::STAGING:
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            break;

        case MemoryCategory::UNIFORMS:
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                             VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;

        case MemoryCategory::GEOMETRY:
        case MemoryCategory::TEXTURES:
        case MemoryCategory::RENDER_TARGETS:
        case MemoryCategory::WEATHER:
        case MemoryCategory::COMPUTE:
            allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            break;

        case MemoryCategory::CACHE:
            // Cache can be less aggressive
            break;
    }

    return allocInfo;
}

// Helper function implementations
namespace memory {

BufferResult createVertexBuffer(const void* data, size_t size, const char* name) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    auto result = MemoryManager::instance().createBuffer(
        bufferInfo, VMA_MEMORY_USAGE_GPU_ONLY, MemoryCategory::GEOMETRY, name);

    if (data && result.isValid()) {
        // Note: Upload via staging buffer requires queue and command pool
        // This will be handled by the caller using upload_to_buffer
        g_memLogger.Debug("Vertex buffer created, upload via upload_to_buffer()");
    }

    return result;
}

BufferResult createIndexBuffer(const void* data, size_t size, const char* name) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    auto result = MemoryManager::instance().createBuffer(
        bufferInfo, VMA_MEMORY_USAGE_GPU_ONLY, MemoryCategory::GEOMETRY, name);

    if (data && result.isValid()) {
        // Note: Upload via staging buffer requires queue and command pool
        // This will be handled by the caller using upload_to_buffer
        g_memLogger.Debug("Index buffer created, upload via upload_to_buffer()");
    }

    return result;
}

BufferResult createUniformBuffer(size_t size, const char* name) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    return MemoryManager::instance().createBuffer(
        bufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, MemoryCategory::UNIFORMS, name);
}

ImageResult createTexture2D(uint32_t width, uint32_t height, VkFormat format,
                             VkImageUsageFlags usage, const char* name) {
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

    return MemoryManager::instance().createImage(
        imageInfo, VMA_MEMORY_USAGE_GPU_ONLY, MemoryCategory::TEXTURES, name);
}

BufferResult createWeatherBuffer(size_t size, const char* name) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    return MemoryManager::instance().createBuffer(
        bufferInfo, VMA_MEMORY_USAGE_GPU_ONLY, MemoryCategory::WEATHER, name);
}

BufferResult create_device_buffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, const char* name) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    return MemoryManager::instance().createBuffer(
        bufferInfo, memoryUsage, MemoryCategory::STAGING, name);
}

void upload_to_buffer(BufferResult dst, std::span<const std::byte> data, VkQueue queue, VkCommandPool pool) {
    if (!dst.isValid() || data.empty()) {
        g_memLogger.Warn("Invalid destination buffer or empty data for upload");
        return;
    }

    auto& memMgr = MemoryManager::instance();

    // Create staging buffer
    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = data.size();
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    auto stagingResult = memMgr.createBuffer(
        stagingInfo, VMA_MEMORY_USAGE_CPU_ONLY, MemoryCategory::STAGING, "StagingUpload");

    if (!stagingResult.isValid()) {
        g_memLogger.Error("Failed to create staging buffer for upload");
        return;
    }

    // Map and copy data
    void* mapped = memMgr.map(stagingResult.allocation);
    if (!mapped) {
        g_memLogger.Error("Failed to map staging buffer");
        memMgr.destroyBuffer(stagingResult.buffer, stagingResult.allocation);
        return;
    }

    std::memcpy(mapped, data.data(), data.size());
    memMgr.unmap(stagingResult.allocation);

    // Record copy command
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    VkResult result = vkAllocateCommandBuffers(memMgr.device_, &allocInfo, &cmdBuffer);
    if (result != VK_SUCCESS) {
        g_memLogger.Error("Failed to allocate command buffer for staging upload");
        memMgr.destroyBuffer(stagingResult.buffer, stagingResult.allocation);
        return;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = data.size();
    vkCmdCopyBuffer(cmdBuffer, stagingResult.buffer, dst.buffer, 1, &copyRegion);

    vkEndCommandBuffer(cmdBuffer);

    // Submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VkFence fence;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(memMgr.device_, &fenceInfo, nullptr, &fence);

    vkQueueSubmit(queue, 1, &submitInfo, fence);
    vkWaitForFences(memMgr.device_, 1, &fence, VK_TRUE, UINT64_MAX);

    // Cleanup
    vkDestroyFence(memMgr.device_, fence, nullptr);
    vkFreeCommandBuffers(memMgr.device_, pool, 1, &cmdBuffer);
    memMgr.destroyBuffer(stagingResult.buffer, stagingResult.allocation);

    g_memLogger.Debug("Successfully uploaded {} bytes to device buffer", data.size());
}

} // namespace memory

// Template specializations for RAII helpers
template<>
void MemoryResource<VkBuffer>::reset() {
    if (valid_ && handle_ != VK_NULL_HANDLE) {
        MemoryManager::instance().destroyBuffer(handle_, allocation_);
        handle_ = VK_NULL_HANDLE;
        valid_ = false;
    }
}

template<>
void MemoryResource<VkImage>::reset() {
    if (valid_ && handle_ != VK_NULL_HANDLE) {
        MemoryManager::instance().destroyImage(handle_, allocation_);
        handle_ = VK_NULL_HANDLE;
        valid_ = false;
    }
}

} // namespace voxelvk
