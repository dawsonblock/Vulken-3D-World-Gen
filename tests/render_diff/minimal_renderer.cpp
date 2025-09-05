#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <stb_image_write.h>

class MinimalRenderer {
private:
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkQueue queue;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkRenderPass renderPass;
    VkFramebuffer framebuffer;
    VkImage colorImage;
    VkDeviceMemory colorImageMemory;
    VkImageView colorImageView;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    
    uint32_t width, height;
    std::vector<uint8_t> pixels;
    
public:
    MinimalRenderer(uint32_t w, uint32_t h) : width(w), height(h) {
        pixels.resize(w * h * 4); // RGBA
    }
    
    ~MinimalRenderer() {
        cleanup();
    }
    
    bool initialize() {
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }
        
        // Create window (hidden)
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        GLFWwindow* window = glfwCreateWindow(width, height, "Minimal Renderer", nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return false;
        }
        
        // Initialize Vulkan
        if (!initVulkan()) {
            glfwDestroyWindow(window);
            glfwTerminate();
            return false;
        }
        
        glfwDestroyWindow(window);
        glfwTerminate();
        return true;
    }
    
    bool initVulkan() {
        // Create instance
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Minimal Renderer";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "VoxelVK";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;
        
        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            std::cerr << "Failed to create Vulkan instance" << std::endl;
            return false;
        }
        
        // Get physical device
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            std::cerr << "No Vulkan devices found" << std::endl;
            return false;
        }
        
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        physicalDevice = devices[0];
        
        // Create logical device
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = 0;
        queueCreateInfo.queueCount = 1;
        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        
        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.queueCreateInfoCount = 1;
        
        if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
            std::cerr << "Failed to create logical device" << std::endl;
            return false;
        }
        
        vkGetDeviceQueue(device, 0, 0, &queue);
        
        // Create command pool
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = 0;
        
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            std::cerr << "Failed to create command pool" << std::endl;
            return false;
        }
        
        // Create command buffer
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        
        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            std::cerr << "Failed to allocate command buffer" << std::endl;
            return false;
        }
        
        return true;
    }
    
    void renderTestScene(const std::string& test_name) {
        // Simple test scene rendering
        if (test_name == "basic_triangle") {
            renderBasicTriangle();
        } else if (test_name == "gradient") {
            renderGradient();
        } else if (test_name == "checkerboard") {
            renderCheckerboard();
        } else {
            renderDefault();
        }
    }
    
    void renderBasicTriangle() {
        // Render a simple triangle with gradient colors
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                uint32_t index = (y * width + x) * 4;
                
                // Simple triangle test
                float fx = static_cast<float>(x) / width;
                float fy = static_cast<float>(y) / height;
                
                // Triangle vertices: (0.2, 0.2), (0.8, 0.2), (0.5, 0.8)
                float v1x = 0.2f, v1y = 0.2f;
                float v2x = 0.8f, v2y = 0.2f;
                float v3x = 0.5f, v3y = 0.8f;
                
                // Barycentric coordinates
                float denom = (v2y - v3y) * (v1x - v3x) + (v3x - v2x) * (v1y - v3y);
                float a = ((v2y - v3y) * (fx - v3x) + (v3x - v2x) * (fy - v3y)) / denom;
                float b = ((v3y - v1y) * (fx - v3x) + (v1x - v3x) * (fy - v3y)) / denom;
                float c = 1.0f - a - b;
                
                if (a >= 0 && b >= 0 && c >= 0) {
                    // Inside triangle
                    pixels[index] = static_cast<uint8_t>(a * 255);     // R
                    pixels[index + 1] = static_cast<uint8_t>(b * 255); // G
                    pixels[index + 2] = static_cast<uint8_t>(c * 255); // B
                    pixels[index + 3] = 255;                           // A
                } else {
                    // Outside triangle - black background
                    pixels[index] = 0;
                    pixels[index + 1] = 0;
                    pixels[index + 2] = 0;
                    pixels[index + 3] = 255;
                }
            }
        }
    }
    
    void renderGradient() {
        // Render a gradient from red to blue
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                uint32_t index = (y * width + x) * 4;
                
                float fx = static_cast<float>(x) / width;
                float fy = static_cast<float>(y) / height;
                
                pixels[index] = static_cast<uint8_t>(fx * 255);         // R
                pixels[index + 1] = static_cast<uint8_t>(fy * 255);     // G
                pixels[index + 2] = static_cast<uint8_t>((1.0f - fx) * 255); // B
                pixels[index + 3] = 255;                                 // A
            }
        }
    }
    
    void renderCheckerboard() {
        // Render a checkerboard pattern
        uint32_t tile_size = 32;
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                uint32_t index = (y * width + x) * 4;
                
                uint32_t tile_x = x / tile_size;
                uint32_t tile_y = y / tile_size;
                
                bool is_white = (tile_x + tile_y) % 2 == 0;
                
                if (is_white) {
                    pixels[index] = 255;     // R
                    pixels[index + 1] = 255; // G
                    pixels[index + 2] = 255; // B
                } else {
                    pixels[index] = 0;       // R
                    pixels[index + 1] = 0;   // G
                    pixels[index + 2] = 0;   // B
                }
                pixels[index + 3] = 255;     // A
            }
        }
    }
    
    void renderDefault() {
        // Default test pattern
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                uint32_t index = (y * width + x) * 4;
                
                // Simple pattern
                pixels[index] = static_cast<uint8_t>((x * 255) / width);     // R
                pixels[index + 1] = static_cast<uint8_t>((y * 255) / height); // G
                pixels[index + 2] = 128;                                      // B
                pixels[index + 3] = 255;                                      // A
            }
        }
    }
    
    bool saveImage(const std::string& filename) {
        return stbi_write_png(filename.c_str(), width, height, 4, pixels.data(), width * 4) != 0;
    }
    
    void cleanup() {
        if (device != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            vkDestroyDevice(device, nullptr);
        }
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <test_name> <output_file> [width] [height]" << std::endl;
        return 1;
    }
    
    std::string test_name = argv[1];
    std::string output_file = argv[2];
    uint32_t width = argc > 3 ? std::stoi(argv[3]) : 800;
    uint32_t height = argc > 4 ? std::stoi(argv[4]) : 600;
    
    MinimalRenderer renderer(width, height);
    
    if (!renderer.initialize()) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return 1;
    }
    
    renderer.renderTestScene(test_name);
    
    if (!renderer.saveImage(output_file)) {
        std::cerr << "Failed to save image" << std::endl;
        return 1;
    }
    
    std::cout << "Rendered " << test_name << " to " << output_file << std::endl;
    return 0;
}