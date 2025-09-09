
#version 450
#ifndef PCSS_GLSL
#define PCSS_GLSL

// Optimized PCSS implementation with better sampling patterns
float pcss_visibility(sampler2DArray sm, vec3 uvw, float texel, float searchPx, float minPx, float maxPx, float bias)
{
    float receiver = uvw.z - bias;
    
    // Blocker search with optimized sampling pattern
    float r = searchPx * texel;
    float avg = 0.0; 
    int blockers = 0;
    const int K = 16;
    
    // Use golden angle for better distribution
    const float golden_angle = 2.39996323; // 2*PI * golden ratio
    for(int i = 0; i < K; i++) {
        float angle = golden_angle * float(i);
        float radius = r * sqrt(float(i + 1) / float(K)); // Spiral distribution
        vec2 offset = vec2(cos(angle), sin(angle)) * radius;
        
        float d = texture(sm, vec3(uvw.xy + offset, uvw.z)).r;
        if(d < receiver) { 
            avg += d; 
            blockers++; 
        }
    }
    
    if(blockers == 0) return 1.0;
    avg /= float(blockers);
    
    // Improved penumbra estimation
    float pen = clamp((receiver - avg) * 200.0, minPx, maxPx);
    
    // PCF with variable radius using Poisson sampling
    const int S = 24;
    int lit = 0;
    
    // Poisson disk samples for better quality
    const vec2 poisson[24] = vec2[](
        vec2(-0.326212, -0.40581), vec2(-0.840144, -0.07358),
        vec2(-0.695914, 0.457137), vec2(-0.203345, 0.620716),
        vec2(0.96234, -0.194983), vec2(0.473434, -0.480026),
        vec2(0.519456, 0.767022), vec2(0.185461, -0.893124),
        vec2(0.507431, 0.064425), vec2(0.89642, 0.412458),
        vec2(-0.32194, -0.932615), vec2(-0.791559, -0.59771),
        vec2(-0.326212, -0.40581), vec2(-0.840144, -0.07358),
        vec2(-0.695914, 0.457137), vec2(-0.203345, 0.620716),
        vec2(0.96234, -0.194983), vec2(0.473434, -0.480026),
        vec2(0.519456, 0.767022), vec2(0.185461, -0.893124),
        vec2(0.507431, 0.064425), vec2(0.89642, 0.412458),
        vec2(-0.32194, -0.932615), vec2(-0.791559, -0.59771)
    );
    
    for(int i = 0; i < S; i++) {
        vec2 offset = poisson[i] * pen * texel;
        float d = texture(sm, vec3(uvw.xy + offset, uvw.z)).r;
        lit += int(d + 1e-4 >= receiver);
    }
    
    return float(lit) / float(S);
}

#endif
