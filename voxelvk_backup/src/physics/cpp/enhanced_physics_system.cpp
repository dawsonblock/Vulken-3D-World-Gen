#include "enhanced_physics_system.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <unordered_set>

namespace voxelvk::physics {

// SpatialHashGrid implementation
SpatialHashGrid::SpatialHashGrid(float cell_size) : cell_size_(cell_size) {}

void SpatialHashGrid::clear() {
    grid_cells_.clear();
    entity_positions_.clear();
}

void SpatialHashGrid::insert(uint32_t entity_id, const AABB& bounds) {
    auto cells = getCells(bounds);
    entity_positions_[entity_id] = cells;
    
    for (uint64_t cell_hash : cells) {
        grid_cells_[cell_hash].push_back(entity_id);
    }
}

void SpatialHashGrid::remove(uint32_t entity_id) {
    auto it = entity_positions_.find(entity_id);
    if (it == entity_positions_.end()) return;
    
    for (uint64_t cell_hash : it->second) {
        auto& cell = grid_cells_[cell_hash];
        cell.erase(std::remove(cell.begin(), cell.end(), entity_id), cell.end());
        if (cell.empty()) {
            grid_cells_.erase(cell_hash);
        }
    }
    
    entity_positions_.erase(it);
}

void SpatialHashGrid::update(uint32_t entity_id, const AABB& old_bounds, const AABB& new_bounds) {
    remove(entity_id);
    insert(entity_id, new_bounds);
}

std::vector<uint32_t> SpatialHashGrid::query(const AABB& bounds) const {
    std::vector<uint32_t> results;
    std::unordered_set<uint32_t> unique_results;
    
    auto cells = getCells(bounds);
    for (uint64_t cell_hash : cells) {
        auto it = grid_cells_.find(cell_hash);
        if (it != grid_cells_.end()) {
            for (uint32_t entity_id : it->second) {
                if (unique_results.insert(entity_id).second) {
                    results.push_back(entity_id);
                }
            }
        }
    }
    
    return results;
}

std::vector<uint32_t> SpatialHashGrid::queryRadius(const glm::vec3& center, float radius) const {
    AABB query_bounds(center, glm::vec3(radius));
    return query(query_bounds);
}

uint64_t SpatialHashGrid::hashPosition(int x, int y, int z) const {
    // Simple hash function - could be improved for better distribution
    return static_cast<uint64_t>(x) | 
           (static_cast<uint64_t>(y) << 20) | 
           (static_cast<uint64_t>(z) << 40);
}

std::vector<uint64_t> SpatialHashGrid::getCells(const AABB& bounds) const {
    std::vector<uint64_t> cells;
    
    glm::vec3 min_pos = bounds.min();
    glm::vec3 max_pos = bounds.max();
    
    int min_x = static_cast<int>(std::floor(min_pos.x / cell_size_));
    int max_x = static_cast<int>(std::floor(max_pos.x / cell_size_));
    int min_y = static_cast<int>(std::floor(min_pos.y / cell_size_));
    int max_y = static_cast<int>(std::floor(max_pos.y / cell_size_));
    int min_z = static_cast<int>(std::floor(min_pos.z / cell_size_));
    int max_z = static_cast<int>(std::floor(max_pos.z / cell_size_));
    
    for (int x = min_x; x <= max_x; ++x) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int z = min_z; z <= max_z; ++z) {
                cells.push_back(hashPosition(x, y, z));
            }
        }
    }
    
    return cells;
}

// PhysicsBody implementation
PhysicsBody::PhysicsBody(uint32_t id, const glm::vec3& pos, ShapeType type) 
    : entity_id(id), position(pos), shape_type(type) {
    
    if (type == ShapeType::AABB) {
        new(&shape.aabb) AABB(pos, glm::vec3(0.3f, 0.9f, 0.3f));
    } else {
        new(&shape.capsule) Capsule(pos, 0.9f, 0.3f);
    }
    
    setMass(1.0f);
}

