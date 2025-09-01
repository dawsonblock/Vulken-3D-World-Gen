#include "rl_backend.hpp"
#include <random>

namespace voxelvk::rl {

/**
 * Dummy RL backend for testing - produces random actions
 */
class DummyBackend : public IRLBackend {
private:
    mutable std::mt19937 rng_;
    mutable std::uniform_real_distribution<float> uniform_dist_;
    
public:
    DummyBackend() : rng_(1234), uniform_dist_(-1.0f, 1.0f) {}
    
    void forward(const std::vector<float>& observations,
                std::vector<float>& actions,
                std::vector<float>& values) override {
        (void)observations;
        
        // Generate random actions
        actions.resize(4); // 4D action space example
        for (auto& action : actions) {
            action = uniform_dist_(rng_);
        }
        
        // Random baseline value
        values.resize(1);
        values[0] = uniform_dist_(rng_) * 0.1f; // Small random baseline
    }
    
    std::string getBackendName() const override {
        return "DummyBackend";
    }
    
    size_t getParameterCount() const override {
        return 0; // No learnable parameters
    }
    
    bool hasTrainingCapability() const override {
        return false; // Dummy backend doesn't learn
    }
};

// Factory integration
std::unique_ptr<IRLBackend> createDummyBackend() {
    return std::make_unique<DummyBackend>();
}

} // namespace voxelvk::rl