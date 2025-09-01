#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include "../src/physics/cpp/aabb.hpp"
#include "../src/physics/cpp/capsule.hpp"
#include "../src/physics/cpp/collision_utils.hpp"
#include "../src/physics/cpp/voxel_solid.hpp"

using namespace voxelvk::physics;

// Mock world interface for testing
class MockWorldInterface : public collision_utils::WorldInterface {
public:
    // Simple world with a ground plane at Y=0 and some obstacles
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
        return block_type != 0;  // Non-zero blocks are solid
    }
};

// Test fixture for physics tests
class PhysicsTest : public ::testing::Test {
protected:
    void SetUp() override {
        world = std::make_unique<MockWorldInterface>();
    }
    
    std::unique_ptr<MockWorldInterface> world;
};

// AABB Tests
TEST_F(PhysicsTest, AABBBasicOperations) {
    AABB box(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
    
    EXPECT_EQ(box.center, glm::vec3(0, 0, 0));
    EXPECT_EQ(box.half_extent, glm::vec3(1, 1, 1));
    EXPECT_EQ(box.min(), glm::vec3(-1, -1, -1));
    EXPECT_EQ(box.max(), glm::vec3(1, 1, 1));
    EXPECT_EQ(box.size(), glm::vec3(2, 2, 2));
    EXPECT_FLOAT_EQ(box.volume(), 8.0f);
}

TEST_F(PhysicsTest, AABBVoxelOverlap) {
    AABB box(glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0.3f, 0.3f, 0.3f));
    
    // Should overlap voxel at (0,0,0)
    EXPECT_TRUE(box.overlapsVoxel(0, 0, 0));
    
    // Should not overlap distant voxels
    EXPECT_FALSE(box.overlapsVoxel(2, 2, 2));
    EXPECT_FALSE(box.overlapsVoxel(-2, -2, -2));
}

TEST_F(PhysicsTest, AABBTransformations) {
    AABB original(glm::vec3(1, 2, 3), glm::vec3(0.5f, 0.5f, 0.5f));
    
    AABB moved = original.moved(glm::vec3(1, 1, 1));
    EXPECT_EQ(moved.center, glm::vec3(2, 3, 4));
    EXPECT_EQ(moved.half_extent, original.half_extent);
    
    AABB expanded = original.expanded(0.5f);
    EXPECT_EQ(expanded.center, original.center);
    EXPECT_EQ(expanded.half_extent, glm::vec3(1, 1, 1));
    
    AABB scaled = original.scaled(2.0f);
    EXPECT_EQ(scaled.center, glm::vec3(2, 4, 6));
    EXPECT_EQ(scaled.half_extent, glm::vec3(1, 1, 1));
}

// Capsule Tests
TEST_F(PhysicsTest, CapsuleBasicOperations) {
    Capsule capsule(glm::vec3(0, 1, 0), 0.9f, 0.3f);
    
    EXPECT_EQ(capsule.center, glm::vec3(0, 1, 0));
    EXPECT_FLOAT_EQ(capsule.half_height, 0.9f);
    EXPECT_FLOAT_EQ(capsule.radius, 0.3f);
    EXPECT_EQ(capsule.top(), glm::vec3(0, 1.9f, 0));
    EXPECT_EQ(capsule.bottom(), glm::vec3(0, 0.1f, 0));
}

TEST_F(PhysicsTest, CapsuleBoundingBox) {
    Capsule capsule(glm::vec3(0, 1, 0), 0.9f, 0.3f);
    AABB bbox = capsule.getBoundingBox();
    
    EXPECT_EQ(bbox.center, capsule.center);
    EXPECT_EQ(bbox.half_extent, glm::vec3(0.3f, 1.2f, 0.3f));  // radius, half_height + radius, radius
}

TEST_F(PhysicsTest, CapsulePlayerFactory) {
    glm::vec3 spawn_pos(5, 10, 15);
    Capsule player = Capsule::playerCapsule(spawn_pos);
    
    EXPECT_EQ(player.center, spawn_pos);
    EXPECT_FLOAT_EQ(player.radius, 0.3f);
    EXPECT_FLOAT_EQ(player.half_height, 0.9f);
}

// Collision Utilities Tests
TEST_F(PhysicsTest, ClosestPointOnAABB) {
    glm::vec3 box_min(-1, -1, -1);
    glm::vec3 box_max(1, 1, 1);
    
    // Point inside box
    glm::vec3 inside(0, 0, 0);
    glm::vec3 closest_inside = collision_utils::closestPointOnAABB(inside, box_min, box_max);
    EXPECT_EQ(closest_inside, inside);
    
    // Point outside box
    glm::vec3 outside(2, 2, 2);
    glm::vec3 closest_outside = collision_utils::closestPointOnAABB(outside, box_min, box_max);
    EXPECT_EQ(closest_outside, glm::vec3(1, 1, 1));
    
    // Point on face
    glm::vec3 on_face(0, 0, 2);
    glm::vec3 closest_face = collision_utils::closestPointOnAABB(on_face, box_min, box_max);
    EXPECT_EQ(closest_face, glm::vec3(0, 0, 1));
}

