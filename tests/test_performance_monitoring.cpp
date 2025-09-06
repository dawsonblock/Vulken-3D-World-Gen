#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

// Performance monitoring for testing
#include "../src/core/performance_monitor.hpp"

using namespace voxelvk;

/**
 * Test performance monitoring systems
 */
int main() {
    std::cout << "Testing performance monitoring..." << std::endl;

    // Test 1: Performance monitor initialization
    auto& perfMonitor = PerformanceMonitor::instance();

    auto config = PerformanceBudgetTracker::BudgetConfig::Performance120();
    bool initResult = perfMonitor.initialize(config);
    assert(initResult);
    (void)initResult; // Suppress unused variable warning
    std::cout << "  ✓ Performance monitor initialization" << std::endl;

    // Test 2: Budget tracking
    auto& budgetTracker = perfMonitor.getBudgetTracker();

    // Record some sample timings
    budgetTracker.recordSample(PerformanceBudget::FRAME_TOTAL, 7.5, 1);
    budgetTracker.recordSample(PerformanceBudget::FRAME_TOTAL, 8.1, 2);
    budgetTracker.recordSample(PerformanceBudget::FRAME_TOTAL, 7.9, 3);
    budgetTracker.recordSample(PerformanceBudget::WEATHER_SYSTEM, 1.5, 1);
    budgetTracker.recordSample(PerformanceBudget::WEATHER_SYSTEM, 1.8, 2);

    assert(budgetTracker.isWithinBudget(PerformanceBudget::FRAME_TOTAL));
    assert(budgetTracker.isWithinBudget(PerformanceBudget::WEATHER_SYSTEM));
    std::cout << "  ✓ Budget tracking and validation" << std::endl;

    // Test 3: Performance gates
    bool gatesPassed = budgetTracker.passesPerformanceGates();
    assert(gatesPassed);
    (void)gatesPassed; // Suppress unused variable warning
    std::cout << "  ✓ Performance gates validation" << std::endl;

    // Test 4: Frame timing
    perfMonitor.beginFrame(1);

    // Simulate some work with timing
    perfMonitor.beginPass("TestPass", PerformanceBudget::GEOMETRY_PASS);
    std::this_thread::sleep_for(std::chrono::milliseconds(2)); // Simulate 2ms work
    perfMonitor.endPass("TestPass");

    perfMonitor.endFrame();
    std::cout << "  ✓ Frame timing and pass tracking" << std::endl;

    // Test 5: NVTX integration (concept test)
    perfMonitor.enableNVTXProfiling(true);
    assert(perfMonitor.isNVTXEnabled());
    std::cout << "  ✓ NVTX profiling toggle" << std::endl;

    // Test 6: Performance report
    std::cout << "\nPerformance Report:" << std::endl;
    perfMonitor.logPerformanceReport();

    // Test 7: Performance budget categories
    double frameAvg = budgetTracker.getAverageTime(PerformanceBudget::FRAME_TOTAL);
    double weatherAvg = budgetTracker.getAverageTime(PerformanceBudget::WEATHER_SYSTEM);

    std::cout << "  Frame average: " << frameAvg << "ms" << std::endl;
    std::cout << "  Weather average: " << weatherAvg << "ms" << std::endl;

    assert(frameAvg > 0.0 && frameAvg < 20.0); // Reasonable range
    assert(weatherAvg > 0.0 && weatherAvg < 5.0); // Weather should be fast

    // Test 8: P95 calculation
    double frameP95 = budgetTracker.getP95Time(PerformanceBudget::FRAME_TOTAL);
    assert(frameP95 >= frameAvg); // P95 should be >= average
    std::cout << "  ✓ P95 calculation: " << frameP95 << "ms" << std::endl;

    // Cleanup
    perfMonitor.shutdown();

    std::cout << "✅ Performance monitoring test passed" << std::endl;
    return 0;
}
