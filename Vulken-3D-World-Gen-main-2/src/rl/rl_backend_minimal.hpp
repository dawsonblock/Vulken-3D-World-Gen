#pragma once
#include "rl_backend.hpp"
#include <vector>
#include <random>

namespace voxelvk::rl {

/**
 * Minimal CPU MLP backend for RL training
 * Simple feedforward network with Adam optimizer
 */
struct MLPConfig {
    uint32_t inputSize = 64;      // Observation space size
    uint32_t hiddenSize = 128;    // Hidden layer size
    uint32_t actionSize = 4;      // Action space size
    uint32_t numHiddenLayers = 2; // Number of hidden layers
    
    float learningRate = 3e-4f;   // Adam learning rate
    float beta1 = 0.9f;          // Adam beta1
    float beta2 = 0.999f;        // Adam beta2
    float epsilon = 1e-8f;       // Adam epsilon
    
    bool useValueHead = true;     // Separate value head
    bool useTanh = false;        // Use tanh activation instead of ReLU
};

class MinimalMLP : public IRLBackend {
public:
    MinimalMLP(const MLPConfig& config = {});
    ~MinimalMLP();
    
    // IRLBackend interface
    void forward(const std::vector<float>& observations,
                std::vector<float>& actions,
                std::vector<float>& values) override;
    
    bool update(const std::vector<float>& observations,
               const std::vector<float>& actions,
               const std::vector<float>& rewards,
               const std::vector<float>& values,
               const std::vector<float>& advantages) override;
    
    bool saveModel(const std::string& path) override;
    bool loadModel(const std::string& path) override;
    
    void setLearningRate(float lr) override { config_.learningRate = lr; }
    void setEpsilon(float eps) override { config_.epsilon = eps; }
    
    std::string getBackendName() const override { return "MinimalMLP"; }
    size_t getParameterCount() const override;
    bool hasTrainingCapability() const override { return true; }
    
    // Configuration access
    const MLPConfig& getConfig() const { return config_; }
    void setConfig(const MLPConfig& config);
    
    // Training statistics
    struct TrainingStats {
        uint32_t updateCount = 0;
        float lastPolicyLoss = 0.0f;
        float lastValueLoss = 0.0f;
        float averageReward = 0.0f;
        double totalTrainingTime = 0.0;
    };
    
    const TrainingStats& getTrainingStats() const { return stats_; }
    void resetStats() { stats_ = TrainingStats{}; }
    
private:
    MLPConfig config_;
    
    // Network weights
    std::vector<float> policyWeights_;
    std::vector<float> valueWeights_;
    
    // Adam optimizer state
    std::vector<float> policyMoments1_;
    std::vector<float> policyMoments2_;
    std::vector<float> valueMoments1_;
    std::vector<float> valueMoments2_;
    
    // Training state
    TrainingStats stats_;
    uint32_t adamStep_ = 0;
    
    // Random number generation
    std::mt19937 rng_;
    std::normal_distribution<float> weightInit_;
    
    // Internal methods
    void initializeWeights();
    void computePolicyForward(const std::vector<float>& input, std::vector<float>& output);
    void computeValueForward(const std::vector<float>& input, float& value);
    
    void computePolicyGradients(const std::vector<float>& observations,
                               const std::vector<float>& actions,
                               const std::vector<float>& advantages,
                               std::vector<float>& gradients);
                               
    void computeValueGradients(const std::vector<float>& observations,
                              const std::vector<float>& targets,
                              std::vector<float>& gradients);
    
    void applyAdamUpdate(std::vector<float>& weights,
                        std::vector<float>& moments1,
                        std::vector<float>& moments2,
                        const std::vector<float>& gradients);
    
    float activation(float x) const { return config_.useTanh ? std::tanh(x) : std::max(0.0f, x); }
    float activationDerivative(float x) const;
    
    size_t getPolicyWeightCount() const;
    size_t getValueWeightCount() const;
};

} // namespace voxelvk::rl