void PhysicsBody::setMass(float new_mass) {
    mass = std::max(new_mass, 0.001f);  // Prevent zero mass
    inverse_mass = is_static ? 0.0f : 1.0f / mass;
}

void PhysicsBody::addForce(const glm::vec3& force) {
    if (!is_static) {
        acceleration += force * inverse_mass;
    }
}

void PhysicsBody::addImpulse(const glm::vec3& impulse) {
    if (!is_static) {
        velocity += impulse * inverse_mass;
        wakeUp();
    }
}

AABB PhysicsBody::getBounds() const {
    if (shape_type == ShapeType::AABB) {
        return shape.aabb.moved(position - shape.aabb.center);
    } else {
        return shape.capsule.getBoundingBox().moved(position - shape.capsule.center);
    }
}

glm::vec3 PhysicsBody::getCenter() const {
    return position;
}

float PhysicsBody::getVolume() const {
    if (shape_type == ShapeType::AABB) {
        return shape.aabb.volume();
    } else {
        return shape.capsule.volume();
    }
}

void PhysicsBody::updateSleepState(float dt) {
    if (is_static || is_kinematic) {
        is_sleeping = false;
        return;
    }
    
    float speed_squared = glm::dot(velocity, velocity);
    
    if (speed_squared < sleep_threshold * sleep_threshold) {
        sleep_timer += dt;
        if (sleep_timer > 1.0f) {  // Sleep after 1 second of inactivity
            is_sleeping = true;
            velocity = glm::vec3(0.0f);
        }
    } else {
        sleep_timer = 0.0f;
        is_sleeping = false;
    }
}

bool PhysicsBody::canCollideWith(const PhysicsBody& other) const {
    return (collision_layer & other.collision_mask) != 0 && 
           (collision_mask & other.collision_layer) != 0;
}

// PhysicsProfiler implementation
void PhysicsProfiler::beginFrame() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    frame_start_ = std::chrono::high_resolution_clock::now();
    current_frame_ = FrameData{};
}

void PhysicsProfiler::endFrame() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto end_time = std::chrono::high_resolution_clock::now();
    current_frame_.total_time_ms = std::chrono::duration<float, std::milli>(end_time - frame_start_).count();
    
    frame_history_.push_back(current_frame_);
    if (frame_history_.size() > 120) {  // Keep last 2 seconds at 60fps
        frame_history_.erase(frame_history_.begin());
    }
}

void PhysicsProfiler::beginPhase(const std::string& phase) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    phase_starts_[phase] = std::chrono::high_resolution_clock::now();
}

void PhysicsProfiler::endPhase(const std::string& phase) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = phase_starts_.find(phase);
    if (it == phase_starts_.end()) return;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    float duration_ms = std::chrono::duration<float, std::milli>(end_time - it->second).count();
    
    if (phase == "collision_detection") {
        current_frame_.collision_detection_ms += duration_ms;
    } else if (phase == "collision_resolution") {
        current_frame_.collision_resolution_ms += duration_ms;
    } else if (phase == "integration") {
        current_frame_.integration_ms += duration_ms;
    }
    
    phase_starts_.erase(it);
}

