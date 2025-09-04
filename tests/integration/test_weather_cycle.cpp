/**
 * Weather System Integration Tests
 * ================================
 * 
 * Integration tests for the weather system that validate state transitions,
 * parameter sweeps, and weather cycle consistency.
 */

#include <gtest/gtest.h>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <random>

// Mock weather system for testing
namespace WeatherSystem {
    
    enum class WeatherType {
        CLEAR = 0,
        CLOUDY = 1,
        LIGHT_RAIN = 2,
        HEAVY_RAIN = 3,
        THUNDERSTORM = 4,
        SNOW = 5,
        FOG = 6,
        SANDSTORM = 7
    };
    
    struct WeatherParams {
        float temperature = 20.0f;      // Celsius
        float humidity = 0.5f;          // 0-1
        float pressure = 1013.25f;      // hPa
        float windSpeed = 5.0f;         // m/s
        float windDirection = 0.0f;     // radians
        float precipitation = 0.0f;     // mm/h
        float visibility = 10000.0f;    // meters
        float cloudCover = 0.0f;        // 0-1
    };
    
    struct TimeOfDay {
        uint32_t hour = 12;         // 0-23
        uint32_t minute = 0;        // 0-59
        uint32_t dayOfYear = 180;   // 1-365
        
        float getNormalizedTime() const {
            return (hour + minute / 60.0f) / 24.0f;
        }
        
        float getSeasonFactor() const {
            // Simple seasonal variation
            return 0.5f + 0.5f * std::sin((dayOfYear - 80) * 2.0f * 3.14159f / 365.0f);
        }
    };
    
    class WeatherSimulator {
    public:
        WeatherSimulator() {
            // Initialize with clear weather
            currentWeather_ = WeatherType::CLEAR;
            updateWeatherParams();
        }
        
        void update(float deltaTime) {
            timeAccumulator_ += deltaTime;
            
            // Update weather every few seconds
            if (timeAccumulator_ >= updateInterval_) {
                timeAccumulator_ = 0.0f;
                updateWeatherState();
                updateWeatherParams();
                
                // Record state for validation
                stateHistory_.push_back({currentWeather_, params_, timeOfDay_});
                if (stateHistory_.size() > maxHistorySize_) {
                    stateHistory_.erase(stateHistory_.begin());
                }
            }
            
            // Advance time of day
            advanceTime(deltaTime);
        }
        
        void setWeather(WeatherType weather) {
            currentWeather_ = weather;
            updateWeatherParams();
        }
        
        void setTimeOfDay(const TimeOfDay& time) {
            timeOfDay_ = time;
            updateWeatherParams();
        }
        
        WeatherType getCurrentWeather() const {
            return currentWeather_;
        }
        
        const WeatherParams& getWeatherParams() const {
            return params_;
        }
        
        const TimeOfDay& getTimeOfDay() const {
            return timeOfDay_;
        }
        
        bool isValidWeatherState() const {
            // Validate weather parameters are within reasonable ranges
            return params_.temperature >= -50.0f && params_.temperature <= 60.0f &&
                   params_.humidity >= 0.0f && params_.humidity <= 1.0f &&
                   params_.pressure >= 800.0f && params_.pressure <= 1200.0f &&
                   params_.windSpeed >= 0.0f && params_.windSpeed <= 100.0f &&
                   params_.precipitation >= 0.0f && params_.precipitation <= 200.0f &&
                   params_.visibility >= 10.0f && params_.visibility <= 50000.0f &&
                   params_.cloudCover >= 0.0f && params_.cloudCover <= 1.0f;
        }
        
