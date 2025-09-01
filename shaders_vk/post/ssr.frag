#version 460
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outReflection;

layout(set = 0, binding = 0) uniform sampler2D uColor;
layout(set = 0, binding = 1) uniform sampler2D uDepth;
layout(set = 0, binding = 2) uniform sampler2D uNormal;
layout(set = 0, binding = 3) uniform sampler2D uRoughness;
layout(set = 0, binding = 4) uniform sampler2D uDepthMipchain;

layout(set = 0, binding = 5) uniform SSRData {
    mat4 projection;
    mat4 view;
    mat4 invProjection;
    mat4 invView;
    vec3 cameraPos;
    float maxDistance;
    float thickness;
    float maxRoughness;
    uint maxSteps;
    bool roughnessAware;
} uSSR;

layout(push_constant) uniform SSRConstants {
    vec2 screenSize;
    vec2 invScreenSize;
    float stepSize;
    uint mipLevels;
} uConst;

// Reconstruct world position from depth
vec3 reconstructWorldPos(vec2 uv, float depth) {
    vec4 clipSpacePos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewSpacePos = uSSR.invProjection * clipSpacePos;
    viewSpacePos /= viewSpacePos.w;
    
    vec4 worldSpacePos = uSSR.invView * viewSpacePos;
    return worldSpacePos.xyz;
}

// Project world position to screen space
vec2 projectToScreen(vec3 worldPos) {
    vec4 clipPos = uSSR.projection * uSSR.view * vec4(worldPos, 1.0);
    clipPos.xyz /= clipPos.w;
    return clipPos.xy * 0.5 + 0.5;
}

bool traceRay(vec3 rayOrigin, vec3 rayDirection, out vec2 hitUV, out float hitDepth) {
    vec3 rayPos = rayOrigin;
    
    for (uint step = 0; step < uSSR.maxSteps; step++) {
        // Advance ray
        rayPos += rayDirection * uConst.stepSize;
        
        // Check distance limit
        if (distance(rayPos, rayOrigin) > uSSR.maxDistance) {
            return false;
        }
        
        // Project to screen space
        vec2 screenUV = projectToScreen(rayPos);
        
        // Check screen bounds
        if (any(lessThan(screenUV, vec2(0.0))) || any(greaterThan(screenUV, vec2(1.0)))) {
            return false;
        }
        
        // Sample depth at current position
        float sceneDepth = texture(uDepth, screenUV).r;
        vec3 sceneWorldPos = reconstructWorldPos(screenUV, sceneDepth);
        
        // Check for intersection
        float rayDepth = distance(rayPos, uSSR.cameraPos);
        float surfaceDepth = distance(sceneWorldPos, uSSR.cameraPos);
        
        if (rayDepth > surfaceDepth && rayDepth - surfaceDepth < uSSR.thickness) {
            // Hit found
            hitUV = screenUV;
            hitDepth = sceneDepth;
            return true;
        }
    }
    
    return false; // No hit found
}

void main() {
    vec2 uv = vUv;
    
    // Sample G-buffer
    float depth = texture(uDepth, uv).r;
    vec3 normal = normalize(texture(uNormal, uv).xyz * 2.0 - 1.0);
    float roughness = texture(uRoughness, uv).r;
    
    // Early exit for sky or high roughness
    if (depth >= 1.0) {
        outReflection = vec4(0.0);
        return;
    }
    
    if (uSSR.roughnessAware && roughness > uSSR.maxRoughness) {
        outReflection = vec4(0.0);
        return;
    }
    
    // Reconstruct world position
    vec3 worldPos = reconstructWorldPos(uv, depth);
    vec3 viewDir = normalize(worldPos - uSSR.cameraPos);
    
    // Calculate reflection direction
    vec3 reflectionDir = reflect(viewDir, normal);
    
    // Trace reflection ray
    vec2 hitUV;
    float hitDepth;
    
    if (traceRay(worldPos, reflectionDir, hitUV, hitDepth)) {
        // Sample reflection color
        vec3 reflectionColor = texture(uColor, hitUV).rgb;
        
        // Fade based on roughness
        float reflectionStrength = 1.0;
        if (uSSR.roughnessAware) {
            reflectionStrength = 1.0 - smoothstep(0.0, uSSR.maxRoughness, roughness);
        }
        
        // Fade based on distance and angle
        float distanceFade = 1.0 - smoothstep(uSSR.maxDistance * 0.5, uSSR.maxDistance, 
                                             distance(worldPos, reconstructWorldPos(hitUV, hitDepth)));
        float angleFade = max(0.0, dot(-viewDir, normal));
        
        reflectionStrength *= distanceFade * angleFade;
        
        outReflection = vec4(reflectionColor, reflectionStrength);
    } else {
        // No reflection found
        outReflection = vec4(0.0);
    }
}