#include <iostream>
#include <cassert>
#include <memory>
#include <chrono>

// Weather system for testing
#include "../src/env/weather/weather_system.hpp"
#include "../src/env/weather/lightning.hpp"

using namespace voxelvk;

/**
 * Test weather system functionality
 */
int main() {
    std::cout << "Testing weather system..." << std::endl;

    // Test 1: Weather system initialization
    auto weatherSystem = std::make_unique<WeatherSystem>();
    assert(weatherSystem != nullptr);
    std::cout << "  ✓ Weather system creation" << std::endl;

    // Test 2: Weather state changes
    weatherSystem->setState(WeatherState::CLEAR);
    auto ubo = weatherSystem->getUBO();
    assert(ubo.state == static_cast<uint32_t>(WeatherState::CLEAR));
    std::cout << "  ✓ Weather state setting" << std::endl;

    // Test 3: Weather parameters
    weatherSystem->setWind(10.0f, 180.0f, 0.5f);
    weatherSystem->setCloudCoverage(0.7f);
    weatherSystem->setPrecipRate(5.0f);

    ubo = weatherSystem->getUBO();
    assert(ubo.windSpeed >= 9.0f && ubo.windSpeed <= 11.0f); // Allow for variation
    assert(ubo.cloudCoverage == 0.7f);
    std::cout << "  ✓ Weather parameter setting" << std::endl;

    // Test 4: Weather transitions
    WeatherState states[] = {WeatherState::CLEAR, WeatherState::RAIN, WeatherState::STORM, WeatherState::SNOW};

    for (auto state : states) {
        weatherSystem->setState(state);
        weatherSystem->tick(0.016); // One frame

        auto stateUBO = weatherSystem->getUBO();
        assert(stateUBO.state == static_cast<uint32_t>(state));
        (void)stateUBO; // Suppress unused variable warning
    }
    std::cout << "  ✓ Weather state transitions" << std::endl;

    // Test 5: Lightning system
    Lightning lightning;
    lightning.setEnabled(true);

    // Simulate storm conditions
    for (int i = 0; i < 100; i++) {
        lightning.tick(0.016f, true); // Storm conditions

        float intensity = lightning.intensity();
        assert(intensity >= 0.0f && intensity <= 1.0f);

        if (intensity > 0.1f) {
            std::cout << "  ✓ Lightning flash detected (intensity: " << intensity << ")" << std::endl;
            break;
        }
    }

    // Test 6: Weather configuration loading (if available)
    try {
        weatherSystem->loadFromYaml("config/weather.yaml");
        std::cout << "  ✓ Weather configuration loading" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  ⚠ Weather config not available (using defaults): " << e.what() << std::endl;
    }

    // Test 7: Frame timing consistency
    auto startTime = std::chrono::high_resolution_clock::now();

    for (int frame = 0; frame < 1000; frame++) {
        weatherSystem->tick(0.016);
        lightning.tick(0.016f, (frame % 100) < 20); // Occasional storms
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double totalTime = std::chrono::duration<double>(endTime - startTime).count();
    double avgFrameTime = totalTime / 1000.0 * 1000.0; // Convert to ms

    std::cout << "  ✓ Performance test: " << avgFrameTime << "ms avg per frame (1000 frames)" << std::endl;

    if (avgFrameTime < 0.1) { // Should be very fast for weather update only
        std::cout << "  ✓ Weather performance within budget" << std::endl;
    } else {
        std::cout << "  ⚠ Weather performance higher than expected" << std::endl;
    }

    std::cout << "✅ Weather system test passed" << std::endl;
    return 0;
}
