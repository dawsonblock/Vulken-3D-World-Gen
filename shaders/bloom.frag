#version 450

// Bloom effect fragment shader
layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

// Input textures
layout(binding = 0) uniform sampler2D sceneTexture;
layout(binding = 1) uniform sampler2D brightTexture;

// Bloom uniforms
layout(binding = 2) uniform BloomUniforms {
    float exposure;
    float bloomStrength;
    float threshold;
    int toneMapping;
} bloom;

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

    color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
    return color * (1.0 / (color * (1.0 / W) + 1.0));
}

void main() {
    // Sample textures
    vec3 sceneColor = texture(sceneTexture, fragTexCoord).rgb;
    vec3 brightColor = texture(brightTexture, fragTexCoord).rgb;

    // Apply exposure
    sceneColor *= bloom.exposure;
    brightColor *= bloom.exposure;

    // Combine scene and bloom
    vec3 result = sceneColor + brightColor * bloom.bloomStrength;

    // Apply tone mapping
    if(bloom.toneMapping == 0) {
        result = reinhardToneMapping(result);
    } else if(bloom.toneMapping == 1) {
        result = acesToneMapping(result);
    } else if(bloom.toneMapping == 2) {
        result = uncharted2ToneMapping(result);
    }

    // Gamma correction
    result = pow(result, vec3(1.0/2.2));

    outColor = vec4(result, 1.0);
}
