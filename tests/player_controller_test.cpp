#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

#include "../src/physics/cpp/cpp_player_controller.hpp"
#include "../src/physics/cpp/collision_utils.hpp"
#include "../src/physics/cpp/voxel_solid.hpp"

using namespace voxelvk::physics;

// Mock world interface for testing
class TestWorldInterface : public collision_utils::WorldInterface {
public:
    uint16_t getBlockAtWorldPosition(float x, float y, float z) const override {
        int ix = static_cast<int>(std::floor(x));
        int iy = static_cast<int>(std::floor(y));
        int iz = static_cast<int>(std::floor(z));
        
        // Ground plane at Y=0
        if (iy == 0) {
            return 1; // Solid block
        }
        
        // Wall at X=5
        if (ix == 5 && iy >= 1 && iy <= 3) {
            return 1; // Solid block
        }
        
        // Platform at Y=3, X=[10,12], Z=[10,12]
        if (iy == 3 && ix >= 10 && ix <= 12 && iz >= 10 && iz <= 12) {
            return 1; // Solid block
        }
        
        return 0; // Air
    }
    
    bool isBlockSolid(uint16_t block_type) const override {
        return block_type != 0;
    }
};

void testPlayerControllerBasics() {
    std::cout << "\n=== Testing Player Controller Basics ===" << std::endl;
    
    auto world = std::make_shared<TestWorldInterface>();
    glm::vec3 spawn_pos(0, 5, 0);
    CppPlayerController player(world, spawn_pos);
    
    // Test initial state
    assert(player.getPosition() == spawn_pos);
    assert(player.getVelocity() == glm::vec3(0, 0, 0));
    assert(!player.isOnGround());
    
    std::cout << "✓ Initial state correct" << std::endl;
    
    // Test configuration
    CppPlayerController::Config config;
    config.gravity = 10.0f;
    player.setConfig(config);
    assert(player.getConfig().gravity == 10.0f);
    
    std::cout << "✓ Configuration works" << std::endl;
    
    // Test collision mode switching
    player.switchCollisionMode(CppPlayerController::CollisionMode::AABB);
    assert(player.getCollisionMode() == CppPlayerController::CollisionMode::AABB);
    
    player.switchCollisionMode(CppPlayerController::CollisionMode::CAPSULE);
    assert(player.getCollisionMode() == CppPlayerController::CollisionMode::CAPSULE);
    
    std::cout << "✓ Collision mode switching works" << std::endl;
}

void testPlayerFalling() {
    std::cout << "\n=== Testing Player Falling Physics ===" << std::endl;
    
    auto world = std::make_shared<TestWorldInterface>();
    glm::vec3 spawn_pos(0, 5, 0);  // Spawn in air
    CppPlayerController player(world, spawn_pos);
    
    // Set up empty input (no movement)
    CppPlayerController::InputState input;
    
    // Simulate falling for several frames
    const float dt = 1.0f / 60.0f;  // 60 FPS
    glm::vec3 camera_forward(1, 0, 0);
    glm::vec3 camera_right(0, 0, 1);
    
    float initial_y = player.getPosition().y;
    
    for (int frame = 0; frame < 60; ++frame) {  // 1 second of falling
        player.setInput(input);
        player.update(dt, camera_forward, camera_right);
        
        // Should be falling (negative Y velocity)
        if (frame > 5) {  // After a few frames
            assert(player.getVelocity().y < 0.0f);
        }
    }
    
    // Should have fallen significantly
    assert(player.getPosition().y < initial_y - 1.0f);
    
    // Should eventually land on ground and stop falling
    for (int frame = 0; frame < 300; ++frame) {  // 5 seconds total
        player.setInput(input);
        player.update(dt, camera_forward, camera_right);
        
        if (player.isOnGround()) {
            break;
        }
    }
    
    assert(player.isOnGround());
    assert(player.getPosition().y > 1.0f);  // Should be above ground level
    
    std::cout << "✓ Falling physics work correctly" << std::endl;
}

void testPlayerMovement() {
    std::cout << "\n=== Testing Player Movement ===" << std::endl;
    
    auto world = std::make_shared<TestWorldInterface>();
    glm::vec3 spawn_pos(0, 2, 0);  // Spawn on ground level
    CppPlayerController player(world, spawn_pos);
    
    // Let player settle on ground first
    CppPlayerController::InputState input;
    glm::vec3 camera_forward(1, 0, 0);
    glm::vec3 camera_right(0, 0, 1);
    const float dt = 1.0f / 60.0f;
    
    for (int i = 0; i < 60; ++i) {
        player.update(dt, camera_forward, camera_right);
    }
    
    glm::vec3 start_pos = player.getPosition();
    
    // Test forward movement
    input.forward = true;
    for (int frame = 0; frame < 60; ++frame) {
        player.setInput(input);
        player.update(dt, camera_forward, camera_right);
    }
    
    glm::vec3 end_pos = player.getPosition();
    
    // Should have moved forward (positive X direction)
    assert(end_pos.x > start_pos.x);
    
    std::cout << "✓ Forward movement works" << std::endl;
    
    // Test jumping
    start_pos = player.getPosition();
    input.forward = false;
    input.jump = true;
    
    player.setInput(input);
    player.update(dt, camera_forward, camera_right);
    
    // Should have jumped (positive Y velocity)
    assert(player.getVelocity().y > 0.0f);
    assert(!player.isOnGround());
    
    std::cout << "✓ Jumping works" << std::endl;
}

