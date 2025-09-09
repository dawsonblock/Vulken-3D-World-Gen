#version 460
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec2 vUv;
layout(location = 0) out float outAO;

layout(set = 0, binding = 0) uniform sampler2D uDepth;
layout(set = 0, binding = 1) uniform sampler2D uNormal;
layout(set = 0, binding = 2) uniform sampler2D uNoise;  // 4x4 noise texture

layout(set = 0, binding = 3) uniform SSAOData {
    mat4 projection;
    mat4 view;
    vec3 samples[64];  // Sample kernel
    float radius;
    float bias;
    float strength;
    uint sampleCount;
} uSSAO;

layout(push_constant) uniform SSAOConstants {
    vec2 screenSize;
    vec2 invScreenSize;
    vec2 noiseScale;
} uConst;

// Reconstruct world position from depth
vec3 reconstructWorldPos(vec2 uv, float depth) {
    vec4 clipSpacePos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewSpacePos = inverse(uSSAO.projection) * clipSpacePos;
    viewSpacePos /= viewSpacePos.w;
    
    vec4 worldSpacePos = inverse(uSSAO.view) * viewSpacePos;
    return worldSpacePos.xyz;
}

void main() {
    vec2 uv = vUv;
    
    // Sample depth and normal
    float depth = texture(uDepth, uv).r;
    vec3 normal = normalize(texture(uNormal, uv).xyz * 2.0 - 1.0); // Unpack normal
    
    // Early exit for sky
    if (depth >= 1.0) {
        outAO = 1.0;
        return;
    }
    
    // Reconstruct world position
    vec3 worldPos = reconstructWorldPos(uv, depth);
    
    // Get noise vector for sample rotation
    vec2 noiseUV = uv * uConst.noiseScale;
    vec3 randomVec = normalize(texture(uNoise, noiseUV).xyz * 2.0 - 1.0);
    
    // Create TBN matrix for sample space transformation
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    
    // SSAO calculation
    float occlusion = 0.0;
    uint validSamples = 0;
    
    for (uint i = 0; i < uSSAO.sampleCount && i < 64; i++) {
        // Sample position in world space
        vec3 samplePos = worldPos + TBN * uSSAO.samples[i] * uSSAO.radius;
        
        // Project sample to screen space
        vec4 offset = uSSAO.projection * uSSAO.view * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5; // Convert to UV coordinates
        
        // Check bounds
        if (any(lessThan(offset.xy, vec2(0.0))) || any(greaterThan(offset.xy, vec2(1.0)))) {
            continue;
        }
        
        // Sample depth at projected position
        float sampleDepth = texture(uDepth, offset.xy).r;
        vec3 sampleWorldPos = reconstructWorldPos(offset.xy, sampleDepth);
        
        // Range check  
        float rangeCheck = smoothstep(0.0, 1.0, uSSAO.radius / abs(worldPos.z - sampleWorldPos.z));
        
        // Occlusion test
        float occlusionTest = (sampleWorldPos.z >= samplePos.z + uSSAO.bias) ? 1.0 : 0.0;
        
        occlusion += occlusionTest * rangeCheck;
        validSamples++;
    }
    
    // Normalize and apply strength
    if (validSamples > 0) {
        occlusion = 1.0 - (occlusion / float(validSamples));
        occlusion = pow(occlusion, uSSAO.strength);
    } else {
        occlusion = 1.0;
    }
    
    outAO = occlusion;
}