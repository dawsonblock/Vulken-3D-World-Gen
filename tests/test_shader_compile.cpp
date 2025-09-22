#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <cstdlib>

class ShaderCompileTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Check if glslc is available
        int result = std::system("glslc --version > /dev/null 2>&1");
        if (result != 0) {
            GTEST_SKIP() << "glslc not available - skipping shader compilation tests";
        }

        // Create temporary directory for test shaders
        testDir = std::filesystem::temp_directory_path() / "vulkan_shader_test";
        std::filesystem::create_directories(testDir);
    }

    void TearDown() override {
        // Clean up test directory
        if (std::filesystem::exists(testDir)) {
            std::filesystem::remove_all(testDir);
        }
    }

    std::filesystem::path testDir;
};

TEST_F(ShaderCompileTest, CompileSimpleVertexShader) {
    // Create a simple vertex shader
    std::string vertexShader = R"(
#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragNormal = inNormal;
    fragTexCoord = inTexCoord;
}
)";

    std::filesystem::path vertexFile = testDir / "test.vert";
    std::ofstream file(vertexFile);
    file << vertexShader;
    file.close();

    // Compile the shader
    std::string command = "glslc --target-env=vulkan1.3 -o " +
                         (testDir / "test.vert.spv").string() + " " +
                         vertexFile.string();

    int result = std::system(command.c_str());
    EXPECT_EQ(result, 0) << "Failed to compile vertex shader";

    // Check if SPIR-V file was created
    EXPECT_TRUE(std::filesystem::exists(testDir / "test.vert.spv"));
}

TEST_F(ShaderCompileTest, CompileSimpleFragmentShader) {
    // Create a simple fragment shader
    std::string fragmentShader = R"(
#version 450
layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float diff = max(dot(normalize(fragNormal), lightDir), 0.0);
    vec3 color = texture(texSampler, fragTexCoord).rgb;
    outColor = vec4(color * diff, 1.0);
}
)";

    std::filesystem::path fragmentFile = testDir / "test.frag";
    std::ofstream file(fragmentFile);
    file << fragmentShader;
    file.close();

    // Compile the shader
    std::string command = "glslc --target-env=vulkan1.3 -o " +
                         (testDir / "test.frag.spv").string() + " " +
                         fragmentFile.string();

    int result = std::system(command.c_str());
    EXPECT_EQ(result, 0) << "Failed to compile fragment shader";

    // Check if SPIR-V file was created
    EXPECT_TRUE(std::filesystem::exists(testDir / "test.frag.spv"));
}

TEST_F(ShaderCompileTest, CompileComputeShader) {
    // Create a simple compute shader
    std::string computeShader = R"(
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) uniform UniformBufferObject {
    float time;
    float deltaTime;
} ubo;

layout(binding = 1) buffer StorageBuffer {
    float data[];
} storage;

void main() {
    uint index = gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * gl_WorkGroupSize.x;
    storage.data[index] = sin(ubo.time + index * 0.1);
}
)";

    std::filesystem::path computeFile = testDir / "test.comp";
    std::ofstream file(computeFile);
    file << computeShader;
    file.close();

    // Compile the shader
    std::string command = "glslc --target-env=vulkan1.3 -o " +
                         (testDir / "test.comp.spv").string() + " " +
                         computeFile.string();

    int result = std::system(command.c_str());
    EXPECT_EQ(result, 0) << "Failed to compile compute shader";

    // Check if SPIR-V file was created
    EXPECT_TRUE(std::filesystem::exists(testDir / "test.comp.spv"));
}

TEST_F(ShaderCompileTest, CompileInvalidShader) {
    // Create an invalid shader
    std::string invalidShader = R"(
#version 450
layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = inPosition; // Missing vec4 conversion
}
)";

    std::filesystem::path invalidFile = testDir / "invalid.vert";
    std::ofstream file(invalidFile);
    file << invalidShader;
    file.close();

    // Try to compile the invalid shader
    std::string command = "glslc --target-env=vulkan1.3 -o " +
                         (testDir / "invalid.vert.spv").string() + " " +
                         invalidFile.string();

    int result = std::system(command.c_str());
    EXPECT_NE(result, 0) << "Invalid shader should fail to compile";

    // Check that SPIR-V file was not created
    EXPECT_FALSE(std::filesystem::exists(testDir / "invalid.vert.spv"));
}

TEST_F(ShaderCompileTest, CompileWithOptimization) {
    // Create a simple shader
    std::string shader = R"(
#version 450
layout(location = 0) in vec3 inPosition;
layout(binding = 0) uniform UniformBufferObject {
    mat4 mvp;
} ubo;

void main() {
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
}
)";

    std::filesystem::path shaderFile = testDir / "optimized.vert";
    std::ofstream file(shaderFile);
    file << shader;
    file.close();

    // Compile with optimization
    std::string command = "glslc --target-env=vulkan1.3 -O -o " +
                         (testDir / "optimized.vert.spv").string() + " " +
                         shaderFile.string();

    int result = std::system(command.c_str());
    EXPECT_EQ(result, 0) << "Failed to compile optimized shader";

    // Check if SPIR-V file was created
    EXPECT_TRUE(std::filesystem::exists(testDir / "optimized.vert.spv"));
}
