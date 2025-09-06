#version 450

// Instanced cube vertex shader for multiple objects
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

// Instance data
layout(location = 4) in vec3 instancePos;
layout(location = 5) in vec3 instanceScale;
layout(location = 6) in vec3 instanceRotation;
layout(location = 7) in vec3 instanceColor;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec2 fragTexCoord;

// Uniform buffer for view and projection matrices
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// Rotation matrix helper
mat3 rotateX(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        1.0, 0.0, 0.0,
        0.0, c,   -s,
        0.0, s,    c
    );
}

mat3 rotateY(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        c,   0.0, s,
        0.0, 1.0, 0.0,
        -s,  0.0, c
    );
}

mat3 rotateZ(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        c,   -s,  0.0,
        s,    c,  0.0,
        0.0, 0.0, 1.0
    );
}

void main() {
    // Build instance transformation matrix
    mat3 rotation = rotateX(instanceRotation.x) * rotateY(instanceRotation.y) * rotateZ(instanceRotation.z);

    // Scale the rotation matrix
    mat3 scaledRotation = mat3(
        rotation[0][0] * instanceScale.x, rotation[0][1] * instanceScale.x, rotation[0][2] * instanceScale.x,
        rotation[1][0] * instanceScale.y, rotation[1][1] * instanceScale.y, rotation[1][2] * instanceScale.y,
        rotation[2][0] * instanceScale.z, rotation[2][1] * instanceScale.z, rotation[2][2] * instanceScale.z
    );

    // Create 4x4 transformation matrix
    mat4 model = mat4(
        vec4(scaledRotation[0], 0.0),
        vec4(scaledRotation[1], 0.0),
        vec4(scaledRotation[2], 0.0),
        vec4(instancePos, 1.0)
    );

    // Apply transformation
    vec4 worldPos = model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;

    // Pass data to fragment shader
    fragColor = inColor * instanceColor;
    fragNormal = rotation * inNormal;
    fragPos = worldPos.xyz;
    fragTexCoord = inTexCoord;
}
