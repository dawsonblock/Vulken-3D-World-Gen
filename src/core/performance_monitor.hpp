#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <fstream>

namespace voxelvk {

/**
 * Performance budget categories for P2 monitoring
 */
enum class PerformanceBudget {
    FRAME_TOTAL,        // Total frame time budget
    WEATHER_SYSTEM,     // Weather effects budget
    SCREEN_SPACE,       // SSAO + SSR budget
    TAA_RESOLVE,        // Temporal anti-aliasing budget
    GEOMETRY_PASS,      // Geometry rendering budget
    LIGHTING_PASS,      // Lighting calculation budget
    POST_PROCESS,       // Post-processing effects budget
    GPU_MEMORY_COPY     // Memory transfer budget
};

/**
 * Performance sample for statistical analysis
 */
struct PerformanceSample {
    double timeMs = 0.0;
    uint64_t timestampBegin = 0;
    uint64_t timestampEnd = 0;
    uint32_t frameIndex = 0;
    
    bool isValid() const { return timestampEnd > timestampBegin; }
    double calculateTimeMs(float timestampPeriod) const;
};

/**
 * Performance budget tracking with P95 monitoring
 */
class PerformanceBudgetTracker {
public:
    struct BudgetConfig {
        double targetFrameTimeMs = 8.33;    // 120 FPS target
        double maxFrameTimeMs = 12.0;       // P95 spike limit
        double weatherBudgetMs = 2.0;       // Weather effects budget
        double screenSpaceBudgetMs = 3.0;   // SSAO + SSR budget  
        double taaBudgetMs = 1.5;           // TAA resolve budget
        double geometryBudgetMs = 4.0;      // Geometry rendering budget
        double lightingBudgetMs = 2.5;      // Lighting budget
        double postProcessBudgetMs = 1.0;   // Post-processing budget
        
        static BudgetConfig Performance120() { return {8.33, 12.0, 1.5, 2.0, 1.0, 3.0, 2.0, 0.8}; }  // 120 FPS
        static BudgetConfig Balanced60()     { return {16.67, 20.0, 3.0, 4.0, 2.0, 6.0, 4.0, 1.5}; }  // 60 FPS
        static BudgetConfig Quality30()      { return {33.33, 40.0, 6.0, 8.0, 4.0, 12.0, 8.0, 3.0}; } // 30 FPS
    };
    
    PerformanceBudgetTracker();
    
    void setBudgetConfig(const BudgetConfig& config) { config_ = config; }
    const BudgetConfig& getBudgetConfig() const { return config_; }
    
    // Budget monitoring
    void recordSample(PerformanceBudget category, double timeMs, uint32_t frameIndex);
    bool isWithinBudget(PerformanceBudget category) const;
    bool isFrameWithinBudget() const;
    
    // Statistical analysis
    double getAverageTime(PerformanceBudget category) const;
    double getP95Time(PerformanceBudget category) const;
    double getMaxTime(PerformanceBudget category) const;
    uint32_t getViolationCount(PerformanceBudget category) const;
    
    // Performance gates for CI
    bool passesPerformanceGates() const;
    void logPerformanceReport() const;
    void exportPerformanceJSON(const std::string& filename) const;
    
    // Reset statistics
    void resetStats();
    
private:
    BudgetConfig config_{};
    
    struct CategoryStats {
        std::vector<double> samples;
        double runningAverage = 0.0;
        double maxTime = 0.0;
        uint32_t violationCount = 0;
        uint32_t sampleCount = 0;
        
        static constexpr size_t MAX_SAMPLES = 1000; // Keep last 1000 samples for P95 calculation
    };
    
    std::array<CategoryStats, 8> categoryStats_; // One per PerformanceBudget
    mutable std::mutex statsMutex_;
    
    double calculateP95(const std::vector<double>& samples) const;
    const char* budgetToString(PerformanceBudget budget) const;
    double getBudgetLimit(PerformanceBudget budget) const;
};

/**
 * GPU timing system with Vulkan timestamp queries
 */
class GPUTimer {
public:
    GPUTimer(VkDevice device, VkPhysicalDevice physicalDevice);
    ~GPUTimer();
    
    bool initialize(uint32_t framesInFlight = 3, uint32_t maxTimestampsPerFrame = 64);
    void shutdown();
    
    // Frame management
    void beginFrame(uint32_t frameIndex);
    void endFrame();
    
    // Timing operations
    uint32_t beginTimestamp(VkCommandBuffer cmd, const std::string& name);
    void endTimestamp(VkCommandBuffer cmd, uint32_t timestampID);
    
    // Results collection
    bool collectResults(); // Call after GPU work completes
    double getTimestampDelta(uint32_t timestampID) const; // In milliseconds
    double getTimestampDelta(const std::string& name) const;
    
    // Statistics
    const std::unordered_map<std::string, double>& getAllTimings() const { return namedTimings_; }
    double getTotalFrameTime() const;
    
    void logTimingReport() const;
    
private:
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    float timestampPeriod_ = 1.0f;
    
    std::vector<VkQueryPool> queryPools_;
    uint32_t framesInFlight_ = 3;
    uint32_t maxTimestampsPerFrame_ = 64;
    uint32_t currentFrameIndex_ = 0;
    
    // Per-frame tracking
    struct FrameTimingData {
        std::vector<std::string> timestampNames;
        std::vector<uint64_t> timestampResults;
        uint32_t nextTimestampIndex = 0;
    };
    
    std::vector<FrameTimingData> frameData_;
    std::unordered_map<std::string, uint32_t> nameToTimestampID_;
    std::unordered_map<std::string, double> namedTimings_;
    
    bool createQueryPools();
    void resetFrame(uint32_t frameIndex);
};

/**
 * Performance regression detector for CI
 */
class PerformanceRegressionDetector {
public:
    struct RegressionThresholds {
        double frameTimeThresholdPercent = 10.0;     // 10% frame time regression
        double memoryThresholdPercent = 15.0;        // 15% memory usage regression  
        uint32_t minSampleCount = 100;               // Minimum samples for detection
        double confidenceLevel = 0.95;               // 95% confidence for regression
    };
    
    PerformanceRegressionDetector();
    PerformanceRegressionDetector(const RegressionThresholds& thresholds);
    
    // Baseline management
    void setBaseline(PerformanceBudget category, const std::vector<double>& baselineSamples);
    void loadBaselineFromFile(const std::string& filename);
    void saveBaselineToFile(const std::string& filename) const;
    
    // Regression detection
    bool detectRegression(PerformanceBudget category, const std::vector<double>& currentSamples);
    void analyzeCurrentPerformance(const PerformanceBudgetTracker& tracker);
    
    // CI integration
    bool shouldFailCI() const { return hasSignificantRegression_; }
    void generateCIReport(const std::string& filename) const;
    
    void logRegressionReport() const;
    
private:
    RegressionThresholds thresholds_;
    
    std::array<std::vector<double>, 8> baselineData_;
    std::array<bool, 8> regressionDetected_{};
    bool hasSignificantRegression_ = false;
    
    double calculateMean(const std::vector<double>& samples) const;
    double calculateStdDev(const std::vector<double>& samples, double mean) const;
    bool statisticallySignificant(const std::vector<double>& baseline, const std::vector<double>& current) const;
};

/**
 * Master performance monitoring system
 */
class PerformanceMonitor {
public:
    static PerformanceMonitor& instance();
    
    bool initialize(const PerformanceBudgetTracker::BudgetConfig& config = PerformanceBudgetTracker::BudgetConfig::Performance120());
    void shutdown();
    
    // Optional: enable GPU timing when a valid Vulkan device is available
    // Safe no-op in headless/tests if never called.
    void enableGPUTimer(VkDevice device, VkPhysicalDevice physicalDevice,
                        uint32_t framesInFlight = 3, uint32_t maxTimestampsPerFrame = 64);
    
    // Frame timing
    void beginFrame(uint32_t frameIndex);
    void endFrame();
    
    // Pass timing with automatic budget tracking
    void beginPass(const std::string& passName, PerformanceBudget category = PerformanceBudget::FRAME_TOTAL);
    void endPass(const std::string& passName);
    
    // Direct timing recording
    void recordTiming(PerformanceBudget category, double timeMs, uint32_t frameIndex);
    
    // GPU timing integration
    GPUTimer& getGPUTimer() { return gpuTimer_; }
    const GPUTimer& getGPUTimer() const { return gpuTimer_; }
    PerformanceBudgetTracker& getBudgetTracker() { return budgetTracker_; }
    
    // Performance gates for CI
    bool passesPerformanceGates() const;
    void enableRegressionDetection(bool enable) { regressionDetectionEnabled_ = enable; }
    
    // Reporting
    void logPerformanceReport() const;
    void exportPerformanceData(const std::string& directory) const;
    
    // NVTX integration  
    void enableNVTXProfiling(bool enable) { nvtxEnabled_ = enable; }
    bool isNVTXEnabled() const { return nvtxEnabled_; }
    
private:
    PerformanceMonitor();
    
    PerformanceBudgetTracker budgetTracker_;
    GPUTimer gpuTimer_;
    PerformanceRegressionDetector regressionDetector_;
    
    bool initialized_ = false;
    bool nvtxEnabled_ = false;
    bool gpuTimerEnabled_ = false;
    bool regressionDetectionEnabled_ = false;
    
    // Current frame tracking
    uint32_t currentFrameIndex_ = 0;
    std::chrono::high_resolution_clock::time_point frameStartTime_;
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> passStartTimes_;
    std::unordered_map<std::string, PerformanceBudget> passCategories_;
    
    static PerformanceMonitor* s_instance;
};

/**
 * RAII performance pass timer
 */
class ScopedPassTimer {
public:
    ScopedPassTimer(const std::string& passName, PerformanceBudget category = PerformanceBudget::FRAME_TOTAL)
        : passName_(passName) {
        PerformanceMonitor::instance().beginPass(passName_, category);
    }
    
    ~ScopedPassTimer() {
        PerformanceMonitor::instance().endPass(passName_);
    }
    
private:
    std::string passName_;
};

/**
 * Convenience macros for performance monitoring
 */
#define PERF_TIMER(name, category) ScopedPassTimer _perf_timer(name, category)
#define PERF_FRAME_TIMER(name) PERF_TIMER(name, PerformanceBudget::FRAME_TOTAL)
#define PERF_WEATHER_TIMER(name) PERF_TIMER(name, PerformanceBudget::WEATHER_SYSTEM)
#define PERF_SCREENSPACE_TIMER(name) PERF_TIMER(name, PerformanceBudget::SCREEN_SPACE)

} // namespace voxelvk