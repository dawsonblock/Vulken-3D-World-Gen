#include <iostream>
#include <vulkan/vulkan.h>

int main() {
    std::cout << "Operator Console - Vulkan System Monitor" << std::endl;

    // Basic Vulkan initialization check
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::cout << "Vulkan extensions available: " << extensionCount << std::endl;
    return 0;
}
