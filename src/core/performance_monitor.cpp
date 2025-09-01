#include "performance_monitor.hpp"
#include "logger.hpp"
#include "nvtx_profiler.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <cmath>

namespace voxelvk {

static Logger g_perfLogger("PerformanceMonitor");

PerformanceMonitor* PerformanceMonitor::s_instance = nullptr;

double PerformanceSample::calculateTimeMs(float timestampPeriod) const {
    if (!isValid()) return 0.0;
    return (timestampEnd - timestampBegin) * timestampPeriod / 1000000.0;
}

// PerformanceBudgetTracker implementation
PerformanceBudgetTracker::PerformanceBudgetTracker() {
    for (auto& stats : categoryStats_) {
        stats = CategoryStats{};
        stats.samples.reserve(CategoryStats::MAX_SAMPLES);
    }
}

void PerformanceBudgetTracker::recordSample(PerformanceBudget category, double timeMs, uint32_t frameIndex) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    int categoryIndex = static_cast<int>(category);
    auto& stats = categoryStats_[categoryIndex];
    
    // Add sample to circular buffer
    if (stats.samples.size() >= CategoryStats::MAX_SAMPLES) {
        stats.samples.erase(stats.samples.begin());
    }
    stats.samples.push_back(timeMs);
    
    // Update running statistics
    stats.sampleCount++;
    double alpha = 1.0 / std::min(100.0, static_cast<double>(stats.sampleCount)); // Running average
    stats.runningAverage = stats.runningAverage * (1.0 - alpha) + timeMs * alpha;
    stats.maxTime = std::max(stats.maxTime, timeMs);
    
    // Check budget violation
    double budgetLimit = getBudgetLimit(category);
    if (timeMs > budgetLimit) {
        stats.violationCount++;
    }
}

bool PerformanceBudgetTracker::isWithinBudget(PerformanceBudget category) const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    int categoryIndex = static_cast<int>(category);
    const auto& stats = categoryStats_[categoryIndex];
    
    if (stats.sampleCount == 0) return true;
    
    double budgetLimit = getBudgetLimit(category);
    return stats.runningAverage <= budgetLimit;
}

bool PerformanceBudgetTracker::isFrameWithinBudget() const {
    return isWithinBudget(PerformanceBudget::FRAME_TOTAL);
}

double PerformanceBudgetTracker::getAverageTime(PerformanceBudget category) const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return categoryStats_[static_cast<int>(category)].runningAverage;
}

double PerformanceBudgetTracker::getP95Time(PerformanceBudget category) const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    int categoryIndex = static_cast<int>(category);
    const auto& stats = categoryStats_[categoryIndex];
    
    return calculateP95(stats.samples);
}

double PerformanceBudgetTracker::calculateP95(const std::vector<double>& samples) const {
    if (samples.empty()) return 0.0;
    
    std::vector<double> sortedSamples = samples;
    std::sort(sortedSamples.begin(), sortedSamples.end());
    
    size_t p95Index = static_cast<size_t>(sortedSamples.size() * 0.95);
    if (p95Index >= sortedSamples.size()) {
        p95Index = sortedSamples.size() - 1;
    }
    
    return sortedSamples[p95Index];
}

bool PerformanceBudgetTracker::passesPerformanceGates() const {
    // Check P95 frame time
    double frameP95 = getP95Time(PerformanceBudget::FRAME_TOTAL);
    if (frameP95 > config_.maxFrameTimeMs) {
        g_perfLogger.Error("Performance gate FAILED: Frame P95 time {:.2f}ms > {:.2f}ms", 
            frameP95, config_.maxFrameTimeMs);
        return false;
    }
    
    // Check average frame time
    double frameAvg = getAverageTime(PerformanceBudget::FRAME_TOTAL);
    if (frameAvg > config_.targetFrameTimeMs) {
        g_perfLogger.Error("Performance gate FAILED: Frame average time {:.2f}ms > {:.2f}ms",
            frameAvg, config_.targetFrameTimeMs);
        return false;
    }
    
    return true;
}

