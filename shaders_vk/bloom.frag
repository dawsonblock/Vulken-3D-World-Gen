#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D inputTexture;
layout(binding = 1) uniform sampler2D bloomTexture;

layout(push_constant) uniform PushConstants {
    float exposure;
    float bloomStrength;
    int toneMapOperator;
    float gamma;
} pc;

// Tone mapping functions
vec3 reinhardToneMapping(vec3 color) {
    return color / (color + vec3(1.0));
}

vec3 acesToneMapping(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 uncharted2ToneMapping(vec3 color) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    const float W = 11.2;
    
    // Apply tone mapping curve
    vec3 numerator = ((color * (A * color + C * B) + D * E));
    vec3 denominator = (color * (A * color + B) + D * F) - E;
    vec3 toneMapped = numerator / denominator;
    
    // Fixed scaling step: properly compute 1.0 / (1.0 + W/W) = 1.0 / (1.0 + 1.0) = 0.5
    vec3 whiteScale = vec3(1.0) / (vec3(1.0) + vec3(W) * (1.0 / W));
    
    // Apply white point scaling correctly
    return toneMapped * whiteScale;
}

void main() {
    vec3 hdrColor = texture(inputTexture, fragTexCoord).rgb;
    vec3 bloomColor = texture(bloomTexture, fragTexCoord).rgb;
    
    // Apply bloom
    hdrColor += bloomColor * pc.bloomStrength;
    
    // Apply exposure
    hdrColor *= pc.exposure;
    
    // Apply tone mapping
    vec3 mapped;
    if (pc.toneMapOperator == 0) {
        mapped = reinhardToneMapping(hdrColor);
    } else if (pc.toneMapOperator == 1) {
        mapped = acesToneMapping(hdrColor);
    } else {
        mapped = uncharted2ToneMapping(hdrColor);
    }
    
    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / pc.gamma));
    
    outColor = vec4(mapped, 1.0);
}