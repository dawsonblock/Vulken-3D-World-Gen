#include <iostream>
#include <memory>
#include <vector>
#include <chrono>

// RL Backend Systems
#include "../src/rl/rl_backend.hpp"
#include "../src/rl/rl_backend_minimal.hpp"
#include "../src/core/logger.hpp"

using namespace voxelvk::rl;
using namespace voxelvk;

/**
 * Simple navigation task for RL demo
 * Agent learns to navigate to a target position in a voxel grid
 */
class NavigationEnvironment {
private:
    static constexpr uint32_t GRID_SIZE = 16;
    static constexpr uint32_t ACTION_SIZE = 4; // Up, Down, Left, Right
    static constexpr uint32_t OBS_SIZE = 64;   // Grid state + agent pos + target pos

    float agentX_, agentY_;
    float targetX_, targetY_;
    uint32_t stepCount_;
    uint32_t maxSteps_;
    Logger logger_;

public:
    NavigationEnvironment() : agentX_(1.0f), agentY_(1.0f), targetX_(14.0f), targetY_(14.0f),
                             stepCount_(0), maxSteps_(200), logger_("NavEnv") {}

    std::vector<float> reset() {
        // Reset agent to random starting position
        agentX_ = 1.0f + static_cast<float>(rand() % static_cast<int>(GRID_SIZE - 4));
        agentY_ = 1.0f + static_cast<float>(rand() % static_cast<int>(GRID_SIZE - 4));

        // Reset target to random position (avoid agent)
        do {
            targetX_ = 1.0f + static_cast<float>(rand() % static_cast<int>(GRID_SIZE - 4));
            targetY_ = 1.0f + static_cast<float>(rand() % static_cast<int>(GRID_SIZE - 4));
        } while (std::abs(targetX_ - agentX_) < 2.0f && std::abs(targetY_ - agentY_) < 2.0f);

        stepCount_ = 0;

        return getObservation();
    }

    struct StepResult {
        std::vector<float> observation;
        float reward;
        bool done;
    };

    StepResult step(const std::vector<float>& action) {
        if (action.size() < ACTION_SIZE) {
            return {getObservation(), -1.0f, true}; // Invalid action
        }

        // Convert action probabilities to discrete action
        uint32_t actionIndex = 0;
        float maxProb = action[0];
        for (uint32_t i = 1; i < ACTION_SIZE; i++) {
            if (action[i] > maxProb) {
                maxProb = action[i];
                actionIndex = i;
            }
        }

        // Execute action
        float oldX = agentX_, oldY = agentY_;

        switch (actionIndex) {
            case 0: agentY_ = std::max(0.0f, agentY_ - 1.0f); break; // Up
            case 1: agentY_ = std::min(static_cast<float>(GRID_SIZE - 1), agentY_ + 1.0f); break; // Down
            case 2: agentX_ = std::max(0.0f, agentX_ - 1.0f); break; // Left
            case 3: agentX_ = std::min(static_cast<float>(GRID_SIZE - 1), agentX_ + 1.0f); break; // Right
        }

        stepCount_++;

        // Calculate reward
        float oldDistance = std::sqrt((oldX - targetX_) * (oldX - targetX_) +
                                     (oldY - targetY_) * (oldY - targetY_));
        float newDistance = std::sqrt((agentX_ - targetX_) * (agentX_ - targetX_) +
                                     (agentY_ - targetY_) * (agentY_ - targetY_));

        float reward = 0.0f;
        bool done = false;

        // Reward for getting closer
        reward += (oldDistance - newDistance) * 0.1f;

        // Large reward for reaching target
        if (newDistance < 1.0f) {
            reward += 10.0f;
            done = true;
            logger_.Debug("Target reached! Steps: {}", stepCount_);
        }

        // Small penalty for each step (encourage efficiency)
        reward -= 0.01f;

        // Episode timeout
        if (stepCount_ >= maxSteps_) {
            done = true;
            reward -= 1.0f; // Penalty for timeout
        }

        return {getObservation(), reward, done};
    }

private:
    std::vector<float> getObservation() {
        std::vector<float> obs(OBS_SIZE, 0.0f);

        // Agent position (normalized)
        obs[0] = agentX_ / GRID_SIZE;
        obs[1] = agentY_ / GRID_SIZE;

        // Target position (normalized)
        obs[2] = targetX_ / GRID_SIZE;
        obs[3] = targetY_ / GRID_SIZE;

        // Distance to target
        float distance = std::sqrt((agentX_ - targetX_) * (agentX_ - targetX_) +
                                  (agentY_ - targetY_) * (agentY_ - targetY_));
        obs[4] = distance / (GRID_SIZE * 1.414f); // Normalized diagonal distance

        // Direction to target
        float dx = targetX_ - agentX_;
        float dy = targetY_ - agentY_;
        float dirLength = std::sqrt(dx * dx + dy * dy);
        if (dirLength > 0.0f) {
            obs[5] = dx / dirLength; // Normalized direction X
            obs[6] = dy / dirLength; // Normalized direction Y
        }

        // Step count (normalized)
    obs[7] = static_cast<float>(stepCount_) / static_cast<float>(maxSteps_);

        // Simple grid representation (8x8 downsampled)
        for (uint32_t y = 0; y < 8; y++) {
            for (uint32_t x = 0; x < 8; x++) {
                uint32_t obsIdx = 8 + y * 8 + x;
                if (obsIdx < OBS_SIZE) {
                    float gridX = static_cast<float>(x) * 2.0f;
                    float gridY = static_cast<float>(y) * 2.0f;

                    // Mark agent position
                    if (std::abs(gridX - agentX_) < 1.5f && std::abs(gridY - agentY_) < 1.5f) {
                        obs[obsIdx] = 1.0f;
                    }
                    // Mark target position
                    else if (std::abs(gridX - targetX_) < 1.5f && std::abs(gridY - targetY_) < 1.5f) {
                        obs[obsIdx] = -1.0f;
                    }
                    else {
                        obs[obsIdx] = 0.0f;
                    }
                }
            }
        }

        return obs;
    }
};

