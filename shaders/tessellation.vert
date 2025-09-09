#version 450

// Tessellation vertex shader
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

// Uniform buffer for transformation matrices
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    // Pass data to tessellation control shader
    fragColor = inColor;
    fragNormal = inNormal;
    fragTexCoord = inTexCoord;

    // Transform position to world space
    gl_Position = ubo.model * vec4(inPosition, 1.0);
}
