#version 450

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D currentFrame;
layout (set = 0, binding = 1) uniform sampler2D historyFrame;
layout (set = 0, binding = 2) uniform sampler2D motionVectors;
layout (set = 0, binding = 3) uniform sampler2D depthBuffer;

layout (push_constant) uniform TAAParams {
    vec2 resolution;
    vec2 jitterOffset;
    float blendFactor;
    float varianceClipping; 
    float velocityWeight;
    uint frameIndex;
} taa;

// TAA neighborhood sampling
vec3 sampleNeighborhood(sampler2D tex, vec2 uv) {
    vec2 texelSize = 1.0 / taa.resolution;
    
    vec3 samples[9];
    samples[0] = texture(tex, uv + vec2(-1, -1) * texelSize).rgb;
    samples[1] = texture(tex, uv + vec2( 0, -1) * texelSize).rgb;
    samples[2] = texture(tex, uv + vec2( 1, -1) * texelSize).rgb;
    samples[3] = texture(tex, uv + vec2(-1,  0) * texelSize).rgb;
    samples[4] = texture(tex, uv).rgb;                          // center
    samples[5] = texture(tex, uv + vec2( 1,  0) * texelSize).rgb;
    samples[6] = texture(tex, uv + vec2(-1,  1) * texelSize).rgb;
    samples[7] = texture(tex, uv + vec2( 0,  1) * texelSize).rgb;
    samples[8] = texture(tex, uv + vec2( 1,  1) * texelSize).rgb;
    
    return samples[4]; // Center sample (can be modified for more complex filtering)
}

// Variance-based clipping to reduce ghosting
vec3 clipHistory(vec3 history, vec3 current, vec3 minNeighbor, vec3 maxNeighbor) {
    vec3 center = current;
    vec3 extent = (maxNeighbor - minNeighbor) * taa.varianceClipping;
    
    vec3 clampedHistory = clamp(history, center - extent, center + extent);
    return mix(history, clampedHistory, 0.8);
}

// Calculate neighborhood min/max for variance clipping
void calculateNeighborhoodMinMax(sampler2D tex, vec2 uv, out vec3 minColor, out vec3 maxColor) {
    vec2 texelSize = 1.0 / taa.resolution;
    
    vec3 samples[9];
    samples[0] = texture(tex, uv + vec2(-1, -1) * texelSize).rgb;
    samples[1] = texture(tex, uv + vec2( 0, -1) * texelSize).rgb;
    samples[2] = texture(tex, uv + vec2( 1, -1) * texelSize).rgb;
    samples[3] = texture(tex, uv + vec2(-1,  0) * texelSize).rgb;
    samples[4] = texture(tex, uv).rgb;
    samples[5] = texture(tex, uv + vec2( 1,  0) * texelSize).rgb;
    samples[6] = texture(tex, uv + vec2(-1,  1) * texelSize).rgb;
    samples[7] = texture(tex, uv + vec2( 0,  1) * texelSize).rgb;
    samples[8] = texture(tex, uv + vec2( 1,  1) * texelSize).rgb;
    
    minColor = samples[0];
    maxColor = samples[0];
    
    for (int i = 1; i < 9; i++) {
        minColor = min(minColor, samples[i]);
        maxColor = max(maxColor, samples[i]);
    }
}

void main() {
    vec2 uv = inUV;
    
    // Sample current frame
    vec3 currentColor = texture(currentFrame, uv).rgb;
    
    // Read motion vector and reproject
    vec2 motionVector = texture(motionVectors, uv).xy;
    vec2 historyUV = uv - motionVector;
    
    // Check if history sample is valid (within screen bounds)
    bool validHistory = all(greaterThanEqual(historyUV, vec2(0.0))) && 
                       all(lessThanEqual(historyUV, vec2(1.0)));
    
    if (!validHistory) {
        // No valid history, output current frame
        outColor = vec4(currentColor, 1.0);
        return;
    }
    
    // Sample history with bilinear filtering
    vec3 historyColor = texture(historyFrame, historyUV).rgb;
    
    // Calculate neighborhood statistics for variance clipping
    vec3 minNeighbor, maxNeighbor;
    calculateNeighborhoodMinMax(currentFrame, uv, minNeighbor, maxNeighbor);
    
    // Clip history to reduce ghosting artifacts
    historyColor = clipHistory(historyColor, currentColor, minNeighbor, maxNeighbor);
    
    // Calculate adaptive blend factor based on motion
    float motionLength = length(motionVector * taa.resolution);
    float motionWeight = clamp(motionLength * taa.velocityWeight, 0.0, 1.0);
    float adaptiveBlend = mix(taa.blendFactor, 0.8, motionWeight);
    
    // Temporal accumulation
    vec3 finalColor = mix(historyColor, currentColor, adaptiveBlend);
    
    outColor = vec4(finalColor, 1.0);
}