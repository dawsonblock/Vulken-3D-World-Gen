#include <iostream>
#include <vector>
#include <array>
#include <cmath>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texCoord;
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// Example vertex data for a cube
const std::vector<Vertex> original_vertices = {
    // Front face
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
    
    // Back face  
    {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
    {{-0.5f,  0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.5f, 0.0f}, {0.0f, 1.0f}},

    // Duplicate vertices for different faces (before optimization)
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 8 (duplicate of 0)
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}, // 9 (duplicate of 1)
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // 10 (duplicate of 2)
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}, // 11 (duplicate of 3)
};

// Optimized unique vertices
const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
    {{-0.5f,  0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.5f, 0.0f}, {0.0f, 1.0f}},
};

// Index buffer
const std::vector<uint16_t> indices = {
    // Front face
    0, 1, 2,  2, 3, 0,
    // Back face
    4, 5, 6,  6, 7, 4,
    // Left face
    5, 0, 3,  3, 6, 5,
    // Right face
    1, 4, 7,  7, 2, 1,
    // Top face
    3, 2, 7,  7, 6, 3,
    // Bottom face
    5, 4, 1,  1, 0, 5
};

void demonstrateIndexBuffers() {
    std::cout << "🔍 === INDEX BUFFER OPTIMIZATION DEMONSTRATION ===" << std::endl;
    
    // Calculate memory usage correctly using byte sizes
    size_t original_vertex_count = indices.size(); // This would be the vertex count without indexing
    size_t unique_vertex_count = vertices.size();
    
    // Calculate memory usage in bytes
    size_t memory_without_indices = original_vertex_count * sizeof(Vertex);
    size_t memory_with_indices = unique_vertex_count * sizeof(Vertex) + indices.size() * sizeof(uint16_t);
    
    // Calculate savings as percentage
    float savings = (1.0f - static_cast<float>(memory_with_indices) / static_cast<float>(memory_without_indices)) * 100.0f;
    
    std::cout << "📊 Memory Usage Analysis:" << std::endl;
    std::cout << "  Without indexing: " << original_vertex_count << " vertices × " << sizeof(Vertex) << " bytes = " << memory_without_indices << " bytes" << std::endl;
    std::cout << "  With indexing: " << unique_vertex_count << " vertices × " << sizeof(Vertex) << " bytes + " << indices.size() << " indices × " << sizeof(uint16_t) << " bytes = " << memory_with_indices << " bytes" << std::endl;
    std::cout << "  Memory savings: " << savings << "%" << std::endl;
    
    std::cout << "✅ Index Buffer Benefits:" << std::endl;
    std::cout << "  - Eliminates duplicate vertex data" << std::endl;
    std::cout << "  - Reduces memory bandwidth requirements" << std::endl;
    std::cout << "  - Improves vertex cache efficiency" << std::endl;
    std::cout << "  - Scales better with complex models" << std::endl;
}

void demonstrateMVPTransforms() {
    std::cout << "\n🎯 === MVP MATRIX TRANSFORMATION PIPELINE ===" << std::endl;
    
    UniformBufferObject ubo{};
    
    // Model matrix - rotation around Y axis
    float time = 1.5f; // Simulated time
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    
    // View matrix - look at the origin from a distance
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    
    // Projection matrix - perspective
    ubo.proj = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 10.0f);
    
    // Fix rotation calculation - correct Y-axis rotation from model matrix
    float rotation_y = glm::degrees(std::atan2(ubo.model[2][0], ubo.model[0][0]));
    
    std::cout << "🔧 Matrix Breakdown:" << std::endl;
    std::cout << "  Model Matrix: Rotation around Y-axis = " << rotation_y << " degrees" << std::endl;
    std::cout << "  View Matrix: Camera at (2,2,2) looking at origin" << std::endl;
    std::cout << "  Projection: 45° FOV, 4:3 aspect ratio, 0.1-10.0 depth range" << std::endl;
    
    std::cout << "✅ MVP Pipeline:" << std::endl;
    std::cout << "  1. Model space → World space (Model matrix)" << std::endl;
    std::cout << "  2. World space → View/Camera space (View matrix)" << std::endl;
    std::cout << "  3. View space → Clip space (Projection matrix)" << std::endl;
    std::cout << "  4. Clip space → NDC → Screen space (GPU/rasterizer)" << std::endl;
    
    // Alternative robust rotation extraction using quaternions
    glm::quat rotation_quat = glm::quat_cast(ubo.model);
    glm::vec3 euler_angles = glm::eulerAngles(rotation_quat);
    std::cout << "  Alternative Y rotation (quaternion method): " << glm::degrees(euler_angles.y) << " degrees" << std::endl;
}

