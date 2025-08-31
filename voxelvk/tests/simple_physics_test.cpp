#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <memory>

// Include the physics headers
#include "../src/physics/cpp/aabb.hpp"
#include "../src/physics/cpp/capsule.hpp"
#include "../src/physics/cpp/collision_utils.hpp"
#include "../src/physics/cpp/voxel_solid.hpp"

using namespace voxelvk::physics;

// Simple test framework
int total_tests = 0;
int passed_tests = 0;

#define TEST(name) \
    void test_##name(); \
    void run_test_##name() { \
        total_tests++; \
        try { \
            test_##name(); \
            passed_tests++; \
            std::cout << "[PASS] " << #name << std::endl; \
        } catch (const std::exception& e) { \
            std::cout << "[FAIL] " << #name << " - " << e.what() << std::endl; \
        } catch (...) { \
            std::cout << "[FAIL] " << #name << " - Unknown exception" << std::endl; \
        } \
    } \
    void test_##name()

#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        throw std::runtime_error("Assertion failed: " #condition); \
    }

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        throw std::runtime_error("Assertion failed: " #a " == " #b); \
    }

#define ASSERT_FLOAT_EQ(a, b) \
    if (std::abs((a) - (b)) > 1e-5f) { \
        throw std::runtime_error("Assertion failed: " #a " ≈ " #b); \
    }

#define ASSERT_GT(a, b) \
    if ((a) <= (b)) { \
        throw std::runtime_error("Assertion failed: " #a " > " #b); \
    }

#define ASSERT_LT(a, b) \
    if ((a) >= (b)) { \
        throw std::runtime_error("Assertion failed: " #a " < " #b); \
    }

// Mock world interface for testing
class MockWorldInterface : public collision_utils::WorldInterface {
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
        
        return 0; // Air
    }
    
    bool isBlockSolid(uint16_t block_type) const override {
        return block_type != 0;
    }
};

// Tests
TEST(AABB_Basic) {
    AABB box(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
    
    ASSERT_EQ(box.center, glm::vec3(0, 0, 0));
    ASSERT_EQ(box.half_extent, glm::vec3(1, 1, 1));
    ASSERT_EQ(box.min(), glm::vec3(-1, -1, -1));
    ASSERT_EQ(box.max(), glm::vec3(1, 1, 1));
    ASSERT_EQ(box.size(), glm::vec3(2, 2, 2));
    ASSERT_FLOAT_EQ(box.volume(), 8.0f);
}

TEST(AABB_VoxelOverlap) {
    AABB box(glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0.3f, 0.3f, 0.3f));
    
    ASSERT_TRUE(box.overlapsVoxel(0, 0, 0));
    ASSERT_FALSE(box.overlapsVoxel(2, 2, 2));
}

TEST(Capsule_Basic) {
    Capsule capsule(glm::vec3(0, 1, 0), 0.9f, 0.3f);
    
    ASSERT_EQ(capsule.center, glm::vec3(0, 1, 0));
    ASSERT_FLOAT_EQ(capsule.half_height, 0.9f);
    ASSERT_FLOAT_EQ(capsule.radius, 0.3f);
    ASSERT_EQ(capsule.top(), glm::vec3(0, 1.9f, 0));
    ASSERT_EQ(capsule.bottom(), glm::vec3(0, 0.1f, 0));
}

TEST(Capsule_BoundingBox) {
    Capsule capsule(glm::vec3(0, 1, 0), 0.9f, 0.3f);
    AABB bbox = capsule.getBoundingBox();
    
    ASSERT_EQ(bbox.center, capsule.center);
    ASSERT_EQ(bbox.half_extent, glm::vec3(0.3f, 1.2f, 0.3f));
}

TEST(ClosestPointOnAABB) {
    glm::vec3 box_min(-1, -1, -1);
    glm::vec3 box_max(1, 1, 1);
    
    // Point inside box
    glm::vec3 inside(0, 0, 0);
    glm::vec3 closest_inside = collision_utils::closestPointOnAABB(inside, box_min, box_max);
    ASSERT_EQ(closest_inside, inside);
    
    // Point outside box
    glm::vec3 outside(2, 2, 2);
    glm::vec3 closest_outside = collision_utils::closestPointOnAABB(outside, box_min, box_max);
    ASSERT_EQ(closest_outside, glm::vec3(1, 1, 1));
}

TEST(ClosestPointOnSegment) {
    glm::vec3 seg_a(0, 0, 0);
    glm::vec3 seg_b(2, 0, 0);
    
    // Point at start
    glm::vec3 at_start = collision_utils::closestPointOnSegment(glm::vec3(-1, 0, 0), seg_a, seg_b);
    ASSERT_EQ(at_start, seg_a);
    
    // Point at end
    glm::vec3 at_end = collision_utils::closestPointOnSegment(glm::vec3(3, 0, 0), seg_a, seg_b);
    ASSERT_EQ(at_end, seg_b);
    
    // Point in middle
    glm::vec3 in_middle = collision_utils::closestPointOnSegment(glm::vec3(1, 1, 0), seg_a, seg_b);
    ASSERT_EQ(in_middle, glm::vec3(1, 0, 0));
}

TEST(CapsuleBoxPenetration) {
    Capsule capsule(glm::vec3(0.5f, 1.5f, 0.5f), 0.9f, 0.3f);
    glm::vec3 box_min(0, 0, 0);
    glm::vec3 box_max(1, 1, 1);
    
    // Test penetration
    auto result = collision_utils::capsuleBoxPenetration(capsule, box_min, box_max);
    ASSERT_TRUE(result.hit);
    ASSERT_GT(result.penetration_depth, 0.0f);
    
    // Test no penetration (capsule far away)
    Capsule far_capsule(glm::vec3(5, 5, 5), 0.9f, 0.3f);
    auto no_hit = collision_utils::capsuleBoxPenetration(far_capsule, box_min, box_max);
    ASSERT_FALSE(no_hit.hit);
}

TEST(CapsuleWorldResolution) {
    MockWorldInterface world;
    
    // Create capsule that intersects ground
    Capsule capsule(glm::vec3(0, 0.5f, 0), 0.9f, 0.3f);
    
    auto result = collision_utils::resolveCapsuleWorld(capsule, world);
    
    // Capsule should be pushed up
    ASSERT_GT(result.total_offset.y, 0.0f);
    ASSERT_TRUE(result.is_on_ground);
    ASSERT_GT(result.blocks_checked, 0);
    ASSERT_GT(result.iterations_used, 0);
    
    // Final position should be above ground
    ASSERT_GT(capsule.center.y, 1.2f);
}

TEST(BlockSolidity) {
    BlockRegistry& registry = BlockRegistry::getInstance();
    
    // Test basic block types
    ASSERT_FALSE(registry.isSolid(static_cast<uint16_t>(BlockType::AIR)));
    ASSERT_TRUE(registry.isSolid(static_cast<uint16_t>(BlockType::STONE)));
    ASSERT_TRUE(registry.isSolid(static_cast<uint16_t>(BlockType::DIRT)));
    ASSERT_FALSE(registry.isSolid(static_cast<uint16_t>(BlockType::WATER)));
    
    // Test global convenience functions
    ASSERT_FALSE(is_solid(static_cast<uint16_t>(BlockType::AIR)));
    ASSERT_TRUE(is_solid(static_cast<uint16_t>(BlockType::STONE)));
}

TEST(CapsuleValidation) {
    collision_utils::CollisionConfig config;
    
    // Test negative radius
    Capsule neg_radius(glm::vec3(0, 0, 0), 0.9f, -0.1f);
    bool radius_fixed = collision_utils::validateAndClampCapsule(neg_radius, config);
    ASSERT_TRUE(radius_fixed);
    ASSERT_GT(neg_radius.radius, 0.0f);
    
    // Test negative half height
    Capsule neg_height(glm::vec3(0, 0, 0), -0.1f, 0.3f);
    bool height_fixed = collision_utils::validateAndClampCapsule(neg_height, config);
    ASSERT_TRUE(height_fixed);
    ASSERT_GT(neg_height.half_height, 0.0f);
}

// Main test runner
int main() {
    std::cout << "Running C++ Physics Tests..." << std::endl;
    std::cout << "=============================" << std::endl;
    
    run_test_AABB_Basic();
    run_test_AABB_VoxelOverlap();
    run_test_Capsule_Basic();
    run_test_Capsule_BoundingBox();
    run_test_ClosestPointOnAABB();
    run_test_ClosestPointOnSegment();
    run_test_CapsuleBoxPenetration();
    run_test_CapsuleWorldResolution();
    run_test_BlockSolidity();
    run_test_CapsuleValidation();
    
    std::cout << "=============================" << std::endl;
    std::cout << "Test Results: " << passed_tests << "/" << total_tests << " passed" << std::endl;
    
    if (passed_tests == total_tests) {
        std::cout << "All tests passed! ✓" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed! ✗" << std::endl;
        return 1;
    }
}