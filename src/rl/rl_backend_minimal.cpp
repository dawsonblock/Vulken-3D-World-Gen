#include "rl_backend_minimal.hpp"
#include "../core/logger.hpp"
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace voxelvk::rl {

static Logger g_mlpLogger("MinimalMLP");

// Forward declaration
std::unique_ptr<IRLBackend> createDummyBackend();

MinimalMLP::MinimalMLP(const MLPConfig& config) 
    : config_(config), rng_(std::random_device{}()), weightInit_(0.0f, 0.1f) {
    
    g_mlpLogger.Info("MinimalMLP initialized");
    g_mlpLogger.Info("  Input size: {}", config_.inputSize);
    g_mlpLogger.Info("  Hidden size: {}", config_.hiddenSize);
    g_mlpLogger.Info("  Action size: {}", config_.actionSize);
    g_mlpLogger.Info("  Hidden layers: {}", config_.numHiddenLayers);
    g_mlpLogger.Info("  Learning rate: {}", config_.learningRate);
    g_mlpLogger.Info("  Activation: {}", config_.useTanh ? "tanh" : "ReLU");
    
    initializeWeights();
}

MinimalMLP::~MinimalMLP() {
    g_mlpLogger.Info("MinimalMLP destroyed");
}

void MinimalMLP::forward(const std::vector<float>& observations,
                        std::vector<float>& actions,
                        std::vector<float>& values) {
    if (observations.size() != config_.inputSize) {
        g_mlpLogger.Warn("Observation size mismatch: expected {}, got {}", 
            config_.inputSize, observations.size());
        
        // Resize and pad/truncate as needed
        std::vector<float> paddedObs(config_.inputSize, 0.0f);
        size_t copySize = std::min(observations.size(), static_cast<size_t>(config_.inputSize));
        std::copy(observations.begin(), observations.begin() + static_cast<ptrdiff_t>(copySize), paddedObs.begin());
        
        computePolicyForward(paddedObs, actions);
        
        float value;
        computeValueForward(paddedObs, value);
        values.assign(1, value);
    } else {
        computePolicyForward(observations, actions);
        
        float value;
        computeValueForward(observations, value);
        values.assign(1, value);
    }
}

bool MinimalMLP::update(const std::vector<float>& observations,
                       const std::vector<float>& actions,
                       const std::vector<float>& rewards,
                       const std::vector<float>& values,
                       const std::vector<float>& advantages) {
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Compute policy gradients
    std::vector<float> policyGradients(policyWeights_.size());
    computePolicyGradients(observations, actions, advantages, policyGradients);
    
    // Compute value gradients
    std::vector<float> valueGradients(valueWeights_.size());
    computeValueGradients(observations, rewards, valueGradients);
    
    // Apply Adam updates
    applyAdamUpdate(policyWeights_, policyMoments1_, policyMoments2_, policyGradients);
    applyAdamUpdate(valueWeights_, valueMoments1_, valueMoments2_, valueGradients);
    
    adamStep_++;
    stats_.updateCount++;
    
    // Calculate losses for monitoring (simplified)
    stats_.lastPolicyLoss = 0.0f;
    for (float grad : policyGradients) {
        stats_.lastPolicyLoss += grad * grad;
    }
    stats_.lastPolicyLoss = std::sqrt(stats_.lastPolicyLoss / static_cast<float>(policyGradients.size()));
    
    stats_.lastValueLoss = 0.0f;
    for (float grad : valueGradients) {
        stats_.lastValueLoss += grad * grad;
    }
    stats_.lastValueLoss = std::sqrt(stats_.lastValueLoss / static_cast<float>(valueGradients.size()));
    
    // Update average reward
    if (!rewards.empty()) {
        float avgReward = 0.0f;
        for (float reward : rewards) avgReward += reward;
    avgReward /= static_cast<float>(rewards.size());
        
        const float alpha = 0.01f;
        stats_.averageReward = stats_.averageReward * (1.0f - alpha) + avgReward * alpha;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    double updateTime = std::chrono::duration<double>(endTime - startTime).count();
    stats_.totalTrainingTime += updateTime;
    
    g_mlpLogger.Debug("Update {}: policy_loss={:.6f}, value_loss={:.6f}, avg_reward={:.3f}",
        stats_.updateCount, stats_.lastPolicyLoss, stats_.lastValueLoss, stats_.averageReward);
    
    return true;
}

size_t MinimalMLP::getParameterCount() const {
    return policyWeights_.size() + valueWeights_.size();
}

void MinimalMLP::setConfig(const MLPConfig& config) {
    if (config.inputSize != config_.inputSize ||
        config.hiddenSize != config_.hiddenSize ||
        config.actionSize != config_.actionSize ||
        config.numHiddenLayers != config_.numHiddenLayers) {
        
        g_mlpLogger.Info("Network architecture changed - reinitializing weights");
        config_ = config;
        initializeWeights();
    } else {
        config_ = config;
    }
}

void MinimalMLP::initializeWeights() {
    g_mlpLogger.Debug("Initializing MLP weights...");
    
    // Calculate weight sizes
    size_t policyWeightCount = getPolicyWeightCount();
    size_t valueWeightCount = getValueWeightCount();
    
    // Initialize policy network weights
    policyWeights_.resize(policyWeightCount);
    for (float& weight : policyWeights_) {
        weight = weightInit_(rng_);
    }
    
    // Initialize value network weights
    valueWeights_.resize(valueWeightCount);
    for (float& weight : valueWeights_) {
        weight = weightInit_(rng_);
    }
    
    // Initialize Adam optimizer state
    policyMoments1_.assign(policyWeightCount, 0.0f);
    policyMoments2_.assign(policyWeightCount, 0.0f);
    valueMoments1_.assign(valueWeightCount, 0.0f);
    valueMoments2_.assign(valueWeightCount, 0.0f);
    
    adamStep_ = 0;
    
    g_mlpLogger.Info("Weights initialized: {} policy + {} value = {} total parameters",
        policyWeightCount, valueWeightCount, getParameterCount());
}

void MinimalMLP::computePolicyForward(const std::vector<float>& input, std::vector<float>& output) {
    // Simple feedforward implementation
    std::vector<float> hidden1(config_.hiddenSize);
    std::vector<float> hidden2(config_.hiddenSize);
    
    // Input -> Hidden1
    for (uint32_t h = 0; h < config_.hiddenSize; h++) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < config_.inputSize; i++) {
            size_t weightIdx = h * config_.inputSize + i;
            if (weightIdx < policyWeights_.size()) {
                sum += input[i] * policyWeights_[weightIdx];
            }
        }
        hidden1[h] = activation(sum);
    }
    
    // Hidden1 -> Hidden2
    size_t hidden1WeightOffset = config_.hiddenSize * config_.inputSize;
    for (uint32_t h = 0; h < config_.hiddenSize; h++) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < config_.hiddenSize; i++) {
            size_t weightIdx = hidden1WeightOffset + h * config_.hiddenSize + i;
            if (weightIdx < policyWeights_.size()) {
                sum += hidden1[i] * policyWeights_[weightIdx];
            }
        }
        hidden2[h] = activation(sum);
    }
    
    // Hidden2 -> Output
    output.resize(config_.actionSize);
    size_t outputWeightOffset = hidden1WeightOffset + config_.hiddenSize * config_.hiddenSize;
    for (uint32_t a = 0; a < config_.actionSize; a++) {
        float sum = 0.0f;
        for (uint32_t h = 0; h < config_.hiddenSize; h++) {
            size_t weightIdx = outputWeightOffset + a * config_.hiddenSize + h;
            if (weightIdx < policyWeights_.size()) {
                sum += hidden2[h] * policyWeights_[weightIdx];
            }
        }
        output[a] = std::tanh(sum); // Actions in [-1, 1]
    }
}