        std::vector<WeatherType> getValidTransitions(WeatherType from) const {
            static const std::unordered_map<WeatherType, std::vector<WeatherType>> transitions = {
                {WeatherType::CLEAR, {WeatherType::CLOUDY, WeatherType::FOG}},
                {WeatherType::CLOUDY, {WeatherType::CLEAR, WeatherType::LIGHT_RAIN, WeatherType::FOG}},
                {WeatherType::LIGHT_RAIN, {WeatherType::CLOUDY, WeatherType::HEAVY_RAIN, WeatherType::CLEAR}},
                {WeatherType::HEAVY_RAIN, {WeatherType::LIGHT_RAIN, WeatherType::THUNDERSTORM, WeatherType::CLOUDY}},
                {WeatherType::THUNDERSTORM, {WeatherType::HEAVY_RAIN, WeatherType::LIGHT_RAIN}},
                {WeatherType::SNOW, {WeatherType::CLOUDY, WeatherType::CLEAR}},
                {WeatherType::FOG, {WeatherType::CLEAR, WeatherType::CLOUDY}},
                {WeatherType::SANDSTORM, {WeatherType::CLEAR, WeatherType::CLOUDY}}
            };
            
            auto it = transitions.find(from);
            return (it != transitions.end()) ? it->second : std::vector<WeatherType>{};
        }
        
        float calculateWeatherScore() const {
            // Calculate a composite score for weather realism
            float score = 1.0f;
            
            // Penalize unrealistic parameter combinations
            if (currentWeather_ == WeatherType::SNOW && params_.temperature > 5.0f) {
                score *= 0.5f;  // Snow at high temperatures is unrealistic
            }
            
            if (params_.precipitation > 0.0f && params_.cloudCover < 0.3f) {
                score *= 0.7f;  // Rain without clouds is unrealistic
            }
            
            if (params_.humidity < 0.2f && params_.precipitation > 10.0f) {
                score *= 0.6f;  // Heavy rain with low humidity is unrealistic
            }
            
            return score;
        }
        
        struct StateSnapshot {
            WeatherType weather;
            WeatherParams params;
            TimeOfDay time;
        };
        
        const std::vector<StateSnapshot>& getStateHistory() const {
            return stateHistory_;
        }
        
    private:
        void updateWeatherState() {
            // Simple weather state machine
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dis(0.0f, 1.0f);
            
            auto validTransitions = getValidTransitions(currentWeather_);
            if (!validTransitions.empty() && dis(gen) < 0.3f) {  // 30% chance of transition
                std::uniform_int_distribution<int> transitionDis(0, static_cast<int>(validTransitions.size()) - 1);
                currentWeather_ = validTransitions[transitionDis(gen)];
            }
        }
        
        void updateWeatherParams() {
            float seasonFactor = timeOfDay_.getSeasonFactor();
            float timeFactor = timeOfDay_.getNormalizedTime();
            
            switch (currentWeather_) {
                case WeatherType::CLEAR:
                    params_.temperature = 15.0f + 10.0f * seasonFactor + 5.0f * std::sin(timeFactor * 2.0f * 3.14159f);
                    params_.humidity = 0.3f + 0.2f * seasonFactor;
                    params_.pressure = 1013.25f + 10.0f * std::sin(timeFactor * 2.0f * 3.14159f);
                    params_.windSpeed = 2.0f + 3.0f * seasonFactor;
                    params_.precipitation = 0.0f;
                    params_.visibility = 15000.0f;
                    params_.cloudCover = 0.1f;
                    break;
                    
                case WeatherType::CLOUDY:
                    params_.temperature = 12.0f + 8.0f * seasonFactor;
                    params_.humidity = 0.6f + 0.2f * seasonFactor;
                    params_.pressure = 1008.0f;
                    params_.windSpeed = 5.0f;
                    params_.precipitation = 0.0f;
                    params_.visibility = 12000.0f;
                    params_.cloudCover = 0.7f;
                    break;
                    
                case WeatherType::LIGHT_RAIN:
                    params_.temperature = 10.0f + 5.0f * seasonFactor;
                    params_.humidity = 0.8f;
                    params_.pressure = 1005.0f;
                    params_.windSpeed = 7.0f;
                    params_.precipitation = 2.5f;
                    params_.visibility = 8000.0f;
                    params_.cloudCover = 0.9f;
                    break;
                    
                case WeatherType::HEAVY_RAIN:
                    params_.temperature = 8.0f + 3.0f * seasonFactor;
                    params_.humidity = 0.95f;
                    params_.pressure = 1000.0f;
                    params_.windSpeed = 12.0f;
                    params_.precipitation = 15.0f;
                    params_.visibility = 3000.0f;
                    params_.cloudCover = 1.0f;
                    break;
                    
                case WeatherType::THUNDERSTORM:
                    params_.temperature = 12.0f + 6.0f * seasonFactor;
                    params_.humidity = 0.9f;
                    params_.pressure = 995.0f;
                    params_.windSpeed = 20.0f;
                    params_.precipitation = 25.0f;
                    params_.visibility = 2000.0f;
                    params_.cloudCover = 1.0f;
                    break;
                    
                case WeatherType::SNOW:
                    params_.temperature = -2.0f + 3.0f * seasonFactor;
                    params_.humidity = 0.85f;
                    params_.pressure = 1010.0f;
                    params_.windSpeed = 8.0f;
                    params_.precipitation = 5.0f;
                    params_.visibility = 5000.0f;
                    params_.cloudCover = 0.9f;
                    break;
                    
                case WeatherType::FOG:
                    params_.temperature = 8.0f + 4.0f * seasonFactor;
                    params_.humidity = 0.95f;
                    params_.pressure = 1015.0f;
                    params_.windSpeed = 1.0f;
                    params_.precipitation = 0.0f;
                    params_.visibility = 200.0f;
                    params_.cloudCover = 0.6f;
                    break;
                    
                case WeatherType::SANDSTORM:
                    params_.temperature = 25.0f + 15.0f * seasonFactor;
                    params_.humidity = 0.1f;
                    params_.pressure = 1005.0f;
                    params_.windSpeed = 25.0f;
                    params_.precipitation = 0.0f;
                    params_.visibility = 500.0f;
                    params_.cloudCover = 0.8f;
                    break;
            }
            
            // Add some random variation
            std::random_device rd;
            std::mt19937 gen(rd());
            std::normal_distribution<float> variation(0.0f, 0.1f);
            
            params_.temperature += variation(gen) * 2.0f;
            params_.humidity = std::clamp(params_.humidity + variation(gen) * 0.1f, 0.0f, 1.0f);
            params_.pressure += variation(gen) * 5.0f;
            params_.windSpeed = std::max(0.0f, params_.windSpeed + variation(gen) * 2.0f);
        }
        
        void advanceTime(float deltaTime) {
            const float timeScale = 60.0f;  // 1 second = 1 minute
            float minutes = deltaTime * timeScale;
            
            timeOfDay_.minute += static_cast<uint32_t>(minutes);
            
            if (timeOfDay_.minute >= 60) {
                timeOfDay_.hour += timeOfDay_.minute / 60;
                timeOfDay_.minute %= 60;
            }
            
            if (timeOfDay_.hour >= 24) {
                timeOfDay_.dayOfYear += timeOfDay_.hour / 24;
                timeOfDay_.hour %= 24;
            }
            
            if (timeOfDay_.dayOfYear > 365) {
                timeOfDay_.dayOfYear = 1 + (timeOfDay_.dayOfYear - 1) % 365;
            }
        }
        
        WeatherType currentWeather_;
        WeatherParams params_;
        TimeOfDay timeOfDay_;
        
        float timeAccumulator_ = 0.0f;
        float updateInterval_ = 1.0f;  // Update every second
        
        std::vector<StateSnapshot> stateHistory_;
        static constexpr size_t maxHistorySize_ = 100;
    };
}

class WeatherCycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        simulator_ = std::make_unique<WeatherSystem::WeatherSimulator>();
    }
    
    std::unique_ptr<WeatherSystem::WeatherSimulator> simulator_;
};

TEST_F(WeatherCycleTest, InitialState) {
    EXPECT_EQ(simulator_->getCurrentWeather(), WeatherSystem::WeatherType::CLEAR);
    EXPECT_TRUE(simulator_->isValidWeatherState());
}

TEST_F(WeatherCycleTest, ParameterRangeValidation) {
    // Test all weather types for parameter validity
    std::vector<WeatherSystem::WeatherType> allWeatherTypes = {
        WeatherSystem::WeatherType::CLEAR,
        WeatherSystem::WeatherType::CLOUDY,
        WeatherSystem::WeatherType::LIGHT_RAIN,
        WeatherSystem::WeatherType::HEAVY_RAIN,
        WeatherSystem::WeatherType::THUNDERSTORM,
        WeatherSystem::WeatherType::SNOW,
        WeatherSystem::WeatherType::FOG,
        WeatherSystem::WeatherType::SANDSTORM
    };
    
    for (auto weatherType : allWeatherTypes) {
        simulator_->setWeather(weatherType);
        EXPECT_TRUE(simulator_->isValidWeatherState()) 
            << "Invalid weather state for type " << static_cast<int>(weatherType);
        
        const auto& params = simulator_->getWeatherParams();
        
        // Log weather parameters for debugging
        std::cout << "Weather type " << static_cast<int>(weatherType)
                  << ": T=" << params.temperature
                  << "°C, H=" << params.humidity
                  << ", P=" << params.pressure << "hPa"
                  << ", Wind=" << params.windSpeed << "m/s"
                  << ", Rain=" << params.precipitation << "mm/h"
                  << std::endl;
    }
}

TEST_F(WeatherCycleTest, StateTransitions) {
    // Test valid state transitions
    auto initialWeather = simulator_->getCurrentWeather();
    auto validTransitions = simulator_->getValidTransitions(initialWeather);
    
    EXPECT_FALSE(validTransitions.empty()) << "No valid transitions from initial state";
    
    // Test each valid transition
    for (auto targetWeather : validTransitions) {
        simulator_->setWeather(targetWeather);
        EXPECT_EQ(simulator_->getCurrentWeather(), targetWeather);
        EXPECT_TRUE(simulator_->isValidWeatherState());
    }
}

TEST_F(WeatherCycleTest, TimeProgression) {
    auto initialTime = simulator_->getTimeOfDay();
    
    // Simulate 1 hour
    for (int i = 0; i < 3600; i++) {  // 1 second steps
        simulator_->update(1.0f);
        EXPECT_TRUE(simulator_->isValidWeatherState());
    }
    
    auto finalTime = simulator_->getTimeOfDay();
    
    // Time should have advanced
    EXPECT_NE(initialTime.hour, finalTime.hour);
    
    std::cout << "Time progressed from " << initialTime.hour << ":" << initialTime.minute
              << " to " << finalTime.hour << ":" << finalTime.minute << std::endl;
}

TEST_F(WeatherCycleTest, SeasonalVariation) {
    // Test weather parameters across different seasons
    std::vector<uint32_t> testDays = {1, 91, 182, 273};  // Start of each season
    
    WeatherSystem::TimeOfDay time;
    time.hour = 12;  // Noon
    time.minute = 0;
    
    for (uint32_t day : testDays) {
        time.dayOfYear = day;
        simulator_->setTimeOfDay(time);
        
        // Test multiple weather types at this season
        simulator_->setWeather(WeatherSystem::WeatherType::CLEAR);
        EXPECT_TRUE(simulator_->isValidWeatherState());
        
        const auto& params = simulator_->getWeatherParams();
        float seasonFactor = time.getSeasonFactor();
        
        std::cout << "Day " << day << " (season factor " << seasonFactor 
                  << "): Temperature " << params.temperature << "°C" << std::endl;
        
        // Temperature should vary with season
        EXPECT_GE(params.temperature, -20.0f);
        EXPECT_LE(params.temperature, 40.0f);
    }
}

TEST_F(WeatherCycleTest, WeatherRealism) {
    // Test weather realism scoring
    float totalScore = 0.0f;
    int numTests = 0;
    
    std::vector<WeatherSystem::WeatherType> allWeatherTypes = {
        WeatherSystem::WeatherType::CLEAR,
        WeatherSystem::WeatherType::CLOUDY,
        WeatherSystem::WeatherType::LIGHT_RAIN,
        WeatherSystem::WeatherType::HEAVY_RAIN,
        WeatherSystem::WeatherType::SNOW,
        WeatherSystem::WeatherType::FOG
    };
    
    for (auto weatherType : allWeatherTypes) {
        simulator_->setWeather(weatherType);
        float score = simulator_->calculateWeatherScore();
        totalScore += score;
        numTests++;
        
        EXPECT_GT(score, 0.0f) << "Weather realism score should be positive";
        EXPECT_LE(score, 1.0f) << "Weather realism score should not exceed 1.0";
        
        std::cout << "Weather type " << static_cast<int>(weatherType)
                  << " realism score: " << score << std::endl;
    }
    
    float averageScore = totalScore / numTests;
    EXPECT_GT(averageScore, 0.7f) << "Average realism score should be reasonable";
    
    std::cout << "Average weather realism score: " << averageScore << std::endl;
}

TEST_F(WeatherCycleTest, LongTermSimulation) {
    // Run extended simulation to test stability
    const float simulationTime = 3600.0f;  // 1 hour
    const float timeStep = 0.1f;
    
    int validStateCount = 0;
    int totalUpdates = 0;
    
    for (float t = 0; t < simulationTime; t += timeStep) {
        simulator_->update(timeStep);
        
        if (simulator_->isValidWeatherState()) {
            validStateCount++;
        }
        totalUpdates++;
    }
    
    // At least 95% of states should be valid
    float validityRate = static_cast<float>(validStateCount) / totalUpdates;
    EXPECT_GT(validityRate, 0.95f) << "Weather validity rate too low: " << validityRate;
    
    std::cout << "Long-term simulation: " << validStateCount << "/" << totalUpdates 
              << " valid states (" << (validityRate * 100.0f) << "%)" << std::endl;
}

TEST_F(WeatherCycleTest, StateHistoryTracking) {
    // Simulate for a while and check state history
    for (int i = 0; i < 10; i++) {
        simulator_->update(1.0f);
    }
    
    const auto& history = simulator_->getStateHistory();
    EXPECT_GT(history.size(), 0) << "State history should be recorded";
    
    // Verify history consistency
    for (size_t i = 1; i < history.size(); i++) {
        const auto& prevState = history[i-1];
        const auto& currentState = history[i];
        
        // Time should advance
        EXPECT_GE(currentState.time.hour * 60 + currentState.time.minute,
                 prevState.time.hour * 60 + prevState.time.minute);
    }
    
    std::cout << "State history contains " << history.size() << " entries" << std::endl;
}

TEST_F(WeatherCycleTest, ExtremWeatherHandling) {
    // Test extreme weather conditions
    WeatherSystem::WeatherType extremeWeatherTypes[] = {
        WeatherSystem::WeatherType::THUNDERSTORM,
        WeatherSystem::WeatherType::SANDSTORM,
        WeatherSystem::WeatherType::HEAVY_RAIN
    };
    
    for (auto weatherType : extremeWeatherTypes) {
        simulator_->setWeather(weatherType);
        
        // Run simulation for a bit
        for (int i = 0; i < 60; i++) {
            simulator_->update(1.0f);
            EXPECT_TRUE(simulator_->isValidWeatherState()) 
                << "Extreme weather state became invalid";
        }
        
        const auto& params = simulator_->getWeatherParams();
        float score = simulator_->calculateWeatherScore();
        
        std::cout << "Extreme weather " << static_cast<int>(weatherType)
                  << ": Wind=" << params.windSpeed << "m/s"
                  << ", Visibility=" << params.visibility << "m"
                  << ", Score=" << score << std::endl;
        
        // Even extreme weather should have some realism
        EXPECT_GT(score, 0.3f) << "Extreme weather realism too low";
    }
}

TEST_F(WeatherCycleTest, ParameterSweepValidation) {
    // Sweep through parameter ranges to ensure stability
    WeatherSystem::TimeOfDay time;
    
    for (uint32_t hour = 0; hour < 24; hour += 4) {
        for (uint32_t day = 1; day <= 365; day += 30) {
            time.hour = hour;
            time.dayOfYear = day;
            simulator_->setTimeOfDay(time);
            
            for (int weatherInt = 0; weatherInt <= static_cast<int>(WeatherSystem::WeatherType::SANDSTORM); weatherInt++) {
                auto weatherType = static_cast<WeatherSystem::WeatherType>(weatherInt);
                simulator_->setWeather(weatherType);
                
                EXPECT_TRUE(simulator_->isValidWeatherState()) 
                    << "Invalid state at hour=" << hour << ", day=" << day 
                    << ", weather=" << weatherInt;
            }
        }
    }
    
    std::cout << "Parameter sweep validation completed successfully" << std::endl;
}