#include "Trainer.hpp"
#include "../env/world_manager.hpp"
#include "../core/logger.hpp"
#include "../core/timer.hpp"
#include "../core/nvtx_profiler.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <random>
#include <chrono>
#include <filesystem>

namespace voxelvk {

// PPO Trainer Implementation
class PPOTrainer {
public:
    struct Config {
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
    };

    struct TrainingStats {
        float policy_loss = 0.0f;
        float value_loss = 0.0f;
        float entropy_loss = 0.0f;
        float total_loss = 0.0f;
        float kl_divergence = 0.0f;
        float explained_variance = 0.0f;
        float grad_norm = 0.0f;
        int updates_count = 0;
        float learning_rate = 0.0f;
    };

    PPOTrainer(const Config& config, int num_envs, int obs_dim, int action_dim)
        : config_(config), num_envs_(num_envs), obs_dim_(obs_dim), action_dim_(action_dim) {
        
        // Initialize policy and value networks
        initializeNetworks();
        
        // Initialize optimizers
        initializeOptimizers();
        
        // Allocate buffers for training data
        allocateBuffers();
        
        logger_.Info("PPO Trainer initialized with {} environments", num_envs);
    }

    ~PPOTrainer() {
        cleanup();
    }

    TrainingStats update(const std::vector<float>& observations,
                        const std::vector<int>& actions,
                        const std::vector<float>& rewards,
                        const std::vector<bool>& dones,
                        const std::vector<float>& values,
                        const std::vector<float>& log_probs) {
        
        NVTX_RANGE("PPO_Update");
        
        TrainingStats stats;
        
        // Compute advantages using GAE
        std::vector<float> advantages = computeAdvantages(rewards, values, dones);
        std::vector<float> returns = computeReturns(rewards, values, dones);
        
        // Normalize advantages
        normalizeAdvantages(advantages);
        
        // Perform PPO updates
        for (int epoch = 0; epoch < config_.num_epochs; ++epoch) {
            auto epoch_stats = performEpochUpdate(observations, actions, rewards, 
                                                advantages, returns, log_probs);
            
            // Accumulate stats
            stats.policy_loss += epoch_stats.policy_loss;
            stats.value_loss += epoch_stats.value_loss;
            stats.entropy_loss += epoch_stats.entropy_loss;
            stats.total_loss += epoch_stats.total_loss;
            stats.kl_divergence += epoch_stats.kl_divergence;
            stats.grad_norm += epoch_stats.grad_norm;
            
            // Early stopping if KL divergence is too high
            if (epoch_stats.kl_divergence > config_.target_kl * 1.5f) {
                logger_.Warn("Early stopping at epoch {} due to high KL divergence: {}", 
                           epoch, epoch_stats.kl_divergence);
                break;
            }
        }
        
        // Average stats over epochs
        stats.policy_loss /= config_.num_epochs;
        stats.value_loss /= config_.num_epochs;
        stats.entropy_loss /= config_.num_epochs;
        stats.total_loss /= config_.num_epochs;
        stats.kl_divergence /= config_.num_epochs;
        stats.grad_norm /= config_.num_epochs;
        stats.updates_count = ++total_updates_;
        stats.learning_rate = current_learning_rate_;
        
        // Update learning rate schedule
        updateLearningRate();
        
        return stats;
    }

    bool saveCheckpoint(const std::string& path) {
        try {
            // Create directory if it doesn't exist
            std::filesystem::create_directories(std::filesystem::path(path).parent_path());
            
            // Save model state (simplified - in practice would use PyTorch/TensorFlow serialization)
            std::ofstream file(path, std::ios::binary);
            if (!file.is_open()) {
                logger_.Error("Failed to open checkpoint file for writing: {}", path);
                return false;
            }
            
            // Write header
            uint32_t version = 1;
            file.write(reinterpret_cast<const char*>(&version), sizeof(version));
            file.write(reinterpret_cast<const char*>(&total_updates_), sizeof(total_updates_));
            file.write(reinterpret_cast<const char*>(&current_learning_rate_), sizeof(current_learning_rate_));
            
            // Write network parameters (placeholder - would serialize actual network weights)
            for (const auto& param : policy_network_params_) {
                file.write(reinterpret_cast<const char*>(param.data()), param.size() * sizeof(float));
            }
            
            for (const auto& param : value_network_params_) {
                file.write(reinterpret_cast<const char*>(param.data()), param.size() * sizeof(float));
            }
            
            file.close();
            logger_.Info("Checkpoint saved to: {}", path);
            return true;
            
        } catch (const std::exception& e) {
            logger_.Error("Exception while saving checkpoint: {}", e.what());
            return false;
        }
    }