void MinimalMLP::computeValueForward(const std::vector<float>& input, float& value) {
    // Simplified value network (shared hidden layers with policy)
    std::vector<float> hidden(config_.hiddenSize);
    
    // Input -> Hidden
    for (uint32_t h = 0; h < config_.hiddenSize; h++) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < config_.inputSize; i++) {
            size_t weightIdx = h * config_.inputSize + i;
            if (weightIdx < valueWeights_.size()) {
                sum += input[i] * valueWeights_[weightIdx];
            }
        }
        hidden[h] = activation(sum);
    }
    
    // Hidden -> Value (single output)
    value = 0.0f;
    size_t valueWeightOffset = config_.hiddenSize * config_.inputSize;
    for (uint32_t h = 0; h < config_.hiddenSize; h++) {
        size_t weightIdx = valueWeightOffset + h;
        if (weightIdx < valueWeights_.size()) {
            value += hidden[h] * valueWeights_[weightIdx];
        }
    }
    
    // No activation for value output (can be negative)
}

void MinimalMLP::computePolicyGradients(const std::vector<float>& observations,
                                       const std::vector<float>& actions,
                                       const std::vector<float>& advantages,
                                       std::vector<float>& gradients) {
    // Simplified gradient computation (placeholder for real PPO gradients)
    gradients.assign(policyWeights_.size(), 0.0f);
    
    // Very basic gradient estimate
    for (size_t i = 0; i < gradients.size() && i < advantages.size(); i++) {
        gradients[i] = advantages[i] * config_.learningRate * 0.01f;
    }
}

