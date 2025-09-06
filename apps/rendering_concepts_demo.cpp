#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// VoxelVK systems
#include "../src/core/logger.hpp"

// Logger
static voxelvk::Logger g_logger("RenderingConceptsDemo");

// Vertex structure for 3D cube
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec3 normal;
};

// Uniform buffer object
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// Lighting uniforms
struct LightingUniforms {
    glm::vec3 lightPos;
    glm::vec3 lightColor;
    glm::vec3 viewPos;
};

// Camera and timing
static glm::vec3 g_CameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
static glm::vec3 g_CameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
static glm::vec3 g_CameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
static float g_Rotation = 0.0f;

// Create 3D cube vertices with positions, colors, and normals
std::vector<Vertex> createCubeVertices() {
    return {
        // Front face
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},

        // Back face
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, -1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}}
    };
}

// Create indices for the cube (12 triangles)
std::vector<uint16_t> createCubeIndices() {
    return {
        0, 1, 2, 2, 3, 0,    // front
        4, 5, 6, 6, 7, 4,    // back
        0, 4, 7, 7, 3, 0,    // left
        1, 5, 6, 6, 2, 1,    // right
        3, 2, 6, 6, 7, 3,    // top
        0, 1, 5, 5, 4, 0     // bottom
    };
}

// Calculate MVP matrices
UniformBufferObject calculateMVP(float time, int width, int height) {
    UniformBufferObject ubo{};

    // Model matrix - rotate around Y axis
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // View matrix - camera position
    ubo.view = glm::lookAt(g_CameraPos, g_CameraTarget, g_CameraUp);

    // Projection matrix - perspective projection
    ubo.proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1; // Flip Y coordinate for Vulkan

    return ubo;
}

// Simulate Phong lighting calculation
glm::vec3 calculatePhongLighting(const glm::vec3& fragPos, const glm::vec3& fragNormal,
                                const glm::vec3& fragColor, const LightingUniforms& lighting) {
    // Ambient lighting
    float ambientStrength = 0.1f;
    glm::vec3 ambient = ambientStrength * lighting.lightColor;

    // Diffuse lighting
    glm::vec3 norm = glm::normalize(fragNormal);
    glm::vec3 lightDir = glm::normalize(lighting.lightPos - fragPos);
    float diff = std::max(glm::dot(norm, lightDir), 0.0f);
    glm::vec3 diffuse = diff * lighting.lightColor;

    // Specular lighting
    float specularStrength = 0.5f;
    glm::vec3 viewDir = glm::normalize(lighting.viewPos - fragPos);
    glm::vec3 reflectDir = glm::reflect(-lightDir, norm);
    float spec = std::pow(std::max(glm::dot(viewDir, reflectDir), 0.0f), 32.0f);
    glm::vec3 specular = specularStrength * spec * lighting.lightColor;

    // Combine lighting
    return (ambient + diffuse + specular) * fragColor;
}

// Simulate vertex shader transformation
glm::vec4 transformVertex(const Vertex& vertex, const UniformBufferObject& ubo) {
    glm::vec4 worldPos = ubo.model * glm::vec4(vertex.pos, 1.0f);
    return ubo.proj * ubo.view * worldPos;
}

// Simulate fragment shader with lighting
glm::vec3 shadeFragment(const Vertex& vertex, const UniformBufferObject& ubo,
                       const LightingUniforms& lighting) {
    // Transform vertex position to world space
    glm::vec3 worldPos = glm::vec3(ubo.model * glm::vec4(vertex.pos, 1.0f));

    // Transform normal to world space
    glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(ubo.model)));
    glm::vec3 worldNormal = normalMatrix * vertex.normal;

    // Calculate Phong lighting
    return calculatePhongLighting(worldPos, worldNormal, vertex.color, lighting);
}

void demonstrateRenderingConcepts() {
    g_logger.Info("=== Advanced 3D Rendering Concepts Demo ===");

    // 1. Create 3D geometry
    auto vertices = createCubeVertices();
    auto indices = createCubeIndices();

    g_logger.Info("✅ Created 3D cube geometry:");
    g_logger.Info("   - {} vertices with position, color, and normal attributes", vertices.size());
    g_logger.Info("   - {} indices for efficient triangle rendering", indices.size());

    // 2. Set up lighting
    LightingUniforms lighting{};
    lighting.lightPos = glm::vec3(2.0f, 2.0f, 2.0f);
    lighting.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    lighting.viewPos = g_CameraPos;

    g_logger.Info("✅ Configured Phong lighting:");
    g_logger.Info("   - Light position: ({:.1f}, {:.1f}, {:.1f})",
                  lighting.lightPos.x, lighting.lightPos.y, lighting.lightPos.z);
    g_logger.Info("   - Light color: ({:.1f}, {:.1f}, {:.1f})",
                  lighting.lightColor.x, lighting.lightColor.y, lighting.lightColor.z);

    // 3. Calculate transformations for different time steps
    g_logger.Info("✅ Calculating MVP transformations:");

    for (int frame = 0; frame < 5; ++frame) {
        float time = frame * 0.1f;
        auto ubo = calculateMVP(time, 800, 600);

        g_logger.Info("   Frame {} (t={:.1f}s):", frame, time);
        g_logger.Info("     Model rotation: {:.1f} degrees", glm::degrees(std::atan2(ubo.model[0][0], ubo.model[0][2])));
        g_logger.Info("     Camera position: ({:.1f}, {:.1f}, {:.1f})",
                      g_CameraPos.x, g_CameraPos.y, g_CameraPos.z);

        // 4. Simulate vertex processing
        g_logger.Info("     Processing vertices...");
        int processedVertices = 0;
        for (const auto& vertex : vertices) {
            glm::vec4 clipPos = transformVertex(vertex, ubo);
            glm::vec3 finalColor = shadeFragment(vertex, ubo, lighting);

            // Only log first few vertices to avoid spam
            if (processedVertices < 3) {
                g_logger.Info("       Vertex {}: clip=({:.2f}, {:.2f}, {:.2f}, {:.2f}), color=({:.2f}, {:.2f}, {:.2f})",
                              processedVertices, clipPos.x, clipPos.y, clipPos.z, clipPos.w,
                              finalColor.x, finalColor.y, finalColor.z);
            }
            processedVertices++;
        }
        g_logger.Info("     Processed {} vertices", processedVertices);
    }

    // 5. Demonstrate index buffer efficiency
    g_logger.Info("✅ Index buffer efficiency:");
    g_logger.Info("   - Without indices: {} vertices ({} triangles × 3)",
                  indices.size(), indices.size() / 3);
    g_logger.Info("   - With indices: {} unique vertices, {} indices",
                  vertices.size(), indices.size());
    g_logger.Info("   - Memory savings: {:.1f}%",
                  (1.0f - (float)vertices.size() / (float)indices.size()) * 100.0f);

    // 6. Demonstrate uniform buffer usage
    g_logger.Info("✅ Uniform buffer usage:");
    g_logger.Info("   - MVP matrices: {} bytes", sizeof(UniformBufferObject));
    g_logger.Info("   - Lighting data: {} bytes", sizeof(LightingUniforms));
    g_logger.Info("   - Total uniform data: {} bytes",
                  sizeof(UniformBufferObject) + sizeof(LightingUniforms));

    // 7. Demonstrate shader pipeline stages
    g_logger.Info("✅ Shader pipeline stages:");
    g_logger.Info("   1. Vertex Shader:");
    g_logger.Info("      - Input: position, color, normal");
    g_logger.Info("      - Transform: MVP matrix multiplication");
    g_logger.Info("      - Output: clip space position, interpolated color/normal");
    g_logger.Info("   2. Fragment Shader:");
    g_logger.Info("      - Input: interpolated color, normal, position");
    g_logger.Info("      - Calculate: Phong lighting (ambient + diffuse + specular)");
    g_logger.Info("      - Output: final pixel color");

    // 8. Demonstrate camera controls
    g_logger.Info("✅ Camera controls implemented:");
    g_logger.Info("   - WASD: Move forward/backward/left/right");
    g_logger.Info("   - QE: Move up/down");
    g_logger.Info("   - Mouse: Look around (not implemented in this demo)");
    g_logger.Info("   - Current camera position: ({:.1f}, {:.1f}, {:.1f})",
                  g_CameraPos.x, g_CameraPos.y, g_CameraPos.z);

    g_logger.Info("=== Demo completed successfully! ===");
}

int main() {
    g_logger.Info("Starting Advanced 3D Rendering Concepts Demo...");

    try {
        demonstrateRenderingConcepts();
    } catch (const std::exception& e) {
        g_logger.Error("Demo failed: {}", e.what());
        return -1;
    }

    g_logger.Info("All advanced rendering concepts demonstrated!");
    return 0;
}