    bool loadCheckpoint(const std::string& path) {
        try {
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open()) {
                logger_.Error("Failed to open checkpoint file for reading: {}", path);
                return false;
            }
            
            // Read header
            uint32_t version;
            file.read(reinterpret_cast<char*>(&version), sizeof(version));
            if (version != 1) {
                logger_.Error("Unsupported checkpoint version: {}", version);
                return false;
            }
            
            file.read(reinterpret_cast<char*>(&total_updates_), sizeof(total_updates_));
            file.read(reinterpret_cast<char*>(&current_learning_rate_), sizeof(current_learning_rate_));
            
            // Read network parameters
            for (auto& param : policy_network_params_) {
                file.read(reinterpret_cast<char*>(param.data()), param.size() * sizeof(float));
            }
            
            for (auto& param : value_network_params_) {
                file.read(reinterpret_cast<char*>(param.data()), param.size() * sizeof(float));
            }
            
            file.close();
            logger_.Info("Checkpoint loaded from: {}", path);
            return true;
            
        } catch (const std::exception& e) {
            logger_.Error("Exception while loading checkpoint: {}", e.what());
            return false;
        }
    }

private:
    Config config_;
    int num_envs_;
    int obs_dim_;
    int action_dim_;
    int total_updates_ = 0;
    float current_learning_rate_;
    
    // Network parameters (simplified - in practice would use actual neural network framework)
    std::vector<std::vector<float>> policy_network_params_;
    std::vector<std::vector<float>> value_network_params_;
    
    // Training buffers
    std::vector<float> advantage_buffer_;
    std::vector<float> return_buffer_;
    
    Logger logger_{"PPOTrainer"};

    void initializeNetworks() {
        // Initialize policy network (simplified)
        policy_network_params_.resize(4); // 4 layers
        policy_network_params_[0].resize(obs_dim_ * 256, 0.01f); // Input -> Hidden1
        policy_network_params_[1].resize(256 * 256, 0.01f);      // Hidden1 -> Hidden2
        policy_network_params_[2].resize(256 * 128, 0.01f);      // Hidden2 -> Hidden3
        policy_network_params_[3].resize(128 * action_dim_, 0.01f); // Hidden3 -> Output
        
        // Initialize value network (simplified)
        value_network_params_.resize(4);
        value_network_params_[0].resize(obs_dim_ * 256, 0.01f);
        value_network_params_[1].resize(256 * 256, 0.01f);
        value_network_params_[2].resize(256 * 128, 0.01f);
        value_network_params_[3].resize(128 * 1, 0.01f); // Single value output
        
        // Xavier initialization
        std::random_device rd;
        std::mt19937 gen(rd());
        
        for (auto& layer : policy_network_params_) {
            float std_dev = std::sqrt(2.0f / layer.size());
            std::normal_distribution<float> dist(0.0f, std_dev);
            for (auto& weight : layer) {
                weight = dist(gen);
            }
        }
        
        for (auto& layer : value_network_params_) {
            float std_dev = std::sqrt(2.0f / layer.size());
            std::normal_distribution<float> dist(0.0f, std_dev);
            for (auto& weight : layer) {
                weight = dist(gen);
            }
        }
        
        current_learning_rate_ = config_.learning_rate;
    }

    void initializeOptimizers() {
        // Initialize Adam optimizers (simplified)
        logger_.Info("Initialized Adam optimizers with learning rate: {}", config_.learning_rate);
    }

    void allocateBuffers() {
        advantage_buffer_.reserve(num_envs_ * 2048); // Max episode length
        return_buffer_.reserve(num_envs_ * 2048);
    }

