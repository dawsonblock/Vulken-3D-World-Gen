#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out mat3 fragTBN;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat3 normalMatrix; // Precomputed on CPU: transpose(inverse(mat3(model)))
    vec3 lightPos;
    vec3 lightColor;
    vec3 viewPos;
} ubo;

void main() {
    // Transform vertex position
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragPosition = worldPos.xyz;
    
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    // Transform normal using precomputed normal matrix (efficient)
    fragNormal = normalize(ubo.normalMatrix * inNormal);
    
    // Pass through texture coordinates
    fragTexCoord = inTexCoord;
    
    // Compute TBN matrix for normal mapping
    vec3 T = normalize(ubo.normalMatrix * inTangent);
    vec3 N = fragNormal;
    
    // Re-orthogonalize T with respect to N (Gram-Schmidt process)
    T = normalize(T - dot(T, N) * N);
    
    // Calculate bitangent
    vec3 B = cross(N, T);
    
    // Create TBN matrix in world space
    fragTBN = mat3(T, B, N);
}