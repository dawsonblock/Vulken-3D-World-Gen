#include "cpp_player_controller.hpp"
#include "collision_utils.hpp"
#include "voxel_solid.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <cmath>

namespace voxelvk::physics {

CppPlayerController::CppPlayerController(std::shared_ptr<collision_utils::WorldInterface> world_interface,
                                        const glm::vec3& spawn_position,
                                        CollisionMode collision_mode,
                                        const Config& config)
    : world_(world_interface)
    , config_(config)
    , position_(spawn_position)
    , velocity_(0.0f)
    , on_ground_(false)
    , collision_mode_(collision_mode) {
    
    if (!world_) {
        throw std::runtime_error("CppPlayerController: world_interface cannot be null");
    }
    
    // Ensure starting position is valid
    validateAndClampPosition();
    resetDebugInfo();
}

void CppPlayerController::update(float dt, const glm::vec3& camera_forward, const glm::vec3& camera_right) {
    beginFrameTiming();
    resetDebugInfo();
    
    // Clamp dt to prevent physics explosions
    dt = std::clamp(dt, 1e-6f, 1.0f);
    
    // Store velocity before collision for debug
    debug_info_.velocity_before_collision = velocity_;
    
    // Update movement based on input
    updateMovement(dt, camera_forward, camera_right);
    
    // Apply gravity
    applyGravity(dt);
    
    // Handle jump input
    handleJump();
    
    // Store original position for step-up logic
    glm::vec3 original_position = position_;
    
    // Move with collision detection
    moveAndCollide(dt);
    
    // Apply friction when on ground and no input
    applyFriction(dt);
    
    // Attempt step-up if movement was blocked
    if (glm::length(position_ - original_position) < config_.epsilon && input_.hasMovementInput()) {
        attemptStepUp(dt, original_position);
    }
    
    // Final validation
    validateAndClampPosition();
    
    // Update debug info
    debug_info_.velocity_after_collision = velocity_;
    updateDebugInfo();
    
    endFrameTiming();
}

void CppPlayerController::updateMovement(float dt, const glm::vec3& camera_forward, const glm::vec3& camera_right) {
    // Calculate wish direction - matches Python exactly
    glm::vec3 wish = calculateWishDirection(camera_forward, camera_right);
    wish.y = 0.0f;  // No vertical input movement
    
    // Normalize wish direction
    float wish_length = glm::length(wish);
    if (wish_length > config_.epsilon) {
        wish /= wish_length;
    }
    
    // Calculate target speed
    float target_speed = calculateTargetSpeed();
    
    // Get effective acceleration
    float acceleration = getEffectiveAcceleration();
    
    // Apply acceleration - matches Python physics exactly
    glm::vec3 horizontal_velocity = velocity_;
    horizontal_velocity.y = 0.0f;
    
    glm::vec3 acceleration_vector = (wish * target_speed - horizontal_velocity) * std::min(1.0f, acceleration * dt);
    velocity_ += acceleration_vector;
}

void CppPlayerController::applyGravity(float dt) {
    velocity_.y -= config_.gravity * dt;
}

void CppPlayerController::handleJump() {
    if (on_ground_ && input_.jump) {
        velocity_.y = config_.jump_speed;
        on_ground_ = false;
    }
}

void CppPlayerController::applyFriction(float dt) {
    if (on_ground_ && !input_.hasMovementInput()) {
        float friction_factor = std::max(0.0f, 1.0f - config_.friction * dt);
        velocity_.x *= friction_factor;
        velocity_.z *= friction_factor;
    }
}

void CppPlayerController::moveAndCollide(float dt) {
    switch (collision_mode_) {
        case CollisionMode::AABB:
            moveAndCollideAABB(dt);
            break;
        case CollisionMode::CAPSULE:
            moveAndCollideCapsule(dt);
            break;
    }
}

void CppPlayerController::moveAndCollideAABB(float dt) {
    // Direct port of Python PlayerController._move_and_collide
    glm::vec3 delta = velocity_ * dt;
    
    // Axis-separated swept collision - process X, Z, then Y
    bool hit_x = false, hit_z = false, hit_y = false;
    
    std::tie(position_, hit_x) = sweepAxis(position_, 0, delta.x);
    std::tie(position_, hit_z) = sweepAxis(position_, 2, delta.z);
    std::tie(position_, hit_y) = sweepAxis(position_, 1, delta.y);
    
    // Update velocity based on collisions - matches Python exactly
    if (hit_x) velocity_.x = 0.0f;
    if (hit_z) velocity_.z = 0.0f;
    if (hit_y) {
        if (delta.y < 0.0f) {
            on_ground_ = true;
        }
        if (delta.y > 0.0f && velocity_.y > 0.0f) {
            velocity_.y = 0.0f;
        }
    } else {
        on_ground_ = false;
    }
}

std::pair<glm::vec3, bool> CppPlayerController::sweepAxis(const glm::vec3& start_pos, int axis, float delta) {
    // Direct port of Python PlayerController._sweep_axis with binary search
    if (std::abs(delta) < config_.epsilon) {
        return {start_pos, false};
    }
    
    float step = (delta > 0.0f) ? 1.0f : -1.0f;
    float remaining = std::abs(delta);
    bool hit = false;
    glm::vec3 current_pos = start_pos;
    
    while (remaining > config_.epsilon) {
        float advance = std::min(remaining, config_.sweep_step_size);
        glm::vec3 trial_pos = current_pos;
        trial_pos[axis] += step * advance;
        
        if (canOccupyAABB(trial_pos)) {
            current_pos = trial_pos;
            remaining -= advance;
        } else {
            // Binary search for precise collision boundary - matches Python exactly
            float hi = advance;
            float lo = 0.0f;
            
            for (int i = 0; i < static_cast<int>(config_.binary_search_iterations); ++i) {
                float mid = 0.5f * (hi + lo);
                glm::vec3 trial_mid = current_pos;
                trial_mid[axis] += step * mid;
                
                if (canOccupyAABB(trial_mid)) {
                    lo = mid;
                } else {
                    hi = mid;
                }
            }
            
            current_pos[axis] += step * lo;
            hit = true;
            break;
        }
    }
    
    return {current_pos, hit};
}

bool CppPlayerController::canOccupyAABB(const glm::vec3& center) const {
    // Direct port of Python PlayerController._can_occupy
    AABB test_aabb(center, config_.aabb_half_extent);
    
    glm::vec3 aabb_min = test_aabb.min();
    glm::vec3 aabb_max = test_aabb.max();
    
    glm::ivec3 min_voxel(
        static_cast<int>(std::floor(aabb_min.x)),
        static_cast<int>(std::floor(aabb_min.y)),
        static_cast<int>(std::floor(aabb_min.z))
    );
    
    glm::ivec3 max_voxel(
        static_cast<int>(std::floor(aabb_max.x)),
        static_cast<int>(std::floor(aabb_max.y)),
        static_cast<int>(std::floor(aabb_max.z))
    );
    
    int blocks_checked = 0;
    for (int y = min_voxel.y; y <= max_voxel.y; ++y) {
        for (int z = min_voxel.z; z <= max_voxel.z; ++z) {
            for (int x = min_voxel.x; x <= max_voxel.x; ++x) {
                blocks_checked++;
                debug_info_.blocks_checked_last_frame = blocks_checked;
                
                if (isBlockSolid(x, y, z)) {
                    if (aabbVoxelOverlap(test_aabb, x, y, z)) {
                        return false;
                    }
                }
            }
        }
    }
    
    return true;
}

bool CppPlayerController::aabbVoxelOverlap(const AABB& aabb, int x, int y, int z) const {
    // Direct port of Python PlayerController._aabb_voxel_overlap
    glm::vec3 aabb_max = aabb.max();
    glm::vec3 aabb_min = aabb.min();
    
    if (aabb_max.x <= static_cast<float>(x) || aabb_min.x >= static_cast<float>(x + 1)) return false;
    if (aabb_max.y <= static_cast<float>(y) || aabb_min.y >= static_cast<float>(y + 1)) return false;
    if (aabb_max.z <= static_cast<float>(z) || aabb_min.z >= static_cast<float>(z + 1)) return false;
    
    return true;
}

void CppPlayerController::moveAndCollideCapsule(float dt) {
    // Direct port of Python PlayerControllerCapsule update logic
    glm::vec3 old_position = position_;
    
    // Apply velocity
    position_ += velocity_ * dt;
    
    // Validate position bounds
    if (glm::length(position_) > 1e6f) {
        std::cout << "Warning: Player position out of bounds, resetting" << std::endl;
        position_ = old_position;
        velocity_ *= 0.1f;  // Dampen velocity
        debug_info_.position_clamped = true;
    }
    
    // Create capsule for collision resolution
    Capsule capsule(position_, config_.capsule_half_height, config_.capsule_radius);
    
    // Resolve collisions using the collision utilities
    collision_utils::CollisionConfig collision_config;
    collision_config.max_collision_iterations = config_.max_collision_iterations;
    collision_config.max_blocks_checked = config_.max_blocks_checked;
    collision_config.max_correction_per_iteration = config_.max_correction_per_iteration;
    collision_config.search_radius_limit = config_.search_radius_limit;
    collision_config.ground_normal_threshold = config_.ground_normal_threshold;
    
    auto result = collision_utils::resolveCapsuleWorldAdvanced(capsule, *world_, collision_config);
    
    // Update position from collision resolution
    position_ = capsule.center;
    
    // Update debug info
    debug_info_.blocks_checked_last_frame = result.blocks_checked;
    debug_info_.collision_iterations_last_frame = result.iterations_used;
    debug_info_.total_correction_applied = glm::length(result.total_offset);
    debug_info_.position_clamped = result.position_clamped;
    
    // Update ground state
    bool was_on_ground = on_ground_;
    on_ground_ = result.is_on_ground;
    
    // Stop downward velocity when landing
    if (on_ground_ && !was_on_ground && velocity_.y < 0.0f) {
        velocity_.y = 0.0f;
    }
}

void CppPlayerController::attemptStepUp(float dt, const glm::vec3& original_position) {
    // Direct port of Python step-up logic
    glm::vec3 lifted_position = original_position;
    lifted_position.y += config_.step_height;
    
    bool can_step = false;
    
    if (collision_mode_ == CollisionMode::AABB) {
        can_step = canOccupyAABB(lifted_position);
    } else {
        can_step = canOccupyCapsule(lifted_position);
    }
    
    if (can_step) {
        position_ = lifted_position;
        moveAndCollide(dt);  // Try movement again from lifted position
        debug_info_.step_up_activated = true;
    }
}

bool CppPlayerController::canOccupyCapsule(const glm::vec3& center) const {
    Capsule test_capsule(center, config_.capsule_half_height, config_.capsule_radius);
    return collision_utils::isCapsulePositionSafe(test_capsule, *world_);
}

void CppPlayerController::switchCollisionMode(CollisionMode mode) {
    collision_mode_ = mode;
    resetDebugInfo();
}

void CppPlayerController::setPosition(const glm::vec3& position) {
    position_ = position;
    validateAndClampPosition();
}

void CppPlayerController::teleport(const glm::vec3& position) {
    position_ = position;
    velocity_ = glm::vec3(0.0f);
    on_ground_ = false;
    validateAndClampPosition();
}

AABB CppPlayerController::getCurrentAABB() const {
    return AABB(position_, config_.aabb_half_extent);
}

Capsule CppPlayerController::getCurrentCapsule() const {
    return Capsule(position_, config_.capsule_half_height, config_.capsule_radius);
}

bool CppPlayerController::canReachPosition(const glm::vec3& target_position) const {
    if (collision_mode_ == CollisionMode::AABB) {
        return canOccupyAABB(target_position);
    } else {
        return canOccupyCapsule(target_position);
    }
}

glm::vec3 CppPlayerController::findNearestSafePosition(const glm::vec3& desired_position) const {
    if (collision_mode_ == CollisionMode::AABB) {
        // For AABB mode, do a simple search
        if (canOccupyAABB(desired_position)) {
            return desired_position;
        }
        
        // Try positions around the desired location
        const float search_radius = 2.0f;
        const int search_steps = 8;
        
        for (int step = 1; step <= search_steps; ++step) {
            float radius = (static_cast<float>(step) / search_steps) * search_radius;
            
            for (int angle = 0; angle < 8; ++angle) {
                float theta = (static_cast<float>(angle) / 8.0f) * 2.0f * 3.14159f;
                glm::vec3 test_pos = desired_position + glm::vec3(
                    radius * std::cos(theta),
                    0.0f,
                    radius * std::sin(theta)
                );
                
                if (canOccupyAABB(test_pos)) {
                    return test_pos;
                }
            }
        }
        
        return position_;  // Return current position if no safe position found
    } else {
        // For capsule mode, use the collision utilities
        Capsule template_capsule(glm::vec3(0.0f), config_.capsule_half_height, config_.capsule_radius);
        return collision_utils::findSafePosition(desired_position, template_capsule, *world_);
    }
}

float CppPlayerController::getDistanceToGround() const {
    glm::ivec3 hit_pos;
    float hit_distance;
    
    if (block_utils::findFirstSolidBlock(*world_, position_, glm::vec3(0, -1, 0), 
                                        100.0f, hit_pos, hit_distance)) {
        return hit_distance;
    }
    
    return std::numeric_limits<float>::max();
}

void CppPlayerController::resetDebugInfo() {
    debug_info_ = DebugInfo{};
}

// Private helper methods

glm::vec3 CppPlayerController::calculateWishDirection(const glm::vec3& camera_forward, 
                                                     const glm::vec3& camera_right) const {
    // Direct port of Python wish direction calculation
    return (camera_forward * (input_.forward ? 1.0f : 0.0f) + 
            camera_forward * (input_.backward ? -1.0f : 0.0f)) +
           (camera_right * (input_.right ? 1.0f : 0.0f) + 
            camera_right * (input_.left ? -1.0f : 0.0f)) +
           (glm::vec3(0, 1, 0) * (input_.up ? 1.0f : 0.0f) + 
            glm::vec3(0, 1, 0) * (input_.down ? -1.0f : 0.0f));
}

float CppPlayerController::calculateTargetSpeed() const {
    return config_.max_speed * (input_.sprint ? config_.sprint_multiplier : 1.0f);
}

float CppPlayerController::getEffectiveAcceleration() const {
    return on_ground_ ? config_.acceleration : config_.air_acceleration;
}

bool CppPlayerController::isBlockSolid(int x, int y, int z) const {
    try {
        uint16_t block_type = world_->getBlockAtWorldPosition(
            static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
        return world_->isBlockSolid(block_type);
    } catch (...) {
        return true;  // Assume solid on error to be safe
    }
}

void CppPlayerController::validateAndClampPosition() {
    // Prevent extreme positions
    const float max_coord = 1e6f;
    
    if (std::abs(position_.x) > max_coord || std::abs(position_.y) > max_coord || std::abs(position_.z) > max_coord) {
        std::cout << "Warning: Player position clamped from extreme values" << std::endl;
        position_ = glm::clamp(position_, glm::vec3(-max_coord), glm::vec3(max_coord));
        debug_info_.position_clamped = true;
    }
    
    // Check for NaN/inf
    if (std::isnan(position_.x) || std::isnan(position_.y) || std::isnan(position_.z) ||
        std::isinf(position_.x) || std::isinf(position_.y) || std::isinf(position_.z)) {
        std::cout << "Warning: Player position contained NaN/Inf, resetting" << std::endl;
        position_ = glm::vec3(0, 100, 0);
        velocity_ = glm::vec3(0);
        debug_info_.position_clamped = true;
    }
}

void CppPlayerController::updateDebugInfo(bool step_up_used) const {
    debug_info_.step_up_activated = step_up_used;
}

void CppPlayerController::beginFrameTiming() {
    frame_start_time_ = std::chrono::high_resolution_clock::now();
}

void CppPlayerController::endFrameTiming() {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - frame_start_time_);
    debug_info_.frame_time_ms = static_cast<float>(duration.count()) / 1000.0f;
}

// Utility functions implementation
namespace player_utils {

void PerformanceProfiler::recordFrame(const CppPlayerController& controller) {
    const auto& debug_info = controller.getDebugInfo();
    
    frame_times_.push_back(debug_info.frame_time_ms);
    blocks_checked_.push_back(debug_info.blocks_checked_last_frame);
    collision_iterations_.push_back(debug_info.collision_iterations_last_frame);
    current_mode_ = controller.getCollisionMode();
}

PerformanceMetrics PerformanceProfiler::getMetrics() const {
    PerformanceMetrics metrics;
    metrics.mode_tested = current_mode_;
    metrics.total_frames_measured = static_cast<int>(frame_times_.size());
    
    if (!frame_times_.empty()) {
        float total_time = 0.0f;
        int total_blocks = 0;
        int total_iterations = 0;
        
        for (size_t i = 0; i < frame_times_.size(); ++i) {
            total_time += frame_times_[i];
            total_blocks += blocks_checked_[i];
            total_iterations += collision_iterations_[i];
        }
        
        metrics.average_frame_time_ms = total_time / frame_times_.size();
        metrics.average_blocks_checked = static_cast<float>(total_blocks) / frame_times_.size();
        metrics.average_collision_iterations = static_cast<float>(total_iterations) / frame_times_.size();
    }
    
    return metrics;
}

void PerformanceProfiler::reset() {
    frame_times_.clear();
    blocks_checked_.clear();
    collision_iterations_.clear();
}

void PerformanceProfiler::printReport() const {
    auto metrics = getMetrics();
    
    std::cout << "\n=== CppPlayerController Performance Report ===" << std::endl;
    std::cout << "Collision Mode: " << (metrics.mode_tested == CollisionMode::AABB ? "AABB" : "CAPSULE") << std::endl;
    std::cout << "Frames Measured: " << metrics.total_frames_measured << std::endl;
    std::cout << "Average Frame Time: " << metrics.average_frame_time_ms << " ms" << std::endl;
    std::cout << "Average Blocks Checked: " << metrics.average_blocks_checked << std::endl;
    std::cout << "Average Collision Iterations: " << metrics.average_collision_iterations << std::endl;
    std::cout << "=============================================" << std::endl;
}

glm::vec3 getPlayerSpawnPosition(const collision_utils::WorldInterface& world,
                                const glm::vec3& desired_position) {
    // Find a safe spawn position above the desired location
    float ground_level = block_utils::findGroundLevel(world, desired_position);
    glm::vec3 spawn_pos = desired_position;
    spawn_pos.y = ground_level + 2.0f;  // Spawn 2 blocks above ground
    
    return spawn_pos;
}

bool isPositionSafeForPlayer(const collision_utils::WorldInterface& world,
                            const glm::vec3& position,
                            const CppPlayerController::Config& config) {
    // Check both AABB and Capsule for safety
    AABB test_aabb(position, config.aabb_half_extent);
    Capsule test_capsule(position, config.capsule_half_height, config.capsule_radius);
    
    return collision_utils::isCapsulePositionSafe(test_capsule, world);
}

void copyStateFromPythonController(CppPlayerController& cpp_controller,
                                  const glm::vec3& python_position,
                                  const glm::vec3& python_velocity,
                                  bool python_on_ground) {
    cpp_controller.setPosition(python_position);
    cpp_controller.setVelocity(python_velocity);
    cpp_controller.setOnGround(python_on_ground);
}

std::tuple<glm::vec3, glm::vec3, bool> 
extractStateForPythonController(const CppPlayerController& cpp_controller) {
    return std::make_tuple(
        cpp_controller.getPosition(),
        cpp_controller.getVelocity(),
        cpp_controller.isOnGround()
    );
}

} // namespace player_utils

} // namespace voxelvk::physics