    std::vector<float> computeAdvantages(const std::vector<float>& rewards,
                                       const std::vector<float>& values,
                                       const std::vector<bool>& dones) {
        NVTX_RANGE("Compute_Advantages");
        
        std::vector<float> advantages;
        advantages.reserve(rewards.size());
        
        float gae = 0.0f;
        for (int t = rewards.size() - 1; t >= 0; --t) {
            float delta;
            if (t == rewards.size() - 1 || dones[t]) {
                delta = rewards[t] - values[t];
                gae = delta;
            } else {
                delta = rewards[t] + config_.gamma * values[t + 1] - values[t];
                gae = delta + config_.gamma * config_.gae_lambda * gae;
            }
            advantages.insert(advantages.begin(), gae);
        }
        
        return advantages;
    }

    std::vector<float> computeReturns(const std::vector<float>& rewards,
                                    const std::vector<float>& values,
                                    const std::vector<bool>& dones) {
        NVTX_RANGE("Compute_Returns");
        
        std::vector<float> returns;
        returns.reserve(rewards.size());
        
        float running_return = 0.0f;
        for (int t = rewards.size() - 1; t >= 0; --t) {
            if (dones[t]) {
                running_return = rewards[t];
            } else {
                running_return = rewards[t] + config_.gamma * running_return;
            }
            returns.insert(returns.begin(), running_return);
        }
        
        return returns;
    }

    void normalizeAdvantages(std::vector<float>& advantages) {
        if (advantages.empty()) return;
        
        // Compute mean and std
        float mean = std::accumulate(advantages.begin(), advantages.end(), 0.0f) / advantages.size();
        float variance = 0.0f;
        for (float adv : advantages) {
            variance += (adv - mean) * (adv - mean);
        }
        variance /= advantages.size();
        float std_dev = std::sqrt(variance + 1e-8f);
        
        // Normalize
        for (auto& adv : advantages) {
            adv = (adv - mean) / std_dev;
        }
    }

    TrainingStats performEpochUpdate(const std::vector<float>& observations,
                                   const std::vector<int>& actions,
                                   const std::vector<float>& rewards,
                                   const std::vector<float>& advantages,
                                   const std::vector<float>& returns,
                                   const std::vector<float>& old_log_probs) {
        NVTX_RANGE("PPO_Epoch_Update");
        
        TrainingStats stats;
        
        // Create minibatches
        int num_samples = observations.size() / obs_dim_;
        int num_minibatches = num_samples / config_.minibatch_size;
        
        std::vector<int> indices(num_samples);
        std::iota(indices.begin(), indices.end(), 0);
        
        // Shuffle indices for minibatch sampling
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(indices.begin(), indices.end(), gen);
        
        for (int mb = 0; mb < num_minibatches; ++mb) {
            int start_idx = mb * config_.minibatch_size;
            int end_idx = std::min(start_idx + config_.minibatch_size, num_samples);
            
            // Extract minibatch data
            std::vector<float> mb_obs, mb_advantages, mb_returns, mb_old_log_probs;
            std::vector<int> mb_actions;
            
            for (int i = start_idx; i < end_idx; ++i) {
                int idx = indices[i];
                
                // Copy observation
                for (int j = 0; j < obs_dim_; ++j) {
                    mb_obs.push_back(observations[idx * obs_dim_ + j]);
                }
                
                mb_actions.push_back(actions[idx]);
                mb_advantages.push_back(advantages[idx]);
                mb_returns.push_back(returns[idx]);
                mb_old_log_probs.push_back(old_log_probs[idx]);
            }
            
            // Perform minibatch update
            auto mb_stats = updateMinibatch(mb_obs, mb_actions, mb_advantages, 
                                          mb_returns, mb_old_log_probs);
            
            // Accumulate stats
            stats.policy_loss += mb_stats.policy_loss;
            stats.value_loss += mb_stats.value_loss;
            stats.entropy_loss += mb_stats.entropy_loss;
            stats.total_loss += mb_stats.total_loss;
            stats.kl_divergence += mb_stats.kl_divergence;
            stats.grad_norm += mb_stats.grad_norm;
        }
        
        // Average over minibatches
        if (num_minibatches > 0) {
            stats.policy_loss /= num_minibatches;
            stats.value_loss /= num_minibatches;
            stats.entropy_loss /= num_minibatches;
            stats.total_loss /= num_minibatches;
            stats.kl_divergence /= num_minibatches;
            stats.grad_norm /= num_minibatches;
        }
        
        return stats;
    }

