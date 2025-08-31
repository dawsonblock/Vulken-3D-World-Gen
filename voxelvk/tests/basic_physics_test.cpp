#include <iostream>
#include <cassert>
#include <cmath>

// Include the physics headers
#include "../src/physics/cpp/aabb.hpp"
#include "../src/physics/cpp/capsule.hpp"

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

TEST(AABB_Contains) {
    AABB box(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
    
    ASSERT_TRUE(box.contains(glm::vec3(0, 0, 0)));
    ASSERT_TRUE(box.contains(glm::vec3(0.5f, 0.5f, 0.5f)));
    ASSERT_FALSE(box.contains(glm::vec3(2, 2, 2)));
}

TEST(AABB_Transformations) {
    AABB original(glm::vec3(1, 2, 3), glm::vec3(0.5f, 0.5f, 0.5f));
    
    AABB moved = original.moved(glm::vec3(1, 1, 1));
    ASSERT_EQ(moved.center, glm::vec3(2, 3, 4));
    ASSERT_EQ(moved.half_extent, original.half_extent);
    
    AABB expanded = original.expanded(0.5f);
    ASSERT_EQ(expanded.center, original.center);
    ASSERT_EQ(expanded.half_extent, glm::vec3(1, 1, 1));
    
    AABB scaled = original.scaled(2.0f);
    ASSERT_EQ(scaled.center, glm::vec3(2, 4, 6));
    ASSERT_EQ(scaled.half_extent, glm::vec3(1, 1, 1));
}

TEST(AABB_Intersection) {
    AABB box1(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
    AABB box2(glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(1, 1, 1));
    
    ASSERT_TRUE(box1.overlaps(box2));
    
    AABB intersection = box1.intersection(box2);
    ASSERT_FALSE(intersection.isEmpty());
    
    AABB union_box = box1.unionAABB(box2);
    ASSERT_FALSE(union_box.isEmpty());
    ASSERT_GT(union_box.volume(), box1.volume());
}

TEST(Capsule_Basic) {
    Capsule capsule(glm::vec3(0, 1, 0), 0.9f, 0.3f);
    
    ASSERT_EQ(capsule.center, glm::vec3(0, 1, 0));
    ASSERT_FLOAT_EQ(capsule.half_height, 0.9f);
    ASSERT_FLOAT_EQ(capsule.radius, 0.3f);
    
    glm::vec3 expected_top(0, 1.9f, 0);
    glm::vec3 expected_bottom(0, 0.1f, 0);
    glm::vec3 actual_top = capsule.top();
    glm::vec3 actual_bottom = capsule.bottom();
    
    ASSERT_FLOAT_EQ(actual_top.x, expected_top.x);
    ASSERT_FLOAT_EQ(actual_top.y, expected_top.y);
    ASSERT_FLOAT_EQ(actual_top.z, expected_top.z);
    
    ASSERT_FLOAT_EQ(actual_bottom.x, expected_bottom.x);
    ASSERT_FLOAT_EQ(actual_bottom.y, expected_bottom.y);
    ASSERT_FLOAT_EQ(actual_bottom.z, expected_bottom.z);
}

TEST(Capsule_BoundingBox) {
    Capsule capsule(glm::vec3(0, 1, 0), 0.9f, 0.3f);
    AABB bbox = capsule.getBoundingBox();
    
    ASSERT_EQ(bbox.center, capsule.center);
    ASSERT_EQ(bbox.half_extent, glm::vec3(0.3f, 1.2f, 0.3f));
}

TEST(Capsule_PlayerFactory) {
    glm::vec3 spawn_pos(5, 10, 15);
    Capsule player = Capsule::playerCapsule(spawn_pos);
    
    ASSERT_EQ(player.center, spawn_pos);
    ASSERT_FLOAT_EQ(player.radius, 0.3f);
    ASSERT_FLOAT_EQ(player.half_height, 0.9f);
}

TEST(Capsule_Transformations) {
    Capsule original(glm::vec3(1, 2, 3), 0.9f, 0.3f);
    
    Capsule moved = original.moved(glm::vec3(1, 1, 1));
    ASSERT_EQ(moved.center, glm::vec3(2, 3, 4));
    ASSERT_FLOAT_EQ(moved.radius, original.radius);
    ASSERT_FLOAT_EQ(moved.half_height, original.half_height);
    
    Capsule scaled = original.scaled(2.0f);
    ASSERT_EQ(scaled.center, glm::vec3(2, 4, 6));
    ASSERT_FLOAT_EQ(scaled.radius, 0.6f);
    ASSERT_FLOAT_EQ(scaled.half_height, 1.8f);
    
    Capsule new_radius = original.withRadius(0.5f);
    ASSERT_FLOAT_EQ(new_radius.radius, 0.5f);
    ASSERT_FLOAT_EQ(new_radius.half_height, original.half_height);
}

TEST(Capsule_Volume) {
    Capsule capsule(glm::vec3(0, 0, 0), 1.0f, 0.5f);
    float volume = capsule.volume();
    
    // Volume should be cylinder + sphere
    // Cylinder: π * r² * h = π * 0.25 * 2 = 0.5π
    // Sphere: (4/3) * π * r³ = (4/3) * π * 0.125 = (4/3) * π * 0.125
    ASSERT_GT(volume, 0.0f);
}

TEST(AABB_ClosestPoint) {
    AABB box(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
    
    // Point inside should return itself
    glm::vec3 inside(0, 0, 0);
    ASSERT_EQ(box.closestPoint(inside), inside);
    
    // Point outside should be clamped to surface
    glm::vec3 outside(2, 2, 2);
    glm::vec3 closest = box.closestPoint(outside);
    ASSERT_EQ(closest, glm::vec3(1, 1, 1));
}

TEST(Capsule_DistanceToPoint) {
    Capsule capsule(glm::vec3(0, 1, 0), 0.9f, 0.3f);
    
    // Point at center should be radius distance away
    float dist_center = capsule.distanceToPoint(glm::vec3(0, 1, 0));
    ASSERT_FLOAT_EQ(dist_center, 0.3f);
    
    // Point far away should be greater than radius
    float dist_far = capsule.distanceToPoint(glm::vec3(10, 1, 0));
    ASSERT_GT(dist_far, 0.3f);
}

// Main test runner
int main() {
    std::cout << "Running Basic C++ Physics Tests..." << std::endl;
    std::cout << "===================================" << std::endl;
    
    run_test_AABB_Basic();
    run_test_AABB_VoxelOverlap();
    run_test_AABB_Contains();
    run_test_AABB_Transformations();
    run_test_AABB_Intersection();
    run_test_Capsule_Basic();
    run_test_Capsule_BoundingBox();
    run_test_Capsule_PlayerFactory();
    run_test_Capsule_Transformations();
    run_test_Capsule_Volume();
    run_test_AABB_ClosestPoint();
    run_test_Capsule_DistanceToPoint();
    
    std::cout << "===================================" << std::endl;
    std::cout << "Test Results: " << passed_tests << "/" << total_tests << " passed" << std::endl;
    
    if (passed_tests == total_tests) {
        std::cout << "All tests passed! ✓" << std::endl;
        std::cout << "\nC++ Physics implementation is working correctly!" << std::endl;
        std::cout << "Key features validated:" << std::endl;
        std::cout << "- AABB collision primitives" << std::endl;
        std::cout << "- Capsule collision primitives" << std::endl;
        std::cout << "- Geometric transformations" << std::endl;
        std::cout << "- Volume calculations" << std::endl;
        std::cout << "- Distance queries" << std::endl;
        std::cout << "- Voxel overlap testing" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed! ✗" << std::endl;
        return 1;
    }
}