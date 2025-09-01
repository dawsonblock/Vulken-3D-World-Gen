#pragma once
#include <vector>
#include <memory>

namespace voxelvk::rl {

/**
 * Reinforcement Learning Backend Interface
 * Pluggable interface for different NN backends (dummy, minimal MLP, PyTorch, ONNX)
 */
struct IRLBackend {
    virtual ~IRLBackend() = default;
    
    // Forward pass: observations -> actions + values
    virtual void forward(const std::vector<float>& observations,
                        std::vector<float>& actions,
                        std::vector<float>& values) = 0;
    
    // Training update: batch learning from collected experiences
    virtual bool update(const std::vector<float>& observations,
                       const std::vector<float>& actions,
                       const std::vector<float>& rewards,
                       const std::vector<float>& values,
                       const std::vector<float>& advantages) { 
        (void)observations; (void)actions; (void)rewards; (void)values; (void)advantages;
        return false; // Default: no training capability
    }
    
    // Model management
    virtual bool saveModel(const std::string& path) { (void)path; return false; }
    virtual bool loadModel(const std::string& path) { (void)path; return false; }
    
    // Configuration
    virtual void setLearningRate(float lr) { (void)lr; }
    virtual void setEpsilon(float eps) { (void)eps; }
    
    // Statistics
    virtual std::string getBackendName() const = 0;
    virtual size_t getParameterCount() const { return 0; }
    virtual bool hasTrainingCapability() const { return false; }
};

/**
 * Backend factory for different implementations
 */
class RLBackendFactory {
public:
    enum class BackendType {
        DUMMY,          // Random actions, no learning
        MINIMAL_MLP,    // Simple CPU MLP with SGD/Adam
        PYTORCH,        // LibTorch C++ backend
        ONNX           // ONNX Runtime inference
    };
    
    static std::unique_ptr<IRLBackend> create(BackendType type);
    static std::vector<BackendType> getAvailableBackends();
    static const char* backendTypeToString(BackendType type);
};

} // namespace voxelvk::rl