    TrainingStats updateMinibatch(const std::vector<float>& observations,
                                const std::vector<int>& actions,
                                const std::vector<float>& advantages,
                                const std::vector<float>& returns,
                                const std::vector<float>& old_log_probs) {
        TrainingStats stats;
        
        // Forward pass through networks (simplified)
        auto [new_log_probs, values, entropy] = forwardPass(observations, actions);
        
        // Compute losses
        stats.policy_loss = computePolicyLoss(new_log_probs, old_log_probs, advantages);
        stats.value_loss = computeValueLoss(values, returns);
        stats.entropy_loss = computeEntropyLoss(entropy);
        stats.total_loss = stats.policy_loss + config_.value_coef * stats.value_loss - 
                          config_.entropy_coef * stats.entropy_loss;
        
        // Compute KL divergence
        stats.kl_divergence = computeKLDivergence(new_log_probs, old_log_probs);
        
        // Backward pass and optimization (simplified)
        stats.grad_norm = performBackwardPass(stats.total_loss);
        
        return stats;
    }

    std::tuple<std::vector<float>, std::vector<float>, float> 
    forwardPass(const std::vector<float>& observations, const std::vector<int>& actions) {
        // Simplified forward pass - in practice would use actual neural network framework
        int batch_size = observations.size() / obs_dim_;
        
        std::vector<float> log_probs(batch_size, -1.0f); // Simplified log probabilities
        std::vector<float> values(batch_size, 0.5f);     // Simplified values
        float entropy = 1.0f;                            // Simplified entropy
        
        return {log_probs, values, entropy};
    }

    float computePolicyLoss(const std::vector<float>& new_log_probs,
                          const std::vector<float>& old_log_probs,
                          const std::vector<float>& advantages) {
        float loss = 0.0f;
        
        for (size_t i = 0; i < new_log_probs.size(); ++i) {
            float ratio = std::exp(new_log_probs[i] - old_log_probs[i]);
            float clipped_ratio = std::clamp(ratio, 1.0f - config_.clip_range, 1.0f + config_.clip_range);
            
            float policy_loss1 = -advantages[i] * ratio;
            float policy_loss2 = -advantages[i] * clipped_ratio;
            
            loss += std::max(policy_loss1, policy_loss2);
        }
        
        return loss / new_log_probs.size();
    }

    float computeValueLoss(const std::vector<float>& values,
                         const std::vector<float>& returns) {
        float loss = 0.0f;
        
        for (size_t i = 0; i < values.size(); ++i) {
            float diff = values[i] - returns[i];
            loss += diff * diff;
        }
        
        return loss / values.size();
    }

    float computeEntropyLoss(float entropy) {
        return entropy;
    }

    float computeKLDivergence(const std::vector<float>& new_log_probs,
                            const std::vector<float>& old_log_probs) {
        float kl = 0.0f;
        
        for (size_t i = 0; i < new_log_probs.size(); ++i) {
            kl += old_log_probs[i] - new_log_probs[i];
        }
        
        return kl / new_log_probs.size();
    }

    float performBackwardPass(float loss) {
        // Simplified gradient computation and parameter updates
        // In practice, would use automatic differentiation framework
        
        float grad_norm = std::sqrt(loss); // Simplified gradient norm
        
        // Clip gradients
        if (grad_norm > config_.max_grad_norm) {
            grad_norm = config_.max_grad_norm;
        }
        
        // Update parameters (simplified SGD step)
        for (auto& layer : policy_network_params_) {
            for (auto& weight : layer) {
                weight -= current_learning_rate_ * grad_norm * 0.001f; // Simplified gradient
            }
        }
        
        for (auto& layer : value_network_params_) {
            for (auto& weight : layer) {
                weight -= current_learning_rate_ * grad_norm * 0.001f; // Simplified gradient
            }
        }
        
        return grad_norm;
    }

