
#ifndef PBR_COMMON_GLSL
#define PBR_COMMON_GLSL

// Constants for better precision and performance
#define PI 3.14159265358979323846
#define INV_PI 0.31830988618379067154
#define EPSILON 1e-6

// Optimized utility functions
float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec3  saturate(vec3 v)  { return clamp(v, 0.0, 1.0); }

// Optimized GGX distribution with better numerical stability  
float D_GGX(float NoH, float a) { 
    float a2 = a * a; 
    float d = (NoH * NoH) * (a2 - 1.0) + 1.0; 
    return a2 / (PI * d * d + EPSILON); 
}

// Optimized Smith masking-shadowing function
float V_SmithGGX(float NoV, float NoL, float a) {
    float a2 = a * a;
    float gv = NoV + sqrt(a2 + (1.0 - a2) * NoV * NoV);
    float gl = NoL + sqrt(a2 + (1.0 - a2) * NoL * NoL);
    return 1.0 / (gv * gl + EPSILON);
}

// Optimized Schlick Fresnel approximation
vec3 F_Schlick(vec3 F0, float VoH) { 
    float f = pow(1.0 - VoH, 5.0);
    return F0 + (1.0 - F0) * f; 
}

// Lambert diffuse with correct normalization
vec3 lambert(vec3 albedo) { 
    return albedo * INV_PI; 
}

// Fast approximate pow for roughness remapping
float fast_pow5(float x) {
    float x2 = x * x;
    return x2 * x2 * x;
}

// Optimized Schlick Fresnel using fast pow
vec3 F_Schlick_fast(vec3 F0, float VoH) { 
    return F0 + (1.0 - F0) * fast_pow5(1.0 - VoH); 
}

#endif