void MinimalMLP::computeValueGradients(const std::vector<float>& observations,
                                      const std::vector<float>& targets,
                                      std::vector<float>& gradients) {
    // Simplified value gradients
    gradients.assign(valueWeights_.size(), 0.0f);
    
    // Basic gradient estimate
    for (size_t i = 0; i < gradients.size() && i < targets.size(); i++) {
        gradients[i] = targets[i] * config_.learningRate * 0.01f;
    }
}

void MinimalMLP::applyAdamUpdate(std::vector<float>& weights,
                                std::vector<float>& moments1,
                                std::vector<float>& moments2,
                                const std::vector<float>& gradients) {
    if (weights.size() != gradients.size() || 
        moments1.size() != gradients.size() || 
        moments2.size() != gradients.size()) {
        g_mlpLogger.Error("Adam update size mismatch");
        return;
    }
    
    float lr = config_.learningRate;
    float beta1 = config_.beta1;
    float beta2 = config_.beta2;
    float epsilon = config_.epsilon;
    
    // Bias correction
    float bias1Correction = 1.0f - std::pow(beta1, static_cast<float>(adamStep_ + 1));
    float bias2Correction = 1.0f - std::pow(beta2, static_cast<float>(adamStep_ + 1));
    
    for (size_t i = 0; i < weights.size(); i++) {
        // Update biased first moment estimate
        moments1[i] = beta1 * moments1[i] + (1.0f - beta1) * gradients[i];
        
        // Update biased second raw moment estimate  
        moments2[i] = beta2 * moments2[i] + (1.0f - beta2) * gradients[i] * gradients[i];
        
        // Compute bias-corrected first moment estimate
        float m1_hat = moments1[i] / bias1Correction;
        
        // Compute bias-corrected second raw moment estimate
        float m2_hat = moments2[i] / bias2Correction;
        
        // Update weights
        weights[i] -= lr * m1_hat / (std::sqrt(m2_hat) + epsilon);
    }
}

float MinimalMLP::activationDerivative(float x) const {
    if (config_.useTanh) {
        float t = std::tanh(x);
        return 1.0f - t * t; // d/dx tanh(x) = 1 - tanh^2(x)
    } else {
        return x > 0.0f ? 1.0f : 0.0f; // d/dx ReLU(x)
    }
}

size_t MinimalMLP::getPolicyWeightCount() const {
    size_t count = 0;
    
    // Input -> Hidden1
    count += config_.inputSize * config_.hiddenSize;
    
    // Hidden layers
    for (uint32_t layer = 1; layer < config_.numHiddenLayers; layer++) {
        count += config_.hiddenSize * config_.hiddenSize;
    }
    
    // Hidden -> Output
    count += config_.hiddenSize * config_.actionSize;
    
    // Add bias terms
    count += config_.hiddenSize * config_.numHiddenLayers; // Hidden biases
    count += config_.actionSize; // Output bias
    
    return count;
}

size_t MinimalMLP::getValueWeightCount() const {
    size_t count = 0;
    
    // Input -> Hidden
    count += config_.inputSize * config_.hiddenSize;
    
    // Hidden -> Value (single output)
    count += config_.hiddenSize * 1;
    
    // Add bias terms
    count += config_.hiddenSize + 1; // Hidden bias + value bias
    
    return count;
}