    void updateLearningRate() {
        // Linear decay
        float decay_rate = 0.99999f;
        current_learning_rate_ *= decay_rate;
        current_learning_rate_ = std::max(current_learning_rate_, config_.learning_rate * 0.1f);
    }

    void cleanup() {
        // Cleanup resources
        policy_network_params_.clear();
        value_network_params_.clear();
        advantage_buffer_.clear();
        return_buffer_.clear();
    }
};

// Main Trainer class implementation
Trainer::Trainer(const TrainingConfig& config) 
    : config_(config), logger_("Trainer") {
    
    logger_.Info("Initializing VoxelRL Trainer with {} environments", config.num_envs);
    
    // Initialize world manager
    world_manager_ = std::make_unique<WorldManager>();
    if (!world_manager_->Initialize()) {
        throw std::runtime_error("Failed to initialize WorldManager");
    }
    
    // Initialize vectorized environments
    initializeEnvironments();
    
    // Initialize PPO trainer
    initializePPOTrainer();
    
    // Initialize logging and metrics
    initializeLogging();
    
    logger_.Info("Trainer initialization complete");
}

Trainer::~Trainer() {
    cleanup();
}

void Trainer::train() {
    logger_.Info("Starting training for {} timesteps", config_.total_timesteps);
    
    Timer training_timer;
    training_timer.Start();
    
    int current_timestep = 0;
    int episode_count = 0;
    
    // Main training loop
    while (current_timestep < config_.total_timesteps) {
        NVTX_RANGE("Training_Episode");
        
        // Collect experience from environments
        auto experience = collectExperience();
        current_timestep += experience.observations.size() / observation_dim_;
        episode_count++;
        
        // Update policy using PPO
        auto training_stats = ppo_trainer_->update(
            experience.observations,
            experience.actions,
            experience.rewards,
            experience.dones,
            experience.values,
            experience.log_probs
        );
        
        // Log training progress
        if (episode_count % config_.log_frequency == 0) {
            logTrainingStats(current_timestep, episode_count, experience, training_stats);
        }
        
        // Save checkpoint
        if (episode_count % config_.checkpoint_frequency == 0) {
            saveCheckpoint(current_timestep);
        }
        
        // Evaluate policy
        if (episode_count % config_.eval_frequency == 0) {
            evaluatePolicy(current_timestep);
        }
    }
    
    training_timer.Stop();
    
    logger_.Info("Training completed in {:.2f} seconds", training_timer.GetElapsedMs() / 1000.0f);
    logger_.Info("Final checkpoint saved");
    
    // Save final checkpoint
    saveCheckpoint(current_timestep);
}

void Trainer::initializeEnvironments() {
    logger_.Info("Initializing {} vectorized environments", config_.num_envs);
    
    // Calculate observation and action dimensions
    observation_dim_ = calculateObservationDim();
    action_dim_ = calculateActionDim();
    
    logger_.Info("Observation dimension: {}, Action dimension: {}", observation_dim_, action_dim_);
    
    // Initialize environment states
    env_states_.resize(config_.num_envs);
    for (int i = 0; i < config_.num_envs; ++i) {
        env_states_[i].episode_length = 0;
        env_states_[i].total_reward = 0.0f;
        env_states_[i].is_done = false;
        
        // Initialize agent position in world
        env_states_[i].agent_position = {
            static_cast<float>(i * 64), // Spread agents across world
            64.0f,
            static_cast<float>(i * 64)
        };
    }
    
    logger_.Info("Environment initialization complete");
}