int main() {
    std::cout << "🤖 VoxelVK RL Navigation Demo" << std::endl;
    std::cout << "Simple navigation task with minimal MLP backend" << std::endl;
    std::cout << "=========================================" << std::endl;

    Logger logger("RLNavDemo");

    // Create RL backend
    MLPConfig config;
    config.inputSize = 64;
    config.actionSize = 4;
    config.hiddenSize = 128;
    config.numHiddenLayers = 2;
    config.learningRate = 0.001f;

    auto backend = std::make_unique<MinimalMLP>(config);

    logger.Info("Created MinimalMLP backend: {} parameters", backend->getParameterCount());

    // Create navigation environment
    NavigationEnvironment env;

    // Training loop
    const uint32_t NUM_EPISODES = 100;
    const uint32_t MAX_STEPS_PER_EPISODE = 200;

    std::vector<float> recentRewards;
    recentRewards.reserve(10);

    logger.Info("Starting training for {} episodes...", NUM_EPISODES);

    for (uint32_t episode = 0; episode < NUM_EPISODES; episode++) {
        auto obs = env.reset();
        float episodeReward = 0.0f;

        // Collect experience for this episode
        std::vector<float> episodeObs, episodeActions, episodeRewards, episodeValues, episodeAdvantages;

        for (uint32_t step = 0; step < MAX_STEPS_PER_EPISODE; step++) {
            // Get action from policy
            std::vector<float> actions, values;
            backend->forward(obs, actions, values);

            // Take environment step
            auto result = env.step(actions);

            // Store experience
            episodeObs.insert(episodeObs.end(), obs.begin(), obs.end());
            episodeActions.insert(episodeActions.end(), actions.begin(), actions.end());
            episodeRewards.push_back(result.reward);
            episodeValues.insert(episodeValues.end(), values.begin(), values.end());

            episodeReward += result.reward;
            obs = result.observation;

            if (result.done) {
                break;
            }
        }

        // Simple advantage estimation (placeholder for GAE)
        episodeAdvantages.resize(episodeRewards.size());
        float runningAdvantage = 0.0f;
        for (int i = static_cast<int>(episodeRewards.size()) - 1; i >= 0; i--) {
            runningAdvantage = episodeRewards[static_cast<size_t>(i)] + 0.99f * runningAdvantage;
            episodeAdvantages[static_cast<size_t>(i)] = runningAdvantage;
        }

        // Update policy
        if (backend->hasTrainingCapability()) {
            backend->update(episodeObs, episodeActions, episodeRewards, episodeValues, episodeAdvantages);
        }

        // Track performance
        recentRewards.push_back(episodeReward);
        if (recentRewards.size() > 10) {
            recentRewards.erase(recentRewards.begin());
        }

        // Log progress
        if (episode % 10 == 0 || episode == NUM_EPISODES - 1) {
            float avgReward = 0.0f;
            for (float r : recentRewards) avgReward += r;
            avgReward /= static_cast<float>(recentRewards.size());

            auto stats = backend->getTrainingStats();

            logger.Info("Episode {}: reward={:.2f}, avg_reward={:.2f}, policy_loss={:.6f}",
                episode, episodeReward, avgReward, stats.lastPolicyLoss);
        }

        // Early success detection
        if (recentRewards.size() >= 5) {
            float avgRecent = 0.0f;
            for (float r : recentRewards) avgRecent += r;
            avgRecent /= static_cast<float>(recentRewards.size());

            if (avgRecent > 8.0f) { // High performance threshold
                logger.Info("🎉 Training converged! Average reward: {:.2f}", avgRecent);
                break;
            }
        }
    }

    // Final statistics
    auto finalStats = backend->getTrainingStats();

    std::cout << "\n=== Training Results ===" << std::endl;
    std::cout << "Backend: " << backend->getBackendName() << std::endl;
    std::cout << "Parameters: " << backend->getParameterCount() << std::endl;
    std::cout << "Updates: " << finalStats.updateCount << std::endl;
    std::cout << "Final average reward: " << finalStats.averageReward << std::endl;
    std::cout << "Total training time: " << finalStats.totalTrainingTime << "s" << std::endl;

    // Test trained policy
    std::cout << "\n=== Testing Trained Policy ===" << std::endl;

    for (uint32_t test = 0; test < 3; test++) {
        auto obs = env.reset();
        std::cout << "Test episode " << test + 1 << ":" << std::endl;

        for (uint32_t step = 0; step < 50; step++) {
            std::vector<float> actions, values;
            backend->forward(obs, actions, values);

            auto result = env.step(actions);

            if (step % 10 == 0) {
                std::cout << "  Step " << step << ": action=[" << actions[0] << "," << actions[1]
                         << "," << actions[2] << "," << actions[3] << "], reward=" << result.reward << std::endl;
            }

            obs = result.observation;

            if (result.done) {
                std::cout << "  Episode completed in " << step + 1 << " steps" << std::endl;
                break;
            }
        }
    }

    // Save trained model
    std::string modelPath = "navigation_policy.vxml";
    if (backend->saveModel(modelPath)) {
        std::cout << "\n✅ Model saved to: " << modelPath << std::endl;
    }

    std::cout << "\n🎉 RL Navigation Demo Complete!" << std::endl;
    std::cout << "The minimal MLP backend successfully learned basic navigation." << std::endl;

    return 0;
}
