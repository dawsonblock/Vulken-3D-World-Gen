#version 450

// Screen-Space Ambient Occlusion fragment shader
layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out float outOcclusion;

// Input textures
layout(binding = 0) uniform sampler2D positionTexture;
layout(binding = 1) uniform sampler2D normalTexture;
layout(binding = 2) uniform sampler2D noiseTexture;

// SSAO uniforms
layout(binding = 3) uniform SSAOUniforms {
    mat4 projection;
    vec3 samples[64];
    float radius;
    float bias;
    int kernelSize;
    vec2 screenSize;  // Screen dimensions for resolution-independent noise scale
} ssao;

void main() {
    // Random rotation vectors - now resolution-independent
    vec2 noiseScale = ssao.screenSize / 4.0;
    // Get position and normal from textures
    vec3 fragPos = texture(positionTexture, fragTexCoord).xyz;
    vec3 normal = normalize(texture(normalTexture, fragTexCoord).xyz);

    // Get random rotation vector
    vec3 randomVec = normalize(texture(noiseTexture, fragTexCoord * noiseScale).xyz);

    // Create TBN matrix
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    // Calculate occlusion
    float occlusion = 0.0;
    for(int i = 0; i < ssao.kernelSize; ++i) {
        // Get sample position
        vec3 samplePos = TBN * ssao.samples[i];
        samplePos = fragPos + samplePos * ssao.radius;

        // Project sample position to screen space
        vec4 offset = vec4(samplePos, 1.0);
        offset = ssao.projection * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        // Sample depth
        float sampleDepth = texture(positionTexture, offset.xy).z;

        // Range check and accumulate
        float rangeCheck = smoothstep(0.0, 1.0, ssao.radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + ssao.bias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(ssao.kernelSize));
    outOcclusion = occlusion;
}