void Trainer::initializePPOTrainer() {
    logger_.Info("Initializing PPO trainer");
    
    PPOTrainer::Config ppo_config;
    ppo_config.learning_rate = config_.learning_rate;
    ppo_config.clip_range = config_.clip_range;
    ppo_config.entropy_coef = config_.entropy_coef;
    ppo_config.value_coef = config_.value_coef;
    ppo_config.max_grad_norm = config_.max_grad_norm;
    ppo_config.batch_size = config_.batch_size;
    ppo_config.minibatch_size = config_.minibatch_size;
    ppo_config.num_epochs = config_.num_epochs;
    ppo_config.gae_lambda = config_.gae_lambda;
    ppo_config.gamma = config_.gamma;
    ppo_config.target_kl = config_.target_kl;
    ppo_config.use_mixed_precision = config_.use_mixed_precision;
    
    ppo_trainer_ = std::make_unique<PPOTrainer>(ppo_config, config_.num_envs, 
                                               observation_dim_, action_dim_);
    
    // Load checkpoint if specified
    if (!config_.load_checkpoint_path.empty()) {
        if (ppo_trainer_->loadCheckpoint(config_.load_checkpoint_path)) {
            logger_.Info("Loaded checkpoint from: {}", config_.load_checkpoint_path);
        } else {
            logger_.Warn("Failed to load checkpoint, starting from scratch");
        }
    }
    
    logger_.Info("PPO trainer initialization complete");
}

void Trainer::initializeLogging() {
    logger_.Info("Initializing logging and metrics");
    
    // Create output directories
    std::filesystem::create_directories(config_.output_dir + "/logs");
    std::filesystem::create_directories(config_.output_dir + "/checkpoints");
    std::filesystem::create_directories(config_.output_dir + "/metrics");
    
    // Initialize metrics file
    metrics_file_.open(config_.output_dir + "/metrics/training_metrics.csv");
    if (metrics_file_.is_open()) {
        // Write CSV header
        metrics_file_ << "timestep,episode,avg_reward,avg_episode_length,policy_loss,value_loss,entropy_loss,kl_divergence,learning_rate\n";
    } else {
        logger_.Warn("Failed to open metrics file for writing");
    }
    
    logger_.Info("Logging initialization complete");
}

Trainer::Experience Trainer::collectExperience() {
    NVTX_RANGE("Collect_Experience");
    
    Experience experience;
    int steps_per_env = config_.steps_per_update;
    
    // Reserve space for experience data
    int total_steps = config_.num_envs * steps_per_env;
    experience.observations.reserve(total_steps * observation_dim_);
    experience.actions.reserve(total_steps);
    experience.rewards.reserve(total_steps);
    experience.dones.reserve(total_steps);
    experience.values.reserve(total_steps);
    experience.log_probs.reserve(total_steps);
    
    // Collect experience from all environments
    for (int step = 0; step < steps_per_env; ++step) {
        for (int env_id = 0; env_id < config_.num_envs; ++env_id) {
            NVTX_RANGE("Environment_Step");
            
            // Get observation
            auto observation = getObservation(env_id);
            
            // Get action from policy
            auto [action, value, log_prob] = getAction(observation);
            
            // Take environment step
            auto [reward, done] = stepEnvironment(env_id, action);
            
            // Store experience
            experience.observations.insert(experience.observations.end(), 
                                         observation.begin(), observation.end());
            experience.actions.push_back(action);
            experience.rewards.push_back(reward);
            experience.dones.push_back(done);
            experience.values.push_back(value);
            experience.log_probs.push_back(log_prob);
            
            // Update environment state
            env_states_[env_id].episode_length++;
            env_states_[env_id].total_reward += reward;
            
            if (done) {
                env_states_[env_id].is_done = true;
                
                // Log episode completion
                logger_.Debug("Environment {} completed episode: length={}, reward={:.2f}", 
                            env_id, env_states_[env_id].episode_length, env_states_[env_id].total_reward);
                
                // Reset environment
                resetEnvironment(env_id);
            }
        }
    }
    
    return experience;
}

std::vector<float> Trainer::getObservation(int env_id) {
    NVTX_RANGE("Get_Observation");
    
    std::vector<float> observation(observation_dim_);
    
    const auto& state = env_states_[env_id];
    
    // Agent position (3 values)
    observation[0] = state.agent_position.x;
    observation[1] = state.agent_position.y;
    observation[2] = state.agent_position.z;
    
    // Voxel grid around agent (64x64x64 = 262144 values)
    int obs_radius = 32;
    int idx = 3;
    
    for (int dz = -obs_radius; dz < obs_radius; ++dz) {
        for (int dy = -obs_radius; dy < obs_radius; ++dy) {
            for (int dx = -obs_radius; dx < obs_radius; ++dx) {
                glm::vec3 world_pos = state.agent_position + glm::vec3(dx, dy, dz);
                
                // Get block type at position (simplified)
                uint16_t block_type = world_manager_->GetBlock(
                    static_cast<int>(world_pos.x),
                    static_cast<int>(world_pos.y),
                    static_cast<int>(world_pos.z)
                );
                
                // Normalize block type to [0, 1]
                observation[idx++] = static_cast<float>(block_type) / 255.0f;
                
                if (idx >= observation_dim_) break;
            }
            if (idx >= observation_dim_) break;
        }
        if (idx >= observation_dim_) break;
    }
    
    return observation;
}

std::tuple<int, float, float> Trainer::getAction(const std::vector<float>& observation) {
    NVTX_RANGE("Get_Action");
    
    // Simplified action selection - in practice would use neural network
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Action space: 6 movement directions + 2 block actions
    std::uniform_int_distribution<> action_dist(0, action_dim_ - 1);
    int action = action_dist(gen);
    
    // Simplified value estimation
    float value = 0.5f;
    
    // Simplified log probability
    float log_prob = -std::log(static_cast<float>(action_dim_));
    
    return {action, value, log_prob};
}

std::tuple<float, bool> Trainer::stepEnvironment(int env_id, int action) {
    NVTX_RANGE("Step_Environment");
    
    auto& state = env_states_[env_id];
    
    // Execute action
    glm::vec3 movement = {0.0f, 0.0f, 0.0f};
    bool place_block = false;
    bool break_block = false;
    
    switch (action) {
        case 0: movement.x = 1.0f; break;  // Move +X
        case 1: movement.x = -1.0f; break; // Move -X
        case 2: movement.y = 1.0f; break;  // Move +Y
        case 3: movement.y = -1.0f; break; // Move -Y
        case 4: movement.z = 1.0f; break;  // Move +Z
        case 5: movement.z = -1.0f; break; // Move -Z
        case 6: place_block = true; break; // Place block
        case 7: break_block = true; break; // Break block
    }
    
    // Apply movement
    glm::vec3 new_pos = state.agent_position + movement;
    
    // Check collision (simplified)
    uint16_t block_at_pos = world_manager_->GetBlock(
        static_cast<int>(new_pos.x),
        static_cast<int>(new_pos.y),
        static_cast<int>(new_pos.z)
    );
    
    if (block_at_pos == 0) { // Air block
        state.agent_position = new_pos;
    }
    
    // Handle block placement/breaking
    if (place_block) {
        glm::vec3 place_pos = state.agent_position + glm::vec3(0, -1, 0);
        world_manager_->SetBlock(
            static_cast<int>(place_pos.x),
            static_cast<int>(place_pos.y),
            static_cast<int>(place_pos.z),
            1 // Stone block
        );
    } else if (break_block) {
        glm::vec3 break_pos = state.agent_position + glm::vec3(0, 0, 1);
        world_manager_->SetBlock(
            static_cast<int>(break_pos.x),
            static_cast<int>(break_pos.y),
            static_cast<int>(break_pos.z),
            0 // Air block
        );
    }
    
    // Calculate reward
    float reward = calculateReward(env_id, action);
    
    // Check if episode is done
    bool done = (state.episode_length >= config_.max_episode_length) || 
                (state.agent_position.y < 0); // Fell into void
    
    return {reward, done};
}

