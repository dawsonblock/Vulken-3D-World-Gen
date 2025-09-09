#version 450

// Advanced post-processing effects fragment shader
layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

// Input textures
layout(binding = 0) uniform sampler2D sceneTexture;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler2D normalTexture;
layout(binding = 3) uniform sampler2D velocityTexture;

// Post-processing uniforms
layout(binding = 4) uniform PostProcessingUniforms {
    vec2 screenSize;
    float time;
    float exposure;
    float gamma;
    float contrast;
    float brightness;
    float saturation;
    float vignetteStrength;
    float chromaticAberration;
    float filmGrain;
    float scanlines;
    float pixelation;
    int effectType;
} ppUniforms;

// Temporal Anti-Aliasing (TAA)
vec3 calculateTAA(vec2 texCoord) {
    vec3 currentColor = texture(sceneTexture, texCoord).rgb;
    vec3 previousColor = texture(sceneTexture, texCoord - texture(velocityTexture, texCoord).xy).rgb;

    // Blend current and previous frame
    float blendFactor = 0.1;
    return mix(currentColor, previousColor, blendFactor);
}

// Screen-Space Reflections (SSR)
vec3 calculateSSR(vec2 texCoord) {
    vec3 normal = texture(normalTexture, texCoord).rgb * 2.0 - 1.0;
    float depth = texture(depthTexture, texCoord).r;

    // Calculate reflection direction
    vec3 viewDir = normalize(vec3(texCoord * 2.0 - 1.0, depth));
    vec3 reflectionDir = reflect(viewDir, normal);

    // Ray march for reflection
    vec3 rayStart = vec3(texCoord, depth);
    vec3 rayDir = reflectionDir;
    float rayStep = 0.01;
    float maxDistance = 1.0;

    vec3 reflectionColor = vec3(0.0);
    float reflectionStrength = 0.0;

    for(int i = 0; i < 64; ++i) {
        vec3 rayPos = rayStart + rayDir * rayStep * float(i);

        if(rayPos.x < 0.0 || rayPos.x > 1.0 || rayPos.y < 0.0 || rayPos.y > 1.0) {
            break;
        }

        float rayDepth = texture(depthTexture, rayPos.xy).r;

        if(rayPos.z > rayDepth) {
            reflectionColor = texture(sceneTexture, rayPos.xy).rgb;
            reflectionStrength = 1.0 - (float(i) / 64.0);
            break;
        }
    }

    return reflectionColor * reflectionStrength;
}

// Depth of Field
vec3 calculateDepthOfField(vec2 texCoord) {
    float depth = texture(depthTexture, texCoord).r;
    float focusDistance = 0.5;
    float blurRadius = abs(depth - focusDistance) * 10.0;

    vec3 color = vec3(0.0);
    float totalWeight = 0.0;

    int sampleCount = 16;
    for(int i = 0; i < sampleCount; ++i) {
        float angle = (float(i) / float(sampleCount)) * 2.0 * 3.14159;
        vec2 offset = vec2(cos(angle), sin(angle)) * blurRadius;
        vec2 sampleCoord = texCoord + offset / ppUniforms.screenSize;

        if(sampleCoord.x >= 0.0 && sampleCoord.x <= 1.0 &&
           sampleCoord.y >= 0.0 && sampleCoord.y <= 1.0) {
            float weight = 1.0 - length(offset) / blurRadius;
            color += texture(sceneTexture, sampleCoord).rgb * weight;
            totalWeight += weight;
        }
    }

    return totalWeight > 0.0 ? color / totalWeight : texture(sceneTexture, texCoord).rgb;
}

// Motion Blur
vec3 calculateMotionBlur(vec2 texCoord) {
    vec2 velocity = texture(velocityTexture, texCoord).xy;
    float velocityLength = length(velocity);

    if(velocityLength < 0.001) {
        return texture(sceneTexture, texCoord).rgb;
    }

    vec3 color = vec3(0.0);
    int sampleCount = 16;

    for(int i = 0; i < sampleCount; ++i) {
        float t = (float(i) / float(sampleCount - 1)) - 0.5;
        vec2 sampleCoord = texCoord + velocity * t;

        if(sampleCoord.x >= 0.0 && sampleCoord.x <= 1.0 &&
           sampleCoord.y >= 0.0 && sampleCoord.y <= 1.0) {
            color += texture(sceneTexture, sampleCoord).rgb;
        }
    }

    return color / float(sampleCount);
}

// Color Grading
vec3 applyColorGrading(vec3 color) {
    // Brightness
    color += ppUniforms.brightness;

    // Contrast
    color = (color - 0.5) * ppUniforms.contrast + 0.5;

    // Saturation
    float luminance = dot(color, vec3(0.299, 0.587, 0.114));
    color = mix(vec3(luminance), color, ppUniforms.saturation);

    return color;
}

// Vignette
vec3 applyVignette(vec3 color, vec2 texCoord) {
    vec2 center = vec2(0.5);
    float distance = length(texCoord - center);
    float vignette = 1.0 - smoothstep(0.3, 0.8, distance) * ppUniforms.vignetteStrength;

    return color * vignette;
}

// Chromatic Aberration
vec3 applyChromaticAberration(vec3 color, vec2 texCoord) {
    vec2 center = vec2(0.5);
    vec2 offset = (texCoord - center) * ppUniforms.chromaticAberration;

    float r = texture(sceneTexture, texCoord + offset).r;
    float g = texture(sceneTexture, texCoord).g;
    float b = texture(sceneTexture, texCoord - offset).b;

    return vec3(r, g, b);
}

// Film Grain
vec3 applyFilmGrain(vec3 color, vec2 texCoord) {
    float noise = fract(sin(dot(texCoord + ppUniforms.time, vec2(12.9898, 78.233))) * 43758.5453);
    noise = (noise - 0.5) * ppUniforms.filmGrain;

    return color + noise;
}

// Scanlines
vec3 applyScanlines(vec3 color, vec2 texCoord) {
    float scanline = sin(texCoord.y * ppUniforms.screenSize.y * 3.14159) * 0.5 + 0.5;
    scanline = pow(scanline, 2.0) * ppUniforms.scanlines;

    return color * (1.0 - scanline);
}

// Pixelation
vec3 applyPixelation(vec3 color, vec2 texCoord) {
    if(ppUniforms.pixelation <= 1.0) {
        return color;
    }

    vec2 pixelSize = 1.0 / ppUniforms.screenSize * ppUniforms.pixelation;
    vec2 pixelCoord = floor(texCoord / pixelSize) * pixelSize;

    return texture(sceneTexture, pixelCoord).rgb;
}

// Tone Mapping
vec3 applyToneMapping(vec3 color) {
    // Exposure
    color *= ppUniforms.exposure;

    // Reinhard tone mapping
    color = color / (color + vec3(1.0));

    // Gamma correction
    color = pow(color, vec3(1.0 / ppUniforms.gamma));

    return color;
}

void main() {
    vec3 color = texture(sceneTexture, fragTexCoord).rgb;

    // Apply post-processing effects based on type
    if(ppUniforms.effectType == 0) {
        // Temporal Anti-Aliasing
        color = calculateTAA(fragTexCoord);
    } else if(ppUniforms.effectType == 1) {
        // Screen-Space Reflections
        color += calculateSSR(fragTexCoord) * 0.3;
    } else if(ppUniforms.effectType == 2) {
        // Depth of Field
        color = calculateDepthOfField(fragTexCoord);
    } else if(ppUniforms.effectType == 3) {
        // Motion Blur
        color = calculateMotionBlur(fragTexCoord);
    }

    // Apply color grading
    color = applyColorGrading(color);

    // Apply visual effects
    color = applyVignette(color, fragTexCoord);
    color = applyChromaticAberration(color, fragTexCoord);
    color = applyFilmGrain(color, fragTexCoord);
    color = applyScanlines(color, fragTexCoord);
    color = applyPixelation(color, fragTexCoord);

    // Apply tone mapping
    color = applyToneMapping(color);

    outColor = vec4(color, 1.0);
}