void demonstrateVertexAttributes() {
    std::cout << "\n📐 === VERTEX ATTRIBUTE LAYOUT ===" << std::endl;
    
    std::cout << "🔹 Vertex Structure:" << std::endl;
    std::cout << "  Position: vec3 (12 bytes) - XYZ coordinates in model space" << std::endl;
    std::cout << "  Color: vec3 (12 bytes) - RGB color values [0-1]" << std::endl;
    std::cout << "  TexCoord: vec2 (8 bytes) - UV texture coordinates [0-1]" << std::endl;
    std::cout << "  Total vertex size: " << sizeof(Vertex) << " bytes" << std::endl;
    
    std::cout << "✅ Attribute Benefits:" << std::endl;
    std::cout << "  - Positions enable 3D transformation and projection" << std::endl;
    std::cout << "  - Colors provide per-vertex color interpolation" << std::endl;
    std::cout << "  - Texture coordinates enable surface detail mapping" << std::endl;
    std::cout << "  - Interleaved layout improves memory access patterns" << std::endl;
    
    // Display first few vertices as examples
    std::cout << "📊 Sample Vertices:" << std::endl;
    for (size_t i = 0; i < std::min(static_cast<size_t>(4), vertices.size()); ++i) {
        const auto& v = vertices[i];
        std::cout << "  Vertex " << i << ": pos(" << v.position.x << "," << v.position.y << "," << v.position.z 
                  << ") color(" << v.color.x << "," << v.color.y << "," << v.color.z 
                  << ") uv(" << v.texCoord.x << "," << v.texCoord.y << ")" << std::endl;
    }
}

void demonstrateRenderingConcepts() {
    std::cout << "\n🎨 === 3D RENDERING CONCEPTS ===" << std::endl;
    
    std::cout << "🔹 Geometry Processing:" << std::endl;
    std::cout << "  - Vertex transformation through MVP pipeline" << std::endl;
    std::cout << "  - Primitive assembly (triangles from vertices)" << std::endl;
    std::cout << "  - Clipping against view frustum" << std::endl;
    std::cout << "  - Perspective division to NDC" << std::endl;
    
    std::cout << "🔹 Rasterization:" << std::endl;
    std::cout << "  - Triangle setup and edge equation calculation" << std::endl;
    std::cout << "  - Fragment generation for covered pixels" << std::endl;
    std::cout << "  - Attribute interpolation (colors, UVs, normals)" << std::endl;
    std::cout << "  - Depth testing and fragment discard" << std::endl;
    
    std::cout << "🔹 Fragment Processing:" << std::endl;
    std::cout << "  - Pixel shader execution per fragment" << std::endl;
    std::cout << "  - Texture sampling and filtering" << std::endl;
    std::cout << "  - Lighting calculations" << std::endl;
    std::cout << "  - Alpha blending and output merge" << std::endl;
    
    std::cout << "✅ Optimization Techniques:" << std::endl;
    std::cout << "  - Index buffers reduce vertex processing overhead" << std::endl;
    std::cout << "  - Vertex cache improves transform reuse" << std::endl;
    std::cout << "  - Early depth testing reduces pixel shader workload" << std::endl;
    std::cout << "  - Texture compression reduces memory bandwidth" << std::endl;
}

int main() {
    try {
        std::cout << "🚀 === RENDERING CONCEPTS DEMONSTRATION ===" << std::endl;
        std::cout << "Educational demo covering fundamental 3D rendering concepts" << std::endl;
        
        demonstrateIndexBuffers();
        demonstrateMVPTransforms();
        demonstrateVertexAttributes();
        demonstrateRenderingConcepts();
        
        std::cout << "\n🎉 === CONCEPTS DEMO COMPLETED ===" << std::endl;
        std::cout << "Key concepts covered:" << std::endl;
        std::cout << "  ✅ Index buffer memory optimization (corrected calculation)" << std::endl;
        std::cout << "  ✅ MVP matrix transformations (fixed rotation extraction)" << std::endl;
        std::cout << "  ✅ Vertex attribute layout and interleaving" << std::endl;
        std::cout << "  ✅ 3D rendering pipeline stages" << std::endl;
        std::cout << "  ✅ Performance optimization techniques" << std::endl;
        std::cout << "🎓 This demo provides theoretical foundation for 3D graphics programming" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}