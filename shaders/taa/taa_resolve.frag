#version 460
#extension GL_GOOGLE_include_directive : enable
#include "../common/weather_ubo.glsl"

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uCurrentColor;
layout(set = 0, binding = 1) uniform sampler2D uHistoryColor;
layout(set = 0, binding = 2) uniform sampler2D uMotionVectors;
layout(set = 0, binding = 3) uniform sampler2D uDepth;

layout(push_constant) uniform TAAConstants {
    vec2 screenSize;
    vec2 invScreenSize;
    float feedbackMin;
    float feedbackMax;
    float motionThreshold;
    bool enableVarianceClipping;
} uTAA;

// TAA utilities
vec3 rgb2ycocg(vec3 rgb) {
    return vec3(
        rgb.r * 0.25 + rgb.g * 0.5 + rgb.b * 0.25,  // Y
        rgb.r * 0.5 - rgb.b * 0.5,                   // Co
        rgb.r * -0.25 + rgb.g * 0.5 + rgb.b * -0.25  // Cg
    );
}

vec3 ycocg2rgb(vec3 ycocg) {
    return vec3(
        ycocg.x + ycocg.y - ycocg.z,    // R
        ycocg.x + ycocg.z,              // G  
        ycocg.x - ycocg.y - ycocg.z     // B
    );
}

vec3 clipToAABB(vec3 color, vec3 aabbMin, vec3 aabbMax) {
    vec3 center = 0.5 * (aabbMax + aabbMin);
    vec3 extents = 0.5 * (aabbMax - aabbMin);
    
    vec3 offset = color - center;
    vec3 ts = abs(extents) / max(abs(offset), vec3(1e-8));
    float t = min(min(ts.x, ts.y), ts.z);
    
    return center + offset * min(t, 1.0);
}

void main() {
    vec2 uv = vUv;
    
    // Sample current frame
    vec3 currentColor = texture(uCurrentColor, uv).rgb;
    
    // Sample motion vector
    vec2 motion = texture(uMotionVectors, uv).xy;
    vec2 historyUV = uv - motion;
    
    // Check if history UV is valid
    if (any(lessThan(historyUV, vec2(0.0))) || any(greaterThan(historyUV, vec2(1.0)))) {
        // No valid history - use current color
        outColor = vec4(currentColor, 1.0);
        return;
    }
    
    // Sample history
    vec3 historyColor = texture(uHistoryColor, historyUV).rgb;
    
    // Neighborhood clamping to reduce ghosting
    vec3 minColor = currentColor;
    vec3 maxColor = currentColor;
    
    // Sample 3x3 neighborhood around current pixel
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 offset = vec2(x, y) * uTAA.invScreenSize;
            vec3 neighborColor = texture(uCurrentColor, uv + offset).rgb;
            minColor = min(minColor, neighborColor);
            maxColor = max(maxColor, neighborColor);
        }
    }
    
    // Clip history to neighborhood AABB (YCoCg space for better results)
    if (uTAA.enableVarianceClipping) {
        vec3 currentYCoCg = rgb2ycocg(currentColor);
        vec3 historyYCoCg = rgb2ycocg(historyColor);
        vec3 minYCoCg = rgb2ycocg(minColor);
        vec3 maxYCoCg = rgb2ycocg(maxColor);
        
        historyYCoCg = clipToAABB(historyYCoCg, minYCoCg, maxYCoCg);
        historyColor = ycocg2rgb(historyYCoCg);
    } else {
        // Simple clamping
        historyColor = clamp(historyColor, minColor, maxColor);
    }
    
    // Adaptive feedback based on motion
    float motionMagnitude = length(motion);
    float feedback = mix(uTAA.feedbackMax, uTAA.feedbackMin, 
                        smoothstep(0.0, uTAA.motionThreshold, motionMagnitude));
    
    // Temporal resolve
    vec3 resolvedColor = mix(currentColor, historyColor, feedback);
    
    outColor = vec4(resolvedColor, 1.0);
}