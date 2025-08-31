#include <iostream>
#include <memory>

// Test compilation of headers in dependency order
#include "../src/physics/cpp/aabb.hpp"
#include "../src/physics/cpp/capsule.hpp"
#include "../src/physics/cpp/voxel_solid.hpp"
#include "../src/physics/cpp/collision_utils.hpp"
#include "../src/physics/cpp/cpp_player_controller.hpp"

using namespace voxelvk::physics;

// Mock world interface for testing
class SimpleTestWorld : public collision_utils::WorldInterface {
public:
    uint16_t getBlockAtWorldPosition(float x, float y, float z) const override {
        int iy = static_cast<int>(std::floor(y));
        return (iy == 0) ? 1 : 0;  // Ground at Y=0
    }
    
    bool isBlockSolid(uint16_t block_type) const override {
        return block_type != 0;
    }
};

int main() {
    std::cout << "Testing compilation of C++ physics headers..." << std::endl;
    
    // Test basic primitives
    AABB box(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
    Capsule capsule(glm::vec3(0, 1, 0), 0.9f, 0.3f);
    
    std::cout << "✓ AABB and Capsule compile successfully" << std::endl;
    
    // Test block registry
    BlockRegistry& registry = BlockRegistry::getInstance();
    bool solid = registry.isSolid(1);
    
    std::cout << "✓ Block registry compiles successfully" << std::endl;
    
    // Test collision utilities
    glm::vec3 closest = collision_utils::closestPointOnAABB(
        glm::vec3(2, 2, 2), glm::vec3(-1, -1, -1), glm::vec3(1, 1, 1));
    
    std::cout << "✓ Collision utilities compile successfully" << std::endl;
    
    // Test player controller
    auto world = std::make_shared<SimpleTestWorld>();
    CppPlayerController player(world, glm::vec3(0, 2, 0));
    
    std::cout << "✓ Player controller compiles successfully" << std::endl;
    
    std::cout << "\nAll C++ physics headers compile successfully! 🎉" << std::endl;
    return 0;
}