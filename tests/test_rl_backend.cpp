#include <iostream>
#include <cassert>
#include <memory>
#include <vector>
#include <filesystem>

// RL backend for testing
#include "../src/rl/rl_backend.hpp"
#include "../src/rl/rl_backend_minimal.hpp"

using namespace voxelvk::rl;

/**
 * Test RL backend systems
 */
int main() {
    std::cout << "Testing RL backend systems..." << std::endl;
    
    // Test 1: Backend factory
    auto dummyBackend = RLBackendFactory::create(RLBackendFactory::BackendType::DUMMY);
    assert(dummyBackend != nullptr);
    assert(dummyBackend->getBackendName() == "DummyBackend");
    assert(!dummyBackend->hasTrainingCapability());
    std::cout << "  ✓ Dummy backend creation and properties" << std::endl;
    
    auto mlpBackend = RLBackendFactory::create(RLBackendFactory::BackendType::MINIMAL_MLP);
    assert(mlpBackend != nullptr);
    assert(mlpBackend->getBackendName() == "MinimalMLP");
    assert(mlpBackend->hasTrainingCapability());
    std::cout << "  ✓ MLP backend creation and properties" << std::endl;
    
    // Test 2: Backend inference
    std::vector<float> obs(64, 0.5f); // 64D observation
    std::vector<float> actions, values;
    
    dummyBackend->forward(obs, actions, values);
    assert(actions.size() == 4); // 4D action space
    assert(values.size() == 1);  // Single value
    std::cout << "  ✓ Dummy backend forward pass" << std::endl;
    
    mlpBackend->forward(obs, actions, values);
    assert(actions.size() >= 1);
    assert(values.size() >= 1);
    std::cout << "  ✓ MLP backend forward pass" << std::endl;
    
    // Test 3: MLP configuration
    MLPConfig config;
    config.inputSize = 32;
    config.hiddenSize = 64;
    config.actionSize = 8;
    config.learningRate = 0.001f;
    
    auto customMLP = std::make_unique<MinimalMLP>(config);
    
    size_t expectedParams = (32 * 64) + (64 * 64) + (64 * 8) + // Weights
                           (64 * 2) + 8 + // Biases
                           (32 * 64) + 64 + 1; // Value network
    
    std::cout << "  Expected parameters: " << expectedParams << std::endl;
    std::cout << "  Actual parameters: " << customMLP->getParameterCount() << std::endl;
    
    assert(customMLP->getParameterCount() > 0);
    std::cout << "  ✓ MLP parameter counting" << std::endl;
    
    // Test 4: Training update
    std::vector<float> trainObs(32, 0.1f);
    std::vector<float> trainActions(8, 0.0f);
    std::vector<float> trainRewards{1.0f};
    std::vector<float> trainValues{0.5f};
    std::vector<float> trainAdvantages{0.5f};
    
    bool updateResult = customMLP->update(trainObs, trainActions, trainRewards, trainValues, trainAdvantages);
    assert(updateResult);
    
    auto stats = customMLP->getTrainingStats();
    assert(stats.updateCount > 0);
    std::cout << "  ✓ MLP training update" << std::endl;
    
    // Test 5: Model save/load
    std::string modelPath = "test_model.vxml";
    
    bool saveResult = customMLP->saveModel(modelPath);
    assert(saveResult);
    std::cout << "  ✓ Model saving" << std::endl;
    
    auto loadMLP = std::make_unique<MinimalMLP>();
    bool loadResult = loadMLP->loadModel(modelPath);
    assert(loadResult);
    
    assert(loadMLP->getParameterCount() == customMLP->getParameterCount());
    std::cout << "  ✓ Model loading" << std::endl;
    
    // Test 6: Available backends
    auto availableBackends = RLBackendFactory::getAvailableBackends();
    assert(availableBackends.size() >= 2); // At least dummy + minimal
    
    for (auto backendType : availableBackends) {
        const char* name = RLBackendFactory::backendTypeToString(backendType);
        std::cout << "  Available backend: " << name << std::endl;
    }
    
    // Cleanup test file
    std::filesystem::remove(modelPath);
    
    std::cout << "✅ RL backend test passed" << std::endl;
    return 0;
}