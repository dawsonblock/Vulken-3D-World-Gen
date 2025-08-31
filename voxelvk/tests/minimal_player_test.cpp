#include <iostream>
#include <memory>

// Test minimal compilation first
#include "../src/physics/cpp/aabb.hpp"
#include "../src/physics/cpp/capsule.hpp"

using namespace voxelvk::physics;

int main() {
    std::cout << "Testing minimal physics compilation..." << std::endl;
    
    // Test AABB
    AABB box(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
    std::cout << "AABB volume: " << box.volume() << std::endl;
    
    // Test Capsule
    Capsule capsule(glm::vec3(0, 1, 0), 0.9f, 0.3f);
    std::cout << "Capsule radius: " << capsule.radius << std::endl;
    
    std::cout << "Minimal physics test passed!" << std::endl;
    return 0;
}