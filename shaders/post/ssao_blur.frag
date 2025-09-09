#version 460
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec2 vUv;
layout(location = 0) out float outAO;

layout(set = 0, binding = 0) uniform sampler2D uSSAO;
layout(set = 0, binding = 1) uniform sampler2D uDepth;

layout(push_constant) uniform BlurConstants {
    vec2 screenSize;
    vec2 invScreenSize;
    vec2 blurDirection;  // (1,0) for horizontal, (0,1) for vertical
    float depthThreshold;
    uint blurRadius;
} uBlur;

void main() {
    vec2 uv = vUv;
    
    float centerDepth = texture(uDepth, uv).r;
    float centerAO = texture(uSSAO, uv).r;
    
    // Early exit for sky
    if (centerDepth >= 1.0) {
        outAO = 1.0;
        return;
    }
    
    // Bilateral blur to preserve edges
    float totalWeight = 1.0;
    float blurredAO = centerAO;
    
    for (uint i = 1; i <= uBlur.blurRadius; i++) {
        vec2 offset = uBlur.blurDirection * uBlur.invScreenSize * float(i);
        
        // Sample both directions
        for (int dir = -1; dir <= 1; dir += 2) {
            vec2 sampleUV = uv + offset * float(dir);
            
            // Check bounds
            if (any(lessThan(sampleUV, vec2(0.0))) || any(greaterThan(sampleUV, vec2(1.0)))) {
                continue;
            }
            
            float sampleDepth = texture(uDepth, sampleUV).r;
            float sampleAO = texture(uSSAO, sampleUV).r;
            
            // Depth-aware weight to preserve edges
            float depthDiff = abs(centerDepth - sampleDepth);
            float weight = exp(-depthDiff / uBlur.depthThreshold);
            
            // Distance-based weight
            float distanceWeight = 1.0 / (1.0 + float(i));
            weight *= distanceWeight;
            
            blurredAO += sampleAO * weight;
            totalWeight += weight;
        }
    }
    
    outAO = blurredAO / totalWeight;
}