#include "../src/env/raycast_dda.hpp"
#include "../src/env/world_manager.hpp"
#include "../src/core/logger.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <cmath>

using namespace voxelvk;

class RaycastDDATest : public ::testing::Test {
protected:
    void SetUp() override {
    // Configure minimal logging for tests
    Logger::SetGlobalLogLevel(LogLevel::ERROR);
        
        WorldConfig config;
        config.chunk_size = 16;  // Smaller for testing
        config.world_height = 64;
        
        world_manager = std::make_unique<WorldManager>(config);
        world_manager->Initialize();
        
        raycaster = std::make_unique<RaycastDDA>(world_manager.get());
        
        SetupTestWorld();
    }
    
    void TearDown() override {
        raycaster.reset();
        world_manager->Shutdown();
        world_manager.reset();
    // No explicit shutdown needed for Logger
    }
    
    void SetupTestWorld() {
        // Create a simple test world with some blocks
        for (int x = -8; x < 8; x++) {
            for (int z = -8; z < 8; z++) {
                // Ground layer
                world_manager->SetBlock(x, 0, z, BlockType::Grass);
                world_manager->SetBlock(x, -1, z, BlockType::Dirt);
                world_manager->SetBlock(x, -2, z, BlockType::Stone);
            }
        }
        
        // Add some obstacles
        world_manager->SetBlock(0, 1, 0, BlockType::Stone);
        world_manager->SetBlock(1, 1, 0, BlockType::Stone);
        world_manager->SetBlock(2, 1, 0, BlockType::Stone);
        world_manager->SetBlock(0, 2, 0, BlockType::Stone);
        world_manager->SetBlock(0, 3, 0, BlockType::Stone);
    }
    
    std::unique_ptr<WorldManager> world_manager;
    std::unique_ptr<RaycastDDA> raycaster;
};

TEST_F(RaycastDDATest, BasicRaycast) {
    // Test basic raycast functionality
    Vec3 origin(5, 5, 5);
    Vec3 direction(0, -1, 0);  // Straight down
    
    RaycastHit hit = raycaster->Raycast(origin, direction, 10.0f);
    
    EXPECT_TRUE(hit.hit);
    EXPECT_EQ(hit.block_type, BlockType::Grass);
    EXPECT_EQ(hit.block_pos.y, 0);
    EXPECT_EQ(hit.face, BlockUtils::BlockFace::Up);
    EXPECT_NEAR(hit.distance, 4.0f, 0.1f);
}

TEST_F(RaycastDDATest, RaycastMiss) {
    // Test raycast that should miss all blocks
    Vec3 origin(5, 5, 5);
    Vec3 direction(0, 1, 0);  // Straight up
    
    RaycastHit hit = raycaster->Raycast(origin, direction, 10.0f);
    
    EXPECT_FALSE(hit.hit);
}

TEST_F(RaycastDDATest, RaycastToObstacle) {
    // Test raycast to the stone obstacle
    Vec3 origin(-2, 1, 0);
    Vec3 direction(1, 0, 0);  // Towards stone pillar
    
    RaycastHit hit = raycaster->Raycast(origin, direction, 10.0f);
    
    EXPECT_TRUE(hit.hit);
    EXPECT_EQ(hit.block_type, BlockType::Stone);
    EXPECT_EQ(hit.block_pos.x, 0);
    EXPECT_EQ(hit.block_pos.y, 1);
    EXPECT_EQ(hit.block_pos.z, 0);
    EXPECT_EQ(hit.face, BlockUtils::BlockFace::West);
}

TEST_F(RaycastDDATest, LineOfSight) {
    // Test line of sight between two points
    Vec3 from(5, 5, 5);
    Vec3 to(5, 1, 5);
    
    bool has_los = raycaster->HasLineOfSight(from, to);
    EXPECT_TRUE(has_los);
    
    // Test blocked line of sight
    Vec3 blocked_to(0, 1, 0);  // Behind stone obstacle
    has_los = raycaster->HasLineOfSight(from, blocked_to);
    EXPECT_FALSE(has_los);
}

TEST_F(RaycastDDATest, BatchRaycast) {
    // Test batch raycast functionality
    std::vector<RaycastRequest> requests;
    
    // Create multiple rays in different directions
    Vec3 origin(5, 5, 5);
    std::vector<Vec3> directions = {
        Vec3(0, -1, 0),   // Down
        Vec3(1, 0, 0),    // Right
        Vec3(-1, 0, 0),   // Left
        Vec3(0, 0, 1),    // Forward
        Vec3(0, 0, -1)    // Backward
    };
    
    for (size_t i = 0; i < directions.size(); i++) {
        RaycastRequest req;
        req.ray = Ray(origin, directions[i], 10.0f);
        req.request_id = static_cast<uint32_t>(i);
        requests.push_back(req);
    }
    
    std::vector<RaycastHit> hits = raycaster->BatchRaycast(requests);
    
    EXPECT_EQ(hits.size(), requests.size());
    
    // Down ray should hit ground
    EXPECT_TRUE(hits[0].hit);
    EXPECT_EQ(hits[0].block_type, BlockType::Grass);
    
    // Other rays should not hit anything (within range)
    for (size_t i = 1; i < hits.size(); i++) {
        EXPECT_FALSE(hits[i].hit);
    }
}

TEST_F(RaycastDDATest, RaycastIgnoreTransparent) {
    // Add some transparent blocks
    world_manager->SetBlock(3, 1, 5, BlockType::Glass);
    world_manager->SetBlock(3, 2, 5, BlockType::Glass);
    world_manager->SetBlock(3, 3, 5, BlockType::Stone);
    
    Vec3 origin(3, 5, 5);
    Vec3 direction(0, -1, 0);
    
    // Normal raycast should hit glass
    RaycastHit hit = raycaster->Raycast(origin, direction, 10.0f);
    EXPECT_TRUE(hit.hit);
    EXPECT_EQ(hit.block_type, BlockType::Glass);
    
    // Ignore transparent should hit stone
    hit = raycaster->RaycastIgnoreTransparent(Ray(origin, direction, 10.0f));
    EXPECT_TRUE(hit.hit);
    EXPECT_EQ(hit.block_type, BlockType::Stone);
}

TEST_F(RaycastDDATest, PerformanceTest) {
    // Test raycast performance
    Vec3 origin(0, 10, 0);
    const int num_rays = 1000;
    
    Timer timer;
    
    for (int i = 0; i < num_rays; i++) {
    #ifndef M_PI
    #define M_PI 3.14159265358979323846
    #endif
    float angle = static_cast<float>(i) / num_rays * 2.0f * static_cast<float>(M_PI);
        Vec3 direction(std::cos(angle), -0.5f, std::sin(angle));
        direction = direction.normalized();
        
        raycaster->Raycast(origin, direction, 20.0f);
    }
    
    double elapsed_ms = timer.ElapsedMs();
    double rays_per_second = (num_rays / elapsed_ms) * 1000.0;
    
    std::cout << "Raycast performance: " << rays_per_second << " rays/second" << std::endl;
    
    // Should be able to cast at least 10,000 rays per second
    EXPECT_GT(rays_per_second, 10000.0);
}

TEST_F(RaycastDDATest, StatisticsCollection) {
    // Test statistics collection
    raycaster->ResetStats();
    
    Vec3 origin(5, 5, 5);
    Vec3 direction(0, -1, 0);
    
    // Cast some rays
    for (int i = 0; i < 10; i++) {
        raycaster->Raycast(origin, direction, 10.0f);
    }
    
    const auto& stats = raycaster->GetStats();
    
    EXPECT_EQ(stats.total_rays_cast, 10);
    EXPECT_EQ(stats.total_hits, 10);
    EXPECT_EQ(stats.total_misses, 0);
    EXPECT_GT(stats.avg_steps_per_ray, 0.0);
    EXPECT_GT(stats.avg_time_per_ray_us, 0.0);
}

TEST_F(RaycastDDATest, StatsNotDoubleCountWithTransparentFirst) {
    // Place glass over stone in the path
    world_manager->SetBlock(6, 5, 5, BlockType::Glass);
    world_manager->SetBlock(6, 4, 5, BlockType::Stone);
    raycaster->ResetStats();
    Vec3 origin(6, 7, 5);
    Vec3 direction(0, -1, 0);
    auto before = raycaster->GetStats();
    (void)before;
    RaycastHit hit = raycaster->Raycast(origin, direction, 10.0f);
    EXPECT_TRUE(hit.hit);
    EXPECT_EQ(hit.block_type, BlockType::Glass);
    const auto& stats = raycaster->GetStats();
    EXPECT_EQ(stats.total_rays_cast, 1) << "Raycast should count once";
    EXPECT_EQ(stats.total_hits, 1);
    EXPECT_EQ(stats.total_misses, 0);
}

// Test ray generation utilities
TEST(RaycastUtilsTest, GenerateRadialRays) {
    Vec3 center(0, 0, 0);
    float radius = 10.0f;
    int horizontal_count = 8;
    int vertical_count = 4;
    
    auto rays = RaycastUtils::GenerateRadialRays(center, radius, horizontal_count, vertical_count);
    
    EXPECT_EQ(rays.size(), horizontal_count * vertical_count);
    
    // Check that all rays start at center
    for (const auto& ray : rays) {
        EXPECT_NEAR(ray.origin.x, center.x, 0.001f);
        EXPECT_NEAR(ray.origin.y, center.y, 0.001f);
        EXPECT_NEAR(ray.origin.z, center.z, 0.001f);
        EXPECT_NEAR(ray.max_distance, radius, 0.001f);
        
        // Check direction is normalized
        float length = ray.direction.length();
        EXPECT_NEAR(length, 1.0f, 0.001f);
    }
}

TEST(RaycastUtilsTest, GenerateSphereRays) {
    Vec3 center(1, 2, 3);
    int count = 100;
    
    auto rays = RaycastUtils::GenerateSphereRays(center, count);
    
    EXPECT_EQ(rays.size(), count);
    
    // Check that directions are roughly uniformly distributed on sphere
    Vec3 sum_directions(0, 0, 0);
    for (const auto& ray : rays) {
        sum_directions = sum_directions + ray.direction;
        
        // Check direction is normalized
        float length = ray.direction.length();
        EXPECT_NEAR(length, 1.0f, 0.001f);
    }
    
    // Sum of uniformly distributed unit vectors should be close to zero
    float sum_length = sum_directions.length();
    EXPECT_LT(sum_length, 5.0f);  // Allow some variance due to randomness
}

// gtest_main provides the test runner entry point