void testCollisionModes() {
    std::cout << "\n=== Testing Collision Modes ===" << std::endl;
    
    auto world = std::make_shared<TestWorldInterface>();
    glm::vec3 spawn_pos(0, 2, 0);
    
    // Test AABB mode
    CppPlayerController player_aabb(world, spawn_pos, CppPlayerController::CollisionMode::AABB);
    
    // Test Capsule mode
    CppPlayerController player_capsule(world, spawn_pos, CppPlayerController::CollisionMode::CAPSULE);
    
    CppPlayerController::InputState input;
    input.forward = true;
    
    glm::vec3 camera_forward(1, 0, 0);
    glm::vec3 camera_right(0, 0, 1);
    const float dt = 1.0f / 60.0f;
    
    // Run both controllers for the same input
    for (int frame = 0; frame < 120; ++frame) {
        player_aabb.setInput(input);
        player_aabb.update(dt, camera_forward, camera_right);
        
        player_capsule.setInput(input);
        player_capsule.update(dt, camera_forward, camera_right);
    }
    
    // Both should have moved, but might be slightly different due to collision differences
    assert(player_aabb.getPosition().x > spawn_pos.x);
    assert(player_capsule.getPosition().x > spawn_pos.x);
    
    std::cout << "✓ Both collision modes work" << std::endl;
}

void testPerformance() {
    std::cout << "\n=== Testing Performance ===" << std::endl;
    
    auto world = std::make_shared<TestWorldInterface>();
    glm::vec3 spawn_pos(0, 2, 0);
    
    // Test both modes
    std::vector<CppPlayerController::CollisionMode> modes = {
        CppPlayerController::CollisionMode::AABB,
        CppPlayerController::CollisionMode::CAPSULE
    };
    
    for (auto mode : modes) {
        CppPlayerController player(world, spawn_pos, mode);
        player_utils::PerformanceProfiler profiler;
        
        CppPlayerController::InputState input;
        input.forward = true;
        input.left = true;  // Diagonal movement
        
        glm::vec3 camera_forward(1, 0, 0);
        glm::vec3 camera_right(0, 0, 1);
        const float dt = 1.0f / 60.0f;
        
        // Simulate 5 seconds of gameplay
        const int num_frames = 300;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int frame = 0; frame < num_frames; ++frame) {
            player.setInput(input);
            player.update(dt, camera_forward, camera_right);
            profiler.recordFrame(player);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "Mode: " << (mode == CppPlayerController::CollisionMode::AABB ? "AABB" : "CAPSULE") << std::endl;
        std::cout << "Total time for " << num_frames << " frames: " << total_time.count() << " ms" << std::endl;
        std::cout << "Average time per frame: " << (static_cast<float>(total_time.count()) / num_frames) << " ms" << std::endl;
        
        profiler.printReport();
        
        // Performance should be reasonable (less than 1ms per frame on average)
        float avg_frame_time = static_cast<float>(total_time.count()) / num_frames;
        assert(avg_frame_time < 1.0f);
    }
    
    std::cout << "✓ Performance is acceptable" << std::endl;
}

void testWallCollision() {
    std::cout << "\n=== Testing Wall Collision ===" << std::endl;
    
    auto world = std::make_shared<TestWorldInterface>();
    glm::vec3 spawn_pos(3, 2, 0);  // Near wall at X=5
    CppPlayerController player(world, spawn_pos);
    
    CppPlayerController::InputState input;
    input.forward = true;  // Move toward wall
    
    glm::vec3 camera_forward(1, 0, 0);  // Forward is +X direction
    glm::vec3 camera_right(0, 0, 1);
    const float dt = 1.0f / 60.0f;
    
    // Move toward wall
    for (int frame = 0; frame < 180; ++frame) {  // 3 seconds
        player.setInput(input);
        player.update(dt, camera_forward, camera_right);
    }
    
    // Should have been stopped by wall, not gone through it
    assert(player.getPosition().x < 4.5f);  // Should not reach the wall
    
    std::cout << "✓ Wall collision works correctly" << std::endl;
}

int main() {
    std::cout << "Running C++ Player Controller Tests..." << std::endl;
    std::cout << "======================================" << std::endl;
    
    try {
        testPlayerControllerBasics();
        testPlayerFalling();
        testPlayerMovement();
        testCollisionModes();
        testWallCollision();
        testPerformance();
        
        std::cout << "\n======================================" << std::endl;
        std::cout << "All Player Controller tests passed! ✓" << std::endl;
        std::cout << "\nC++ Player Controller is fully functional!" << std::endl;
        std::cout << "Key features validated:" << std::endl;
        std::cout << "- Basic physics (gravity, jumping, movement)" << std::endl;
        std::cout << "- Both AABB and Capsule collision modes" << std::endl;
        std::cout << "- Wall and ground collision detection" << std::endl;
        std::cout << "- Performance profiling and optimization" << std::endl;
        std::cout << "- Configuration and state management" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cout << "\nTest failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "\nTest failed with unknown exception" << std::endl;
        return 1;
    }
}