TEST_F(PhysicsTest, ClosestPointOnSegment) {
    glm::vec3 seg_a(0, 0, 0);
    glm::vec3 seg_b(2, 0, 0);
    
    // Point at start
    glm::vec3 at_start = collision_utils::closestPointOnSegment(glm::vec3(-1, 0, 0), seg_a, seg_b);
    EXPECT_EQ(at_start, seg_a);
    
    // Point at end
    glm::vec3 at_end = collision_utils::closestPointOnSegment(glm::vec3(3, 0, 0), seg_a, seg_b);
    EXPECT_EQ(at_end, seg_b);
    
    // Point in middle
    glm::vec3 in_middle = collision_utils::closestPointOnSegment(glm::vec3(1, 1, 0), seg_a, seg_b);
    EXPECT_EQ(in_middle, glm::vec3(1, 0, 0));
    
    // Degenerate segment
    glm::vec3 degenerate = collision_utils::closestPointOnSegment(glm::vec3(1, 1, 1), seg_a, seg_a);
    EXPECT_EQ(degenerate, seg_a);
}

TEST_F(PhysicsTest, CapsuleBoxPenetration) {
    Capsule capsule(glm::vec3(0.5f, 1.5f, 0.5f), 0.9f, 0.3f);
    glm::vec3 box_min(0, 0, 0);
    glm::vec3 box_max(1, 1, 1);
    
    // Test penetration
    auto result = collision_utils::capsuleBoxPenetration(capsule, box_min, box_max);
    EXPECT_TRUE(result.hit);
    EXPECT_GT(result.penetration_depth, 0.0f);
    
    // Test no penetration (capsule far away)
    Capsule far_capsule(glm::vec3(5, 5, 5), 0.9f, 0.3f);
    auto no_hit = collision_utils::capsuleBoxPenetration(far_capsule, box_min, box_max);
    EXPECT_FALSE(no_hit.hit);
    
    // Test invalid box
    auto invalid = collision_utils::capsuleBoxPenetration(capsule, box_max, box_min);  // Invalid bounds
    EXPECT_FALSE(invalid.hit);
    
    // Test invalid capsule
    Capsule invalid_capsule(glm::vec3(0, 0, 0), 0.9f, -0.1f);  // Negative radius
    auto invalid_cap = collision_utils::capsuleBoxPenetration(invalid_capsule, box_min, box_max);
    EXPECT_FALSE(invalid_cap.hit);
}

TEST_F(PhysicsTest, CapsuleWorldResolution) {
    // Create capsule that intersects ground
    Capsule capsule(glm::vec3(0, 0.5f, 0), 0.9f, 0.3f);  // Bottom at Y=0.5-0.9=-0.4, intersects ground
    
    auto result = collision_utils::resolveCapsuleWorld(capsule, *world);
    
    // Capsule should be pushed up
    EXPECT_GT(result.total_offset.y, 0.0f);
    EXPECT_TRUE(result.is_on_ground);
    EXPECT_GT(result.blocks_checked, 0);
    EXPECT_GT(result.iterations_used, 0);
    
    // Final position should be above ground
    EXPECT_GT(capsule.center.y, 1.2f);  // Should be at least radius + half_height above Y=0
}

TEST_F(PhysicsTest, CapsuleWorldResolutionWall) {
    // Create capsule that intersects wall at X=5
    Capsule capsule(glm::vec3(4.8f, 2.0f, 0), 0.9f, 0.3f);  // Slightly inside wall
    
    auto result = collision_utils::resolveCapsuleWorld(capsule, *world);
    
    // Capsule should be pushed away from wall
    EXPECT_NE(result.total_offset.x, 0.0f);
    EXPECT_GT(result.blocks_checked, 0);
    
    // Final position should be clear of wall
    EXPECT_LT(capsule.center.x, 4.7f);  // Should be pushed back
}

TEST_F(PhysicsTest, CapsuleValidation) {
    collision_utils::CollisionConfig config;
    
    // Test NaN position
    Capsule nan_capsule(glm::vec3(std::numeric_limits<float>::quiet_NaN(), 0, 0), 0.9f, 0.3f);
    bool was_modified = collision_utils::validateAndClampCapsule(nan_capsule, config);
    EXPECT_TRUE(was_modified);
    EXPECT_EQ(nan_capsule.center, glm::vec3(0, 2, 0));
    
    // Test negative radius
    Capsule neg_radius(glm::vec3(0, 0, 0), 0.9f, -0.1f);
    bool radius_fixed = collision_utils::validateAndClampCapsule(neg_radius, config);
    EXPECT_TRUE(radius_fixed);
    EXPECT_GE(neg_radius.radius, config.min_capsule_radius);
    
    // Test negative half height
    Capsule neg_height(glm::vec3(0, 0, 0), -0.1f, 0.3f);
    bool height_fixed = collision_utils::validateAndClampCapsule(neg_height, config);
    EXPECT_TRUE(height_fixed);
    EXPECT_GE(neg_height.half_height, config.min_capsule_half_height);
}