bool MinimalMLP::saveModel(const std::string& path) {
    g_mlpLogger.Info("Saving model to: {}", path);
    
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        g_mlpLogger.Error("Failed to open model file for writing: {}", path);
        return false;
    }
    
    // Write header
    file.write("VXML", 4); // Magic number
    uint32_t version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    
    // Write config
    file.write(reinterpret_cast<const char*>(&config_), sizeof(config_));
    
    // Write training stats
    file.write(reinterpret_cast<const char*>(&stats_), sizeof(stats_));
    
    // Write policy weights
    uint32_t policySize = static_cast<uint32_t>(policyWeights_.size());
    file.write(reinterpret_cast<const char*>(&policySize), sizeof(policySize));
    file.write(reinterpret_cast<const char*>(policyWeights_.data()), policySize * sizeof(float));
    
    // Write value weights
    uint32_t valueSize = static_cast<uint32_t>(valueWeights_.size());
    file.write(reinterpret_cast<const char*>(&valueSize), sizeof(valueSize));
    file.write(reinterpret_cast<const char*>(valueWeights_.data()), valueSize * sizeof(float));
    
    if (!file.good()) {
        g_mlpLogger.Error("Error writing model file");
        return false;
    }
    
    g_mlpLogger.Info("Model saved successfully: {} parameters", getParameterCount());
    return true;
}

bool MinimalMLP::loadModel(const std::string& path) {
    g_mlpLogger.Info("Loading model from: {}", path);
    
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        g_mlpLogger.Error("Failed to open model file for reading: {}", path);
        return false;
    }
    
    // Read and validate header
    char magic[4];
    file.read(magic, 4);
    if (std::string(magic, 4) != "VXML") {
        g_mlpLogger.Error("Invalid model file format");
        return false;
    }
    
    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != 1) {
        g_mlpLogger.Error("Unsupported model version: {}", version);
        return false;
    }
    
    // Read config
    MLPConfig loadedConfig;
    file.read(reinterpret_cast<char*>(&loadedConfig), sizeof(loadedConfig));
    
    // Read training stats
    file.read(reinterpret_cast<char*>(&stats_), sizeof(stats_));
    
    // Read weights
    uint32_t policySize, valueSize;
    file.read(reinterpret_cast<char*>(&policySize), sizeof(policySize));
    
    policyWeights_.resize(policySize);
    file.read(reinterpret_cast<char*>(policyWeights_.data()), policySize * sizeof(float));
    
    file.read(reinterpret_cast<char*>(&valueSize), sizeof(valueSize));
    valueWeights_.resize(valueSize);
    file.read(reinterpret_cast<char*>(valueWeights_.data()), valueSize * sizeof(float));
    
    if (!file.good()) {
        g_mlpLogger.Error("Error reading model file");
        return false;
    }
    
    // Update config and reinitialize optimizer state
    config_ = loadedConfig;
    policyMoments1_.assign(policyWeights_.size(), 0.0f);
    policyMoments2_.assign(policyWeights_.size(), 0.0f);
    valueMoments1_.assign(valueWeights_.size(), 0.0f);
    valueMoments2_.assign(valueWeights_.size(), 0.0f);
    
    g_mlpLogger.Info("Model loaded successfully: {} parameters", getParameterCount());
    return true;
}

// Factory implementation
std::unique_ptr<IRLBackend> RLBackendFactory::create(BackendType type) {
    switch (type) {
        case BackendType::DUMMY:
            return createDummyBackend();
        case BackendType::MINIMAL_MLP:
            return std::make_unique<MinimalMLP>();
        case BackendType::PYTORCH:
            // TODO: Implement PyTorch backend
            g_mlpLogger.Warn("PyTorch backend not implemented, falling back to dummy");
            return createDummyBackend();
        case BackendType::ONNX:
            // TODO: Implement ONNX backend
            g_mlpLogger.Warn("ONNX backend not implemented, falling back to dummy");
            return createDummyBackend();
        default:
            g_mlpLogger.Error("Unknown backend type, using dummy");
            return createDummyBackend();
    }
}

std::vector<RLBackendFactory::BackendType> RLBackendFactory::getAvailableBackends() {
    return {BackendType::DUMMY, BackendType::MINIMAL_MLP};
}

const char* RLBackendFactory::backendTypeToString(BackendType type) {
    switch (type) {
        case BackendType::DUMMY: return "Dummy";
        case BackendType::MINIMAL_MLP: return "MinimalMLP";
        case BackendType::PYTORCH: return "PyTorch";
        case BackendType::ONNX: return "ONNX";
        default: return "Unknown";
    }
}

} // namespace voxelvk::rl