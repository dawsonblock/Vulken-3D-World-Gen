#pragma once

#include <vector>
#include <memory>
#include <string>
#include <fstream>
#include <glm/glm.hpp>
#include "../core/logger.hpp"

namespace voxelvk {

// Forward declarations
class WorldManager;
class PPOTrainer;

struct TrainingConfig {
    // Environment settings
    int num_envs = 256;
    int total_timesteps = 10000000;
    int steps_per_update = 2048;
    int max_episode_length = 2000;
    
    // PPO hyperparameters
    float learning_rate = 3e-4f;
    float clip_range = 0.2f;
    float entropy_coef = 0.01f;
    float value_coef = 0.5f;
    float max_grad_norm = 0.5f;
    int batch_size = 8192;
    int minibatch_size = 512;
    int num_epochs = 4;
    float gae_lambda = 0.95f;
    float gamma = 0.99f;
    float target_kl = 0.015f;
    bool use_mixed_precision = true;
    
    // Logging and checkpointing
    int log_frequency = 10;
    int checkpoint_frequency = 1000;
    int eval_frequency = 5000;
    std::string output_dir = "output";
    std::string load_checkpoint_path = "";
    
    // World settings
    std::string world_config_path = "config/world.yaml";
    int observation_radius = 32;
    bool use_gpu_generation = true;
};

class Trainer {
public:
    explicit Trainer(const TrainingConfig& config);
    ~Trainer();
    
    // Main training loop
    void train();
    
    // Training statistics
    struct Experience {
        std::vector<float> observations;
        std::vector<int> actions;
        std::vector<float> rewards;
        std::vector<bool> dones;
        std::vector<float> values;
        std::vector<float> log_probs;
    };

private:
    // Environment state for each parallel environment
    struct EnvironmentState {
        glm::vec3 agent_position;
        int episode_length = 0;
        float total_reward = 0.0f;
        bool is_done = false;
    };
    
    TrainingConfig config_;
    Logger logger_;
    
    // Core components
    std::unique_ptr<WorldManager> world_manager_;
    std::unique_ptr<PPOTrainer> ppo_trainer_;
    
    // Environment management
    std::vector<EnvironmentState> env_states_;
    int observation_dim_;
    int action_dim_;
    
    // Logging
    std::ofstream metrics_file_;
    
    // Initialization methods
    void initializeEnvironments();
    void initializePPOTrainer();
    void initializeLogging();
    
    // Training loop methods
    Experience collectExperience();
    std::vector<float> getObservation(int env_id);
    std::tuple<int, float, float> getAction(const std::vector<float>& observation);
    std::tuple<float, bool> stepEnvironment(int env_id, int action);
    float calculateReward(int env_id, int action);
    void resetEnvironment(int env_id);
    
    // Logging and evaluation
    void logTrainingStats(int timestep, int episode, 
                         const Experience& experience, 
                         const PPOTrainer::TrainingStats& training_stats);
    void saveCheckpoint(int timestep);
    void evaluatePolicy(int timestep);
    
    // Utility methods
    int calculateObservationDim();
    int calculateActionDim();
    void cleanup();
};

} // namespace voxelvk