float Trainer::calculateReward(int env_id, int action) {
    const auto& state = env_states_[env_id];
    
    float reward = 0.0f;
    
    // Survival reward
    reward += 0.001f;
    
    // Movement reward (encourage exploration)
    if (action >= 0 && action <= 5) {
        reward += 0.01f;
    }
    
    // Block interaction reward
    if (action == 6 || action == 7) {
        reward += 0.1f;
    }
    
    // Height reward (encourage staying above ground)
    if (state.agent_position.y > 60) {
        reward += 0.01f;
    } else if (state.agent_position.y < 10) {
        reward -= 0.1f; // Penalty for going too low
    }
    
    // Boundary penalty
    float max_coord = 1000.0f;
    if (std::abs(state.agent_position.x) > max_coord || 
        std::abs(state.agent_position.z) > max_coord) {
        reward -= 0.5f;
    }
    
    return reward;
}

void Trainer::resetEnvironment(int env_id) {
    auto& state = env_states_[env_id];
    
    // Reset state
    state.episode_length = 0;
    state.total_reward = 0.0f;
    state.is_done = false;
    
    // Reset agent position with some randomization
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> pos_dist(-100.0f, 100.0f);
    
    state.agent_position = {
        pos_dist(gen),
        64.0f + pos_dist(gen) * 0.1f, // Small variation in Y
        pos_dist(gen)
    };
}

void Trainer::logTrainingStats(int timestep, int episode, 
                             const Experience& experience, 
                             const PPOTrainer::TrainingStats& training_stats) {
    
    // Calculate average reward and episode length
    float avg_reward = 0.0f;
    float avg_episode_length = 0.0f;
    int completed_episodes = 0;
    
    for (const auto& state : env_states_) {
        if (state.is_done) {
            avg_reward += state.total_reward;
            avg_episode_length += state.episode_length;
            completed_episodes++;
        }
    }
    
    if (completed_episodes > 0) {
        avg_reward /= completed_episodes;
        avg_episode_length /= completed_episodes;
    }
    
    // Log to console
    logger_.Info("Timestep: {}, Episode: {}, Avg Reward: {:.2f}, Avg Length: {:.1f}, "
               "Policy Loss: {:.4f}, Value Loss: {:.4f}, KL: {:.6f}",
               timestep, episode, avg_reward, avg_episode_length,
               training_stats.policy_loss, training_stats.value_loss, training_stats.kl_divergence);
    
    // Write to metrics file
    if (metrics_file_.is_open()) {
        metrics_file_ << timestep << "," << episode << "," << avg_reward << "," 
                     << avg_episode_length << "," << training_stats.policy_loss << ","
                     << training_stats.value_loss << "," << training_stats.entropy_loss << ","
                     << training_stats.kl_divergence << "," << training_stats.learning_rate << "\n";
        metrics_file_.flush();
    }
}

void Trainer::saveCheckpoint(int timestep) {
    std::string checkpoint_path = config_.output_dir + "/checkpoints/checkpoint_" + 
                                 std::to_string(timestep) + ".bin";
    
    if (ppo_trainer_->saveCheckpoint(checkpoint_path)) {
        logger_.Info("Checkpoint saved at timestep {} to: {}", timestep, checkpoint_path);
    } else {
        logger_.Error("Failed to save checkpoint at timestep {}", timestep);
    }
}

void Trainer::evaluatePolicy(int timestep) {
    logger_.Info("Evaluating policy at timestep {}", timestep);
    
    // TODO: Implement proper policy evaluation
    // For now, just log current performance metrics
    
    float total_reward = 0.0f;
    int total_episodes = 0;
    
    for (const auto& state : env_states_) {
        if (state.is_done) {
            total_reward += state.total_reward;
            total_episodes++;
        }
    }
    
    if (total_episodes > 0) {
        float avg_eval_reward = total_reward / total_episodes;
        logger_.Info("Evaluation at timestep {}: Average reward = {:.2f}", timestep, avg_eval_reward);
    }
}

int Trainer::calculateObservationDim() {
    // Agent position (3) + voxel grid (64^3) = 262147
    return 3 + (64 * 64 * 64);
}

int Trainer::calculateActionDim() {
    // 6 movement directions + 2 block actions = 8
    return 8;
}

void Trainer::cleanup() {
    if (metrics_file_.is_open()) {
        metrics_file_.close();
    }
    
    ppo_trainer_.reset();
    world_manager_.reset();
    
    logger_.Info("Trainer cleanup complete");
}

} // namespace voxelvk