PhysicsProfiler::FrameData PhysicsProfiler::getAverageFrame(int num_frames) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (frame_history_.empty()) return FrameData{};
    
    int frames_to_average = std::min(num_frames, static_cast<int>(frame_history_.size()));
    auto start_it = frame_history_.end() - frames_to_average;
    
    FrameData avg_data{};
    
    for (auto it = start_it; it != frame_history_.end(); ++it) {
        avg_data.total_time_ms += it->total_time_ms;
        avg_data.collision_detection_ms += it->collision_detection_ms;
        avg_data.collision_resolution_ms += it->collision_resolution_ms;
        avg_data.integration_ms += it->integration_ms;
        avg_data.active_bodies += it->active_bodies;
        avg_data.sleeping_bodies += it->sleeping_bodies;
        avg_data.collision_pairs += it->collision_pairs;
        avg_data.collision_events += it->collision_events;
        avg_data.spatial_queries += it->spatial_queries;
        avg_data.broad_phase_pairs += it->broad_phase_pairs;
        avg_data.narrow_phase_tests += it->narrow_phase_tests;
    }
    
    float inv_frames = 1.0f / frames_to_average;
    avg_data.total_time_ms *= inv_frames;
    avg_data.collision_detection_ms *= inv_frames;
    avg_data.collision_resolution_ms *= inv_frames;
    avg_data.integration_ms *= inv_frames;
    avg_data.active_bodies = static_cast<int>(avg_data.active_bodies * inv_frames);
    avg_data.sleeping_bodies = static_cast<int>(avg_data.sleeping_bodies * inv_frames);
    avg_data.collision_pairs = static_cast<int>(avg_data.collision_pairs * inv_frames);
    avg_data.collision_events = static_cast<int>(avg_data.collision_events * inv_frames);
    avg_data.spatial_queries = static_cast<int>(avg_data.spatial_queries * inv_frames);
    avg_data.broad_phase_pairs = static_cast<int>(avg_data.broad_phase_pairs * inv_frames);
    avg_data.narrow_phase_tests = static_cast<int>(avg_data.narrow_phase_tests * inv_frames);
    
    return avg_data;
}

void PhysicsProfiler::printReport() const {
    auto avg_data = getAverageFrame(60);
    
    std::cout << "\n=== Enhanced Physics Performance Report ===" << std::endl;
    std::cout << "Average Frame Time: " << avg_data.total_time_ms << " ms" << std::endl;
    std::cout << "  Collision Detection: " << avg_data.collision_detection_ms << " ms" << std::endl;
    std::cout << "  Collision Resolution: " << avg_data.collision_resolution_ms << " ms" << std::endl;
    std::cout << "  Integration: " << avg_data.integration_ms << " ms" << std::endl;
    std::cout << "Bodies: " << avg_data.active_bodies << " active, " << avg_data.sleeping_bodies << " sleeping" << std::endl;
    std::cout << "Collision Pairs: " << avg_data.collision_pairs << " tested, " << avg_data.collision_events << " events" << std::endl;
    std::cout << "Spatial Queries: " << avg_data.spatial_queries << std::endl;
    std::cout << "==============================================" << std::endl;
}

void PhysicsProfiler::reset() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    frame_history_.clear();
    current_frame_ = FrameData{};
}

// EnhancedPhysicsWorld implementation
EnhancedPhysicsWorld::EnhancedPhysicsWorld(const PhysicsConfig& config) 
    : config_(config) {
    
    if (config_.use_spatial_hashing) {
        spatial_grid_ = std::make_unique<SpatialHashGrid>(4.0f);
    }
    
    last_time_ = std::chrono::high_resolution_clock::now();
    
    initializeThreads();
}

EnhancedPhysicsWorld::~EnhancedPhysicsWorld() {
    shutdownThreads();
}

uint32_t EnhancedPhysicsWorld::addBody(const glm::vec3& position, 
                                      PhysicsBody::ShapeType shape_type,
                                      const PhysicsMaterial& material) {
    std::lock_guard<std::mutex> lock(bodies_mutex_);
    
    uint32_t body_id = next_body_id_++;
    auto body = std::make_unique<PhysicsBody>(body_id, position, shape_type);
    body->material = material;
    
    if (spatial_grid_) {
        spatial_grid_->insert(body_id, body->getBounds());
    }
    
    bodies_[body_id] = std::move(body);
    return body_id;
}

void EnhancedPhysicsWorld::removeBody(uint32_t body_id) {
    std::lock_guard<std::mutex> lock(bodies_mutex_);
    
    if (spatial_grid_) {
        spatial_grid_->remove(body_id);
    }
    
    bodies_.erase(body_id);
}

PhysicsBody* EnhancedPhysicsWorld::getBody(uint32_t body_id) {
    std::lock_guard<std::mutex> lock(bodies_mutex_);
    auto it = bodies_.find(body_id);
    return it != bodies_.end() ? it->second.get() : nullptr;
}

const PhysicsBody* EnhancedPhysicsWorld::getBody(uint32_t body_id) const {
    // Can't use lock in const method, assume thread safety is handled elsewhere
    auto it = bodies_.find(body_id);
    return it != bodies_.end() ? it->second.get() : nullptr;
}

void EnhancedPhysicsWorld::step(float dt) {
    if (config_.profile_performance) {
        profiler_.beginFrame();
    }
    
    // Use fixed timestep with accumulator
    accumulator_ += dt;
    
    while (accumulator_ >= config_.fixed_timestep) {
        fixedStep();
        accumulator_ -= config_.fixed_timestep;
    }
    
    if (config_.profile_performance) {
        profiler_.endFrame();
    }
}

void EnhancedPhysicsWorld::fixedStep() {
    float dt = config_.fixed_timestep;
    
    // Update sleep states
    if (config_.use_sleeping) {
        for (auto& [id, body] : bodies_) {
            body->updateSleepState(dt);
        }
    }
    
    // Collision detection
    if (config_.profile_performance) profiler_.beginPhase("collision_detection");
    
    std::vector<std::pair<uint32_t, uint32_t>> collision_pairs;
    broadPhaseCollisionDetection(collision_pairs);
    narrowPhaseCollisionDetection(collision_pairs);
    
    if (config_.profile_performance) {
        profiler_.endPhase("collision_detection");
        profiler_.getCurrentFrame().collision_pairs = static_cast<int>(collision_pairs.size());
    }
    
    // Collision resolution
    if (config_.profile_performance) profiler_.beginPhase("collision_resolution");
    
    resolveCollisions();
    
    if (config_.profile_performance) profiler_.endPhase("collision_resolution");
    
    // Integration
    if (config_.profile_performance) profiler_.beginPhase("integration");
    
    integrateVelocities(dt);
    integratePositions(dt);
    updateSpatialGrid();
    
    if (config_.profile_performance) {
        profiler_.endPhase("integration");
        
        int active_bodies = 0, sleeping_bodies = 0;
        for (const auto& [id, body] : bodies_) {
            if (body->is_sleeping) sleeping_bodies++;
            else active_bodies++;
        }
        
        profiler_.getCurrentFrame().active_bodies = active_bodies;
        profiler_.getCurrentFrame().sleeping_bodies = sleeping_bodies;
    }
}

void EnhancedPhysicsWorld::broadPhaseCollisionDetection(std::vector<std::pair<uint32_t, uint32_t>>& pairs) {
    pairs.clear();
    
    if (spatial_grid_) {
        // Use spatial hashing for broad phase
        std::unordered_set<uint64_t> tested_pairs;
        
        for (const auto& [id, body] : bodies_) {
            if (body->is_sleeping || body->material.trigger) continue;
            
            auto nearby = spatial_grid_->query(body->getBounds());
            for (uint32_t other_id : nearby) {
                if (other_id <= id) continue;  // Avoid duplicate pairs
                
                uint64_t pair_hash = (static_cast<uint64_t>(id) << 32) | other_id;
                if (tested_pairs.insert(pair_hash).second) {
                    pairs.emplace_back(id, other_id);
                }
            }
        }
    } else {
        // Brute force broad phase
        for (auto it1 = bodies_.begin(); it1 != bodies_.end(); ++it1) {
            if (it1->second->is_sleeping || it1->second->material.trigger) continue;
            
            for (auto it2 = std::next(it1); it2 != bodies_.end(); ++it2) {
                if (it2->second->is_sleeping || it2->second->material.trigger) continue;
                
                pairs.emplace_back(it1->first, it2->first);
            }
        }
    }
}

void EnhancedPhysicsWorld::narrowPhaseCollisionDetection(const std::vector<std::pair<uint32_t, uint32_t>>& pairs) {
    collision_events_.clear();
    
    for (const auto& [id1, id2] : pairs) {
        auto body1 = bodies_[id1].get();
        auto body2 = bodies_[id2].get();
        
        if (!body1->canCollideWith(*body2)) continue;
        
        // Simple AABB vs AABB or Capsule vs Capsule collision
        AABB bounds1 = body1->getBounds();
        AABB bounds2 = body2->getBounds();
        
        if (bounds1.overlaps(bounds2)) {
            CollisionEvent event;
            event.entity_a = id1;
            event.entity_b = id2;
            event.contact_point = (bounds1.center + bounds2.center) * 0.5f;
            event.normal = glm::normalize(bounds2.center - bounds1.center);
            event.penetration_depth = 0.1f;  // Simplified
            event.relative_velocity = glm::length(body1->velocity - body2->velocity);
            event.timestamp = std::chrono::high_resolution_clock::now();
            
            collision_events_.push_back(event);
            
            if (collision_callback_) {
                collision_callback_(event);
            }
        }
    }
}

void EnhancedPhysicsWorld::resolveCollisions() {
    for (const auto& event : collision_events_) {
        auto body1 = bodies_[event.entity_a].get();
        auto body2 = bodies_[event.entity_b].get();
        
        if (body1->material.trigger || body2->material.trigger) continue;
        
        // Simple collision response
        float total_mass = body1->mass + body2->mass;
        float impulse_magnitude = 2.0f * event.relative_velocity / total_mass;
        
        glm::vec3 impulse = event.normal * impulse_magnitude;
        
        body1->addImpulse(-impulse * body2->mass / total_mass);
        body2->addImpulse(impulse * body1->mass / total_mass);
        
        // Position correction
        float correction = event.penetration_depth * config_.baumgarte_factor;
        glm::vec3 correction_vector = event.normal * correction;
        
        if (!body1->is_static) {
            body1->position -= correction_vector * 0.5f;
        }
        if (!body2->is_static) {
            body2->position += correction_vector * 0.5f;
        }
    }
}

void EnhancedPhysicsWorld::integrateVelocities(float dt) {
    for (auto& [id, body] : bodies_) {
        if (body->is_static || body->is_sleeping) continue;
        
        // Apply gravity
        if (body->enable_gravity) {
            body->addForce(config_.gravity * body->mass);
        }
        
        // Apply drag
        glm::vec3 drag_force = physics_utils::calculateDragForce(body->velocity, body->material.drag);
        body->addForce(drag_force);
        
        // Integrate velocity
        body->velocity += body->acceleration * dt;
        
        // Reset acceleration
        body->acceleration = glm::vec3(0.0f);
    }
}

void EnhancedPhysicsWorld::integratePositions(float dt) {
    for (auto& [id, body] : bodies_) {
        if (body->is_static || body->is_sleeping) continue;
        
        glm::vec3 old_position = body->position;
        body->position += body->velocity * dt;
        
        // Update shape positions
        if (body->shape_type == PhysicsBody::ShapeType::AABB) {
            body->shape.aabb.center = body->position;
        } else {
            body->shape.capsule.center = body->position;
        }
    }
}

void EnhancedPhysicsWorld::updateSpatialGrid() {
    if (!spatial_grid_) return;
    
    // Rebuild spatial grid - could be optimized to only update changed bodies
    spatial_grid_->clear();
    for (const auto& [id, body] : bodies_) {
        spatial_grid_->insert(id, body->getBounds());
    }
}

size_t EnhancedPhysicsWorld::getActiveBodyCount() const {
    size_t count = 0;
    for (const auto& [id, body] : bodies_) {
        if (!body->is_sleeping) count++;
    }
    return count;
}

std::vector<uint32_t> EnhancedPhysicsWorld::queryAABB(const AABB& bounds) const {
    if (spatial_grid_) {
        return spatial_grid_->query(bounds);
    } else {
        std::vector<uint32_t> results;
        for (const auto& [id, body] : bodies_) {
            if (body->getBounds().overlaps(bounds)) {
                results.push_back(id);
            }
        }
        return results;
    }
}

std::vector<uint32_t> EnhancedPhysicsWorld::queryRadius(const glm::vec3& center, float radius) const {
    if (spatial_grid_) {
        return spatial_grid_->queryRadius(center, radius);
    } else {
        std::vector<uint32_t> results;
        for (const auto& [id, body] : bodies_) {
            float distance = glm::length(body->position - center);
            if (distance <= radius) {
                results.push_back(id);
            }
        }
        return results;
    }
}

bool EnhancedPhysicsWorld::raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance,
                                  uint32_t& hit_body, glm::vec3& hit_point, glm::vec3& hit_normal) const {
    // Simple raycast implementation - can be enhanced
    glm::vec3 normalized_dir = glm::normalize(direction);
    
    float closest_distance = max_distance;
    bool found_hit = false;
    
    for (const auto& [id, body] : bodies_) {
        AABB bounds = body->getBounds();
        
        // Simple ray-AABB intersection
        glm::vec3 inv_dir = 1.0f / normalized_dir;
        glm::vec3 t1 = (bounds.min() - origin) * inv_dir;
        glm::vec3 t2 = (bounds.max() - origin) * inv_dir;
        
        glm::vec3 tmin_vec = glm::min(t1, t2);
        glm::vec3 tmax_vec = glm::max(t1, t2);
        
        float tmin = glm::max(glm::max(tmin_vec.x, tmin_vec.y), tmin_vec.z);
        float tmax = glm::min(glm::min(tmax_vec.x, tmax_vec.y), tmax_vec.z);
        
        if (tmax >= 0 && tmin <= tmax && tmin < closest_distance) {
            closest_distance = tmin;
            hit_body = id;
            hit_point = origin + normalized_dir * tmin;
            hit_normal = glm::vec3(0, 1, 0);  // Simplified normal
            found_hit = true;
        }
    }
    
    return found_hit;
}

void EnhancedPhysicsWorld::initializeThreads() {
    // For now, single-threaded. Multi-threading can be added later
}

void EnhancedPhysicsWorld::shutdownThreads() {
    should_stop_ = true;
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
}

// Utility functions
namespace physics_utils {

glm::vec3 calculateGravityForce(float mass, const glm::vec3& gravity) {
    return mass * gravity;
}

glm::vec3 calculateDragForce(const glm::vec3& velocity, float drag_coefficient) {
    float speed_squared = glm::dot(velocity, velocity);
    if (speed_squared < 1e-6f) return glm::vec3(0.0f);
    
    return -velocity * drag_coefficient * speed_squared;
}

glm::vec3 calculateSpringForce(const glm::vec3& displacement, float spring_constant, float damping) {
    return -displacement * spring_constant;
}

bool isPointInside(const glm::vec3& point, const PhysicsBody& body) {
    if (body.shape_type == PhysicsBody::ShapeType::AABB) {
        return body.shape.aabb.contains(point);
    } else {
        return body.shape.capsule.contains(point);
    }
}

glm::vec3 interpolatePosition(const glm::vec3& prev_pos, const glm::vec3& current_pos, float alpha) {
    return glm::mix(prev_pos, current_pos, alpha);
}

bool shouldUpdateBody(const PhysicsBody& body, float dt) {
    if (body.is_static) return false;
    if (body.is_sleeping) return false;
    
    return glm::length(body.velocity) > 0.01f || glm::length(body.acceleration) > 0.01f;
}

} // namespace physics_utils

} // namespace voxelvk::physics