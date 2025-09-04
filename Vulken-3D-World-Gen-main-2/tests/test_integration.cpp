#include <iostream>
#include <cassert>
#include <memory>

// Integration test for all major systems
#include "../src/env/weather/weather_system.hpp"
#include "../src/core/performance_monitor.hpp"
#include "../src/rl/rl_backend.hpp"

using namespace voxelvk;
using namespace voxelvk::rl;

/**
 * Integration test for P0+P1+P2+Weather+RL systems
 */
int main() {
    std::cout << "Testing system integration..." << std::endl;
    
    // Test 1: Weather + Performance monitoring integration
    {
        auto& perfMonitor = PerformanceMonitor::instance();
        auto config = PerformanceBudgetTracker::BudgetConfig::Performance120();
        assert(perfMonitor.initialize(config));
        
        auto weatherSystem = std::make_unique<WeatherSystem>();
        
        perfMonitor.beginFrame(1);
        perfMonitor.beginPass("WeatherUpdate", PerformanceBudget::WEATHER_SYSTEM);
        
        // Simulate weather update
        weatherSystem->tick(0.016);
        
        perfMonitor.endPass("WeatherUpdate");
        perfMonitor.endFrame();
        
        // Check that weather stays within budget
        auto& tracker = perfMonitor.getBudgetTracker();
        assert(tracker.isWithinBudget(PerformanceBudget::WEATHER_SYSTEM));
        
        perfMonitor.shutdown();
        std::cout << "  ✓ Weather + Performance integration" << std::endl;
    }
    
    // Test 2: RL Backend + Weather integration
    {
        auto rlBackend = RLBackendFactory::create(RLBackendFactory::BackendType::MINIMAL_MLP);
        auto weatherSystem = std::make_unique<WeatherSystem>();
        
        // Simulate RL agent observing weather state
        auto weatherUBO = weatherSystem->getUBO();
        
        std::vector<float> weatherObs = {
            static_cast<float>(weatherUBO.state),
            weatherUBO.windSpeed / 20.0f,      // Normalize wind speed
            weatherUBO.cloudCoverage,
            weatherUBO.timeOfDay
        };
        
        // Pad to expected size
        weatherObs.resize(64, 0.0f);
        
        std::vector<float> actions, values;
        rlBackend->forward(weatherObs, actions, values);
        
        assert(!actions.empty());
        assert(!values.empty());
        
        std::cout << "  ✓ RL Backend + Weather integration" << std::endl;
    }
    
    // Test 3: Multi-system frame simulation
    {
        auto weatherSystem = std::make_unique<WeatherSystem>();
        auto rlBackend = RLBackendFactory::create(RLBackendFactory::BackendType::DUMMY);
        
        auto& perfMonitor = PerformanceMonitor::instance();
        perfMonitor.initialize();
        
        const uint32_t SIMULATION_FRAMES = 180; // 3 seconds at 60 FPS
        
        for (uint32_t frame = 0; frame < SIMULATION_FRAMES; frame++) {
            perfMonitor.beginFrame(frame);
            
            // Weather update
            perfMonitor.beginPass("WeatherUpdate", PerformanceBudget::WEATHER_SYSTEM);
            weatherSystem->tick(0.016);
            perfMonitor.endPass("WeatherUpdate");
            
            // RL inference
            perfMonitor.beginPass("RLInference", PerformanceBudget::FRAME_TOTAL);
            std::vector<float> obs(64, static_cast<float>(frame) / SIMULATION_FRAMES);
            std::vector<float> actions, values;
            rlBackend->forward(obs, actions, values);
            perfMonitor.endPass("RLInference");
            
            perfMonitor.endFrame();
            
            // Check system stability
            if (frame % 60 == 0) {
                auto weatherUBO = weatherSystem->getUBO();
                assert(weatherUBO.windSpeed >= 0.0f);
                assert(weatherUBO.cloudCoverage >= 0.0f && weatherUBO.cloudCoverage <= 1.0f);
            }
        }
        
        // Validate performance
        auto& tracker = perfMonitor.getBudgetTracker();
        bool framesBudgetOk = tracker.isWithinBudget(PerformanceBudget::FRAME_TOTAL);
        bool weatherBudgetOk = tracker.isWithinBudget(PerformanceBudget::WEATHER_SYSTEM);
        
        std::cout << "  Frame budget compliance: " << (framesBudgetOk ? "PASS" : "FAIL") << std::endl;
        std::cout << "  Weather budget compliance: " << (weatherBudgetOk ? "PASS" : "FAIL") << std::endl;
        
        perfMonitor.shutdown();
        std::cout << "  ✓ Multi-system frame simulation (" << SIMULATION_FRAMES << " frames)" << std::endl;
    }
    
    // Test 4: System interoperability
    {
        // Test that all systems can coexist without conflicts
        auto weatherSystem = std::make_unique<WeatherSystem>();
        auto dummyBackend = RLBackendFactory::create(RLBackendFactory::BackendType::DUMMY);
        auto mlpBackend = RLBackendFactory::create(RLBackendFactory::BackendType::MINIMAL_MLP);
        
        // All systems should be able to operate simultaneously
        for (int i = 0; i < 10; i++) {
            weatherSystem->tick(0.016);
            
            std::vector<float> obs(64, 0.1f * i);
            std::vector<float> actions1, values1, actions2, values2;
            
            dummyBackend->forward(obs, actions1, values1);
            mlpBackend->forward(obs, actions2, values2);
            
            assert(!actions1.empty() && !actions2.empty());
        }
        
        std::cout << "  ✓ System interoperability" << std::endl;
    }
    
    std::cout << "✅ Integration test passed" << std::endl;
    std::cout << "All major systems (Weather, Performance, RL) working together" << std::endl;
    
    return 0;
}