// Block Solidity Tests
TEST_F(PhysicsTest, BlockSolidity) {
    BlockRegistry& registry = BlockRegistry::getInstance();
    
    // Test basic block types
    EXPECT_FALSE(registry.isSolid(static_cast<uint16_t>(BlockType::AIR)));
    EXPECT_TRUE(registry.isSolid(static_cast<uint16_t>(BlockType::STONE)));
    EXPECT_TRUE(registry.isSolid(static_cast<uint16_t>(BlockType::DIRT)));
    EXPECT_FALSE(registry.isSolid(static_cast<uint16_t>(BlockType::WATER)));
    
    // Test global convenience functions
    EXPECT_FALSE(is_solid(static_cast<uint16_t>(BlockType::AIR)));
    EXPECT_TRUE(is_solid(static_cast<uint16_t>(BlockType::STONE)));
    
    // Test unknown block type (should fall back to non-air = solid)
    EXPECT_TRUE(is_solid(999));  // Unknown block type
}

TEST_F(PhysicsTest, BlockProperties) {
    BlockRegistry& registry = BlockRegistry::getInstance();
    
    // Test stone properties
    const BlockProperties& stone = registry.getProperties(static_cast<uint16_t>(BlockType::STONE));
    EXPECT_TRUE(stone.solid);
    EXPECT_FALSE(stone.transparent);
    EXPECT_FALSE(stone.liquid);
    EXPECT_TRUE(stone.collideable);
    
    // Test water properties
    const BlockProperties& water = registry.getProperties(static_cast<uint16_t>(BlockType::WATER));
    EXPECT_FALSE(water.solid);  // Can move through water
    EXPECT_TRUE(water.transparent);
    EXPECT_TRUE(water.liquid);
    EXPECT_FALSE(water.collideable);  // Special liquid handling
}

TEST_F(PhysicsTest, FastBlockChecker) {
    FastBlockChecker checker;
    
    // Test fast lookup
    EXPECT_FALSE(checker.isSolidFast(static_cast<uint16_t>(BlockType::AIR)));
    EXPECT_TRUE(checker.isSolidFast(static_cast<uint16_t>(BlockType::STONE)));
    
    // Test batch checking
    std::vector<uint16_t> types = {
        static_cast<uint16_t>(BlockType::AIR),
        static_cast<uint16_t>(BlockType::STONE),
        static_cast<uint16_t>(BlockType::WATER)
    };
    
    std::vector<bool> results(types.size());
    checker.batchIsSolid(types.data(), results.data(), types.size());
    
    EXPECT_FALSE(results[0]);  // Air
    EXPECT_TRUE(results[1]);   // Stone
    EXPECT_FALSE(results[2]);  // Water
}

// Performance Tests
TEST_F(PhysicsTest, CapsuleResolutionPerformance) {
    // Test with multiple capsules to ensure reasonable performance
    std::vector<Capsule> capsules;
    std::vector<collision_utils::CapsuleResolutionResult> results;
    
    const int num_capsules = 10;
    for (int i = 0; i < num_capsules; ++i) {
        capsules.emplace_back(glm::vec3(i * 0.1f, 0.5f, 0), 0.9f, 0.3f);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    collision_utils::batchResolveCapsules(capsules, *world, results);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    EXPECT_EQ(results.size(), num_capsules);
    
    // Should complete within reasonable time (adjust threshold as needed)
    EXPECT_LT(duration.count(), 10000);  // Less than 10ms for 10 capsules
    
    // All capsules should be resolved successfully
    for (const auto& result : results) {
        EXPECT_GT(result.iterations_used, 0);
        EXPECT_GT(result.blocks_checked, 0);
    }
}

// Integration Tests
TEST_F(PhysicsTest, PlayerControllerIntegration) {
    // This would test the full player controller when implemented
    // For now, test that capsule physics work correctly for player scenarios
    
    // Spawn player above ground
    Capsule player = Capsule::playerCapsule(glm::vec3(2, 5, 2));
    
    // Simulate falling to ground
    collision_utils::CollisionConfig config;
    config.ground_normal_threshold = 0.7f;
    
    auto result = collision_utils::resolveCapsuleWorldAdvanced(player, *world, config);
    
    // Player should end up on ground
    EXPECT_TRUE(result.is_on_ground);
    EXPECT_GT(result.total_offset.y, 0.0f);  // Pushed up from intersection
    EXPECT_GT(player.center.y, 1.2f);  // Above ground level
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}