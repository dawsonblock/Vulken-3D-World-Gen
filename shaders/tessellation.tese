#version 450

// Tessellation evaluation shader
layout(triangles, equal_spacing, cw) in;

// Input from tessellation control shader
layout(location = 0) in vec3 tessColor[];
layout(location = 1) in vec3 tessNormal[];
layout(location = 2) in vec2 tessTexCoord[];

// Output to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragPos;

// Uniform buffers
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(binding = 1) uniform TessellationUniforms {
    float tessellationLevel;
    float displacementScale;
    float edgeTessellationFactor;
    float innerTessellationFactor;
    vec3 cameraPos;
    float maxTessellationLevel;
    float minTessellationLevel;
} tessUniforms;

// Displacement textures
layout(binding = 2) uniform sampler2D displacementTexture;
layout(binding = 3) uniform sampler2D normalTexture;

// Noise function for procedural displacement
float noise(vec2 uv) {
    return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
}

// Fractal noise for detailed displacement
float fractalNoise(vec2 uv, int octaves) {
    float value = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;

    for(int i = 0; i < octaves; ++i) {
        value += amplitude * noise(uv * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }

    return value;
}

// Calculate displacement
float calculateDisplacement(vec2 uv) {
    // Sample displacement texture
    float textureDisplacement = texture(displacementTexture, uv).r;

    // Add procedural noise
    float noiseDisplacement = fractalNoise(uv * 8.0, 4);

    // Combine displacements
    float totalDisplacement = textureDisplacement + noiseDisplacement * 0.1;

    return totalDisplacement * tessUniforms.displacementScale;
}

// Calculate normal from displacement
vec3 calculateDisplacedNormal(vec2 uv, float displacement) {
    float texelSize = 1.0 / 1024.0; // Assuming 1024x1024 texture

    // Sample neighboring heights
    float heightL = calculateDisplacement(uv + vec2(-texelSize, 0.0));
    float heightR = calculateDisplacement(uv + vec2(texelSize, 0.0));
    float heightD = calculateDisplacement(uv + vec2(0.0, -texelSize));
    float heightU = calculateDisplacement(uv + vec2(0.0, texelSize));

    // Calculate normal using finite differences
    vec3 normal = normalize(vec3(
        heightL - heightR,
        2.0 * texelSize,
        heightD - heightU
    ));

    return normal;
}

void main() {
    // Interpolate vertex attributes
    vec3 position = gl_TessCoord.x * gl_in[0].gl_Position.xyz +
                   gl_TessCoord.y * gl_in[1].gl_Position.xyz +
                   gl_TessCoord.z * gl_in[2].gl_Position.xyz;

    vec3 color = gl_TessCoord.x * tessColor[0] +
                gl_TessCoord.y * tessColor[1] +
                gl_TessCoord.z * tessColor[2];

    vec3 normal = gl_TessCoord.x * tessNormal[0] +
                 gl_TessCoord.y * tessNormal[1] +
                 gl_TessCoord.z * tessNormal[2];

    vec2 texCoord = gl_TessCoord.x * tessTexCoord[0] +
                   gl_TessCoord.y * tessTexCoord[1] +
                   gl_TessCoord.z * tessTexCoord[2];

    // Calculate displacement
    float displacement = calculateDisplacement(texCoord);

    // Apply displacement along normal
    vec3 displacedPosition = position + normalize(normal) * displacement;

    // Calculate displaced normal
    vec3 displacedNormal = calculateDisplacedNormal(texCoord, displacement);

    // Sample normal map for additional detail
    vec3 normalMap = texture(normalTexture, texCoord).rgb * 2.0 - 1.0;
    displacedNormal = normalize(displacedNormal + normalMap * 0.1);

    // Transform to world space
    vec4 worldPos = ubo.model * vec4(displacedPosition, 1.0);

    // Transform to clip space
    gl_Position = ubo.proj * ubo.view * worldPos;

    // Pass data to fragment shader
    fragColor = color;
    fragNormal = mat3(transpose(inverse(ubo.model))) * displacedNormal;
    fragPos = worldPos.xyz;
    fragTexCoord = texCoord;
}