void PerformanceBudgetTracker::logPerformanceReport() const {
    g_perfLogger.Info("=== Performance Budget Report ===");
    
    const char* categoryNames[] = {
        "Frame Total", "Weather System", "Screen Space", "TAA Resolve",
        "Geometry Pass", "Lighting Pass", "Post Process", "GPU Memory Copy"
    };
    
    for (int i = 0; i < 8; i++) {
        auto category = static_cast<PerformanceBudget>(i);
        double avg = getAverageTime(category);
        double p95 = getP95Time(category);
        double budget = getBudgetLimit(category);
        uint32_t violations = categoryStats_[i].violationCount;
        
        const char* status = "OK";
        if (p95 > budget) status = "OVER BUDGET";
        else if (avg > budget * 0.8) status = "HIGH";
        
        g_perfLogger.Info("  {}: avg={:.2f}ms, p95={:.2f}ms, budget={:.2f}ms [{}] ({} violations)",
            categoryNames[i], avg, p95, budget, status, violations);
    }
    
    bool gatesPassed = passesPerformanceGates();
    g_perfLogger.Info("Performance Gates: {}", gatesPassed ? "PASSED" : "FAILED");
    
    g_perfLogger.Info("=================================");
}

double PerformanceBudgetTracker::getBudgetLimit(PerformanceBudget budget) const {
    switch (budget) {
        case PerformanceBudget::FRAME_TOTAL: return config_.targetFrameTimeMs;
        case PerformanceBudget::WEATHER_SYSTEM: return config_.weatherBudgetMs;
        case PerformanceBudget::SCREEN_SPACE: return config_.screenSpaceBudgetMs;
        case PerformanceBudget::TAA_RESOLVE: return config_.taaBudgetMs;
        case PerformanceBudget::GEOMETRY_PASS: return config_.geometryBudgetMs;
        case PerformanceBudget::LIGHTING_PASS: return config_.lightingBudgetMs;
        case PerformanceBudget::POST_PROCESS: return config_.postProcessBudgetMs;
        case PerformanceBudget::GPU_MEMORY_COPY: return 1.0; // 1ms default
        default: return config_.targetFrameTimeMs;
    }
}

const char* PerformanceBudgetTracker::budgetToString(PerformanceBudget budget) const {
    switch (budget) {
        case PerformanceBudget::FRAME_TOTAL: return "FRAME_TOTAL";
        case PerformanceBudget::WEATHER_SYSTEM: return "WEATHER_SYSTEM";
        case PerformanceBudget::SCREEN_SPACE: return "SCREEN_SPACE";
        case PerformanceBudget::TAA_RESOLVE: return "TAA_RESOLVE";
        case PerformanceBudget::GEOMETRY_PASS: return "GEOMETRY_PASS";
        case PerformanceBudget::LIGHTING_PASS: return "LIGHTING_PASS";
        case PerformanceBudget::POST_PROCESS: return "POST_PROCESS";
        case PerformanceBudget::GPU_MEMORY_COPY: return "GPU_MEMORY_COPY";
        default: return "UNKNOWN";
    }
}

// GPUTimer implementation
GPUTimer::GPUTimer(VkDevice device, VkPhysicalDevice physicalDevice) 
    : device_(device), physicalDevice_(physicalDevice) {
    
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
    timestampPeriod_ = properties.limits.timestampPeriod;
    
    g_perfLogger.Info("GPUTimer initialized (timestamp period: {:.3f}ns)", timestampPeriod_);
}

GPUTimer::~GPUTimer() {
    shutdown();
}

bool GPUTimer::initialize(uint32_t framesInFlight, uint32_t maxTimestampsPerFrame) {
    framesInFlight_ = framesInFlight;
    maxTimestampsPerFrame_ = maxTimestampsPerFrame;
    
    g_perfLogger.Info("Initializing GPU timer: {} frames, {} timestamps/frame", 
        framesInFlight_, maxTimestampsPerFrame_);
    
    frameData_.resize(framesInFlight_);
    for (auto& frame : frameData_) {
        frame.timestampNames.reserve(maxTimestampsPerFrame_);
        frame.timestampResults.resize(maxTimestampsPerFrame_);
    }
    
    if (!createQueryPools()) {
        g_perfLogger.Error("Failed to create query pools");
        return false;
    }
    
    g_perfLogger.Info("GPU timer initialized successfully");
    return true;
}

void GPUTimer::shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        
        for (auto pool : queryPools_) {
            if (pool != VK_NULL_HANDLE) {
                vkDestroyQueryPool(device_, pool, nullptr);
            }
        }
        queryPools_.clear();
    }
    
    frameData_.clear();
    namedTimings_.clear();
    nameToTimestampID_.clear();
    
    g_perfLogger.Info("GPU timer shutdown complete");
}

void GPUTimer::beginFrame(uint32_t frameIndex) {
    currentFrameIndex_ = frameIndex % framesInFlight_;
    resetFrame(currentFrameIndex_);
    
    // Reset query pool for this frame
    if (currentFrameIndex_ < queryPools_.size()) {
        // vkCmdResetQueryPool would be called with command buffer
        g_perfLogger.Debug("Frame {} begun (timer index: {})", frameIndex, currentFrameIndex_);
    }
}

void GPUTimer::endFrame() {
    collectResults();
    g_perfLogger.Debug("Frame timing collection complete");
}

uint32_t GPUTimer::beginTimestamp(VkCommandBuffer cmd, const std::string& name) {
    auto& frame = frameData_[currentFrameIndex_];
    
    if (frame.nextTimestampIndex >= maxTimestampsPerFrame_) {
        g_perfLogger.Warn("GPU timestamp overflow for frame {}", currentFrameIndex_);
        return UINT32_MAX;
    }
    
    uint32_t timestampID = frame.nextTimestampIndex++;
    frame.timestampNames.push_back(name);
    nameToTimestampID_[name] = timestampID;
    
    // Write timestamp to query pool
    if (currentFrameIndex_ < queryPools_.size()) {
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
            queryPools_[currentFrameIndex_], timestampID);
    }
    
    return timestampID;
}

void GPUTimer::endTimestamp(VkCommandBuffer cmd, uint32_t timestampID) {
    if (timestampID == UINT32_MAX || timestampID >= maxTimestampsPerFrame_) return;
    
    // Write end timestamp
    if (currentFrameIndex_ < queryPools_.size()) {
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            queryPools_[currentFrameIndex_], timestampID + maxTimestampsPerFrame_ / 2);
    }
}

bool GPUTimer::collectResults() {
    auto& frame = frameData_[currentFrameIndex_];
    
    if (currentFrameIndex_ >= queryPools_.size() || frame.nextTimestampIndex == 0) {
        return true; // No timestamps to collect
    }
    
    VkQueryPool pool = queryPools_[currentFrameIndex_];
    uint32_t queryCount = frame.nextTimestampIndex * 2; // Begin + end for each timestamp
    
    VkResult result = vkGetQueryPoolResults(device_, pool, 0, queryCount,
        frame.timestampResults.size() * sizeof(uint64_t),
        frame.timestampResults.data(), sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    
    if (result != VK_SUCCESS) {
        g_perfLogger.Warn("Failed to collect GPU timestamp results");
        return false;
    }
    
    // Calculate timing deltas and update named timings
    namedTimings_.clear();
    
    for (uint32_t i = 0; i < frame.nextTimestampIndex; i++) {
        if (i < frame.timestampNames.size() && i * 2 + 1 < frame.timestampResults.size()) {
            uint64_t beginTime = frame.timestampResults[i];
            uint64_t endTime = frame.timestampResults[i + maxTimestampsPerFrame_ / 2];
            
            if (endTime > beginTime) {
                double deltaMs = (endTime - beginTime) * timestampPeriod_ / 1000000.0;
                namedTimings_[frame.timestampNames[i]] = deltaMs;
            }
        }
    }
    
    return true;
}

bool GPUTimer::createQueryPools() {
    queryPools_.resize(framesInFlight_);
    
    for (uint32_t i = 0; i < framesInFlight_; i++) {
        VkQueryPoolCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        createInfo.queryCount = maxTimestampsPerFrame_; // Begin + end timestamps
        
        VkResult result = vkCreateQueryPool(device_, &createInfo, nullptr, &queryPools_[i]);
        if (result != VK_SUCCESS) {
            g_perfLogger.Error("Failed to create query pool {}", i);
            return false;
        }
        
        std::string name = "GPUTimerPool_" + std::to_string(i);
        // VK_OBJECT_NAME would require device capability check
    }
    
    return true;
}

void GPUTimer::resetFrame(uint32_t frameIndex) {
    auto& frame = frameData_[frameIndex];
    frame.timestampNames.clear();
    frame.nextTimestampIndex = 0;
    std::fill(frame.timestampResults.begin(), frame.timestampResults.end(), 0);
}

double GPUTimer::getTimestampDelta(const std::string& name) const {
    auto it = namedTimings_.find(name);
    return it != namedTimings_.end() ? it->second : 0.0;
}

void GPUTimer::logTimingReport() const {
    g_perfLogger.Info("=== GPU Timing Report ===");
    
    double totalTime = 0.0;
    for (const auto& [name, time] : namedTimings_) {
        totalTime += time;
        g_perfLogger.Info("  {}: {:.3f}ms", name, time);
    }
    
    g_perfLogger.Info("  Total GPU time: {:.3f}ms", totalTime);
    g_perfLogger.Info("========================");
}

// PerformanceMonitor implementation
PerformanceMonitor::PerformanceMonitor() : gpuTimer_(VK_NULL_HANDLE, VK_NULL_HANDLE) {
    // Will be properly initialized with real device handles
}

PerformanceMonitor& PerformanceMonitor::instance() {
    if (!s_instance) {
        s_instance = new PerformanceMonitor();
    }
    return *s_instance;
}

bool PerformanceMonitor::initialize(const PerformanceBudgetTracker::BudgetConfig& config) {
    g_perfLogger.Info("Initializing performance monitoring system");
    
    budgetTracker_.setBudgetConfig(config);
    
    g_perfLogger.Info("Performance budget configuration:");
    g_perfLogger.Info("  Target frame time: {:.2f}ms ({:.0f} FPS)", 
        config.targetFrameTimeMs, 1000.0 / config.targetFrameTimeMs);
    g_perfLogger.Info("  P95 spike limit: {:.2f}ms", config.maxFrameTimeMs);
    g_perfLogger.Info("  Weather budget: {:.2f}ms", config.weatherBudgetMs);
    g_perfLogger.Info("  Screen space budget: {:.2f}ms", config.screenSpaceBudgetMs);
    
    // Enable NVTX profiling if available
    if (nvtxEnabled_) {
        g_perfLogger.Info("NVTX profiling enabled for Nsight capture");
    }
    
    initialized_ = true;
    g_perfLogger.Info("Performance monitoring initialized successfully");
    
    return true;
}

void PerformanceMonitor::shutdown() {
    if (!initialized_) return;
    
    g_perfLogger.Info("Shutting down performance monitoring");
    
    // Log final performance report
    logPerformanceReport();
    
    gpuTimer_.shutdown();
    
    initialized_ = false;
    
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void PerformanceMonitor::beginFrame(uint32_t frameIndex) {
    currentFrameIndex_ = frameIndex;
    frameStartTime_ = std::chrono::high_resolution_clock::now();
    
    gpuTimer_.beginFrame(frameIndex);
    
    if (nvtxEnabled_) {
        std::string frameName = "Frame_" + std::to_string(frameIndex);
        NVTX_RANGE_PUSH(frameName.c_str());
    }
}

void PerformanceMonitor::endFrame() {
    auto frameEndTime = std::chrono::high_resolution_clock::now();
    double frameTime = std::chrono::duration<double>(frameEndTime - frameStartTime_).count() * 1000.0;
    
    // Record total frame time
    budgetTracker_.recordSample(PerformanceBudget::FRAME_TOTAL, frameTime, currentFrameIndex_);
    
    gpuTimer_.endFrame();
    
    if (nvtxEnabled_) {
        NVTX_RANGE_POP();
    }
    
    // Clear pass tracking for next frame
    passStartTimes_.clear();
    passCategories_.clear();
}

void PerformanceMonitor::beginPass(const std::string& passName, PerformanceBudget category) {
    passStartTimes_[passName] = std::chrono::high_resolution_clock::now();
    passCategories_[passName] = category;
    
    if (nvtxEnabled_) {
        NVTX_RANGE_PUSH(passName.c_str());
    }
}

void PerformanceMonitor::endPass(const std::string& passName) {
    auto endTime = std::chrono::high_resolution_clock::now();
    
    auto startIt = passStartTimes_.find(passName);
    auto categoryIt = passCategories_.find(passName);
    
    if (startIt != passStartTimes_.end() && categoryIt != passCategories_.end()) {
        double passTime = std::chrono::duration<double>(endTime - startIt->second).count() * 1000.0;
        budgetTracker_.recordSample(categoryIt->second, passTime, currentFrameIndex_);
        
        g_perfLogger.Debug("Pass '{}': {:.3f}ms", passName, passTime);
    }
    
    if (nvtxEnabled_) {
        NVTX_RANGE_POP();
    }
}

void PerformanceMonitor::recordTiming(PerformanceBudget category, double timeMs, uint32_t frameIndex) {
    budgetTracker_.recordSample(category, timeMs, frameIndex);
}

bool PerformanceMonitor::passesPerformanceGates() const {
    return budgetTracker_.passesPerformanceGates();
}

void PerformanceMonitor::logPerformanceReport() const {
    budgetTracker_.logPerformanceReport();
    gpuTimer_.logTimingReport();
}

void PerformanceMonitor::exportPerformanceData(const std::string& directory) const {
    // Export performance data for CI analysis
    std::string filename = directory + "/performance_report.json";
    std::ofstream file(filename);
    
    if (!file.is_open()) {
        g_perfLogger.Error("Failed to open performance export file: {}", filename);
        return;
    }
    
    file << "{\n";
    file << "  \"timestamp\": \"" << std::chrono::system_clock::now().time_since_epoch().count() << "\",\n";
    file << "  \"performance_gates\": " << (passesPerformanceGates() ? "true" : "false") << ",\n";
    file << "  \"budget_config\": {\n";
    
    auto config = budgetTracker_.getBudgetConfig();
    file << "    \"target_frame_time_ms\": " << config.targetFrameTimeMs << ",\n";
    file << "    \"max_frame_time_ms\": " << config.maxFrameTimeMs << "\n";
    file << "  },\n";
    
    file << "  \"timings\": {\n";
    
    const char* categoryNames[] = {
        "frame_total", "weather_system", "screen_space", "taa_resolve",
        "geometry_pass", "lighting_pass", "post_process", "gpu_memory_copy"
    };
    
    for (int i = 0; i < 8; i++) {
        auto category = static_cast<PerformanceBudget>(i);
        double avg = budgetTracker_.getAverageTime(category);
        double p95 = budgetTracker_.getP95Time(category);
        
        file << "    \"" << categoryNames[i] << "\": {\n";
        file << "      \"average_ms\": " << avg << ",\n";
        file << "      \"p95_ms\": " << p95 << "\n";
        file << "    }";
        
        if (i < 7) file << ",";
        file << "\n";
    }
    
    file << "  }\n";
    file << "}\n";
    
    g_perfLogger.Info("Performance data exported to: {}", filename);
}

} // namespace voxelvk