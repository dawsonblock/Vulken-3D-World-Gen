#version 450

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D inputTexture;
layout (set = 0, binding = 1) uniform sampler2D depthTexture;

layout (push_constant) uniform BlurParams {
    vec2 resolution;
    vec2 blurDirection;  // (1,0) for horizontal, (0,1) for vertical
    float depthThreshold;
    float normalThreshold;
    float blurRadius;
    float sharpness;
} blur;

const int KERNEL_SIZE = 9;
const float KERNEL_WEIGHTS[KERNEL_SIZE] = float[](
    0.05, 0.09, 0.12, 0.15, 0.16, 0.15, 0.12, 0.09, 0.05
);

// Depth-based weight calculation
float getDepthWeight(float centerDepth, float sampleDepth) {
    float depthDiff = abs(centerDepth - sampleDepth);
    return exp(-depthDiff * blur.sharpness / blur.depthThreshold);
}

// Normal reconstruction from depth (simplified)
vec3 reconstructNormal(sampler2D depth, vec2 uv) {
    vec2 texelSize = 1.0 / blur.resolution;
    
    float d0 = texture(depth, uv).x;
    float d1 = texture(depth, uv + vec2(texelSize.x, 0)).x;
    float d2 = texture(depth, uv + vec2(0, texelSize.y)).x;
    
    vec3 p0 = vec3(0, 0, d0);
    vec3 p1 = vec3(texelSize.x, 0, d1);
    vec3 p2 = vec3(0, texelSize.y, d2);
    
    vec3 normal = normalize(cross(p1 - p0, p2 - p0));
    return normal;
}

// Normal-based weight calculation  
float getNormalWeight(vec3 centerNormal, vec3 sampleNormal) {
    float normalSimilarity = dot(centerNormal, sampleNormal);
    return pow(max(0.0, normalSimilarity), blur.sharpness);
}

void main() {
    vec2 uv = inUV;
    vec2 texelSize = 1.0 / blur.resolution;
    
    // Center sample
    vec4 centerColor = texture(inputTexture, uv);
    float centerDepth = texture(depthTexture, uv).x;
    vec3 centerNormal = reconstructNormal(depthTexture, uv);
    
    vec4 blurredColor = vec4(0.0);
    float totalWeight = 0.0;
    
    // Bilateral blur kernel
    for (int i = 0; i < KERNEL_SIZE; i++) {
        int offset = i - KERNEL_SIZE / 2;
        vec2 sampleUV = uv + float(offset) * blur.blurDirection * texelSize * blur.blurRadius;
        
        // Clamp to screen bounds
        if (any(lessThan(sampleUV, vec2(0.0))) || any(greaterThan(sampleUV, vec2(1.0)))) {
            continue;
        }
        
        // Sample color and depth
        vec4 sampleColor = texture(inputTexture, sampleUV);
        float sampleDepth = texture(depthTexture, sampleUV).x;
        vec3 sampleNormal = reconstructNormal(depthTexture, sampleUV);
        
        // Calculate bilateral weights
        float spatialWeight = KERNEL_WEIGHTS[i];
        float depthWeight = getDepthWeight(centerDepth, sampleDepth);
        float normalWeight = getNormalWeight(centerNormal, sampleNormal);
        
        float finalWeight = spatialWeight * depthWeight * normalWeight;
        
        blurredColor += sampleColor * finalWeight;
        totalWeight += finalWeight;
    }
    
    // Normalize and output
    if (totalWeight > 0.0) {
        outColor = blurredColor / totalWeight;
    } else {
        outColor = centerColor;
    }
}