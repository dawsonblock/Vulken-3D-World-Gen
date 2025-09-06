#version 450

const float PI = 3.141592653589793;

// Volumetric lighting and fog fragment shader
layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

// Input textures
layout(binding = 0) uniform sampler2D sceneTexture;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler2D noiseTexture;

// Volumetric lighting uniforms
layout(binding = 3) uniform VolumetricUniforms {
    vec3 cameraPos;
    vec3 lightPos;
    vec3 lightColor;
    float lightIntensity;
    float fogDensity;
    float fogStart;
    float fogEnd;
    float scatteringCoeff;
    float absorptionCoeff;
    float phaseFunction;
    int sampleCount;
    float stepSize;
    float noiseScale;
    float time;
} volUniforms;

// Fog types
const int FOG_LINEAR = 0;
const int FOG_EXPONENTIAL = 1;
const int FOG_EXPONENTIAL_SQUARED = 2;

// Volumetric lighting calculation
vec3 calculateVolumetricLighting(vec3 rayStart, vec3 rayEnd, vec3 lightPos, vec3 lightColor) {
    vec3 rayDir = normalize(rayEnd - rayStart);
    float rayLength = length(rayEnd - rayStart);

    vec3 lightDir = normalize(lightPos - rayStart);

    vec3 volumetricColor = vec3(0.0);
    float stepSize = rayLength / float(volUniforms.sampleCount);

    for(int i = 0; i < volUniforms.sampleCount; ++i) {
        float t = (float(i) + 0.5) * stepSize;
        vec3 samplePos = rayStart + rayDir * t;

        // Calculate per-sample distance to light
        float lightDistance = length(lightPos - samplePos);

        // Calculate light attenuation
        float lightAttenuation = 1.0 / (1.0 + 0.09 * lightDistance + 0.032 * lightDistance * lightDistance);

        // Calculate scattering
        float scattering = volUniforms.scatteringCoeff * lightAttenuation;

        // Calculate absorption
        float absorption = volUniforms.absorptionCoeff * t;

        // Calculate phase function (Henyey-Greenstein) with proper normalization
        float cosTheta = dot(rayDir, lightDir);
        float phase = (1.0 - volUniforms.phaseFunction * volUniforms.phaseFunction) /
                     pow(1.0 + volUniforms.phaseFunction * volUniforms.phaseFunction -
                         2.0 * volUniforms.phaseFunction * cosTheta, 1.5);
        phase = phase / (4.0 * PI);  // Normalize by 1/(4π)

        // Sample noise for volumetric variation
        vec3 noiseSample = texture(noiseTexture, samplePos.xz * volUniforms.noiseScale + volUniforms.time).rgb;
        float noiseFactor = (noiseSample.r + noiseSample.g + noiseSample.b) / 3.0;

        // Calculate volumetric contribution
        vec3 volumetricContribution = lightColor * scattering * phase * noiseFactor * stepSize;
        volumetricContribution *= exp(-absorption);

        volumetricColor += volumetricContribution;
    }

    return volumetricColor;
}

// Fog calculation
float calculateFog(vec3 worldPos, vec3 cameraPos, int fogType) {
    float distance = length(worldPos - cameraPos);

    if(fogType == FOG_LINEAR) {
        return clamp((volUniforms.fogEnd - distance) / (volUniforms.fogEnd - volUniforms.fogStart), 0.0, 1.0);
    } else if(fogType == FOG_EXPONENTIAL) {
        return exp(-volUniforms.fogDensity * distance);
    } else if(fogType == FOG_EXPONENTIAL_SQUARED) {
        return exp(-volUniforms.fogDensity * distance * distance);
    }

    return 1.0;
}

// God rays calculation
vec3 calculateGodRays(vec3 rayStart, vec3 rayEnd, vec3 lightPos, vec3 lightColor) {
    vec3 rayDir = normalize(rayEnd - rayStart);
    float rayLength = length(rayEnd - rayStart);

    vec3 lightDir = normalize(lightPos - rayStart);
    float lightDistance = length(lightPos - rayStart);

    vec3 godRayColor = vec3(0.0);
    float stepSize = rayLength / float(volUniforms.sampleCount);

    for(int i = 0; i < volUniforms.sampleCount; ++i) {
        float t = (float(i) + 0.5) * stepSize;
        vec3 samplePos = rayStart + rayDir * t;

        // Calculate light attenuation
        float lightAttenuation = 1.0 / (1.0 + 0.09 * lightDistance + 0.032 * lightDistance * lightDistance);

        // Calculate scattering for god rays
        float scattering = volUniforms.scatteringCoeff * lightAttenuation;

        // Calculate phase function for god rays
        float cosTheta = dot(rayDir, lightDir);
        float phase = (1.0 - volUniforms.phaseFunction * volUniforms.phaseFunction) /
                     pow(1.0 + volUniforms.phaseFunction * volUniforms.phaseFunction -
                         2.0 * volUniforms.phaseFunction * cosTheta, 1.5);

        // Sample noise for volumetric variation
        vec3 noiseSample = texture(noiseTexture, samplePos.xz * volUniforms.noiseScale + volUniforms.time).rgb;
        float noiseFactor = (noiseSample.r + noiseSample.g + noiseSample.b) / 3.0;

        // Calculate god ray contribution
        vec3 godRayContribution = lightColor * scattering * phase * noiseFactor * stepSize;
        godRayColor += godRayContribution;
    }

    return godRayColor;
}

// Atmospheric scattering
vec3 calculateAtmosphericScattering(vec3 rayStart, vec3 rayEnd, vec3 sunDir, vec3 sunColor) {
    vec3 rayDir = normalize(rayEnd - rayStart);
    float rayLength = length(rayEnd - rayStart);

    vec3 atmosphericColor = vec3(0.0);
    float stepSize = rayLength / float(volUniforms.sampleCount);

    for(int i = 0; i < volUniforms.sampleCount; ++i) {
        float t = (float(i) + 0.5) * stepSize;
        vec3 samplePos = rayStart + rayDir * t;

        // Calculate height-based density
        float height = samplePos.y;
        float density = exp(-height / 8000.0); // Scale height of 8km

        // Calculate scattering
        float scattering = volUniforms.scatteringCoeff * density;

        // Calculate phase function for atmospheric scattering
        float cosTheta = dot(rayDir, sunDir);
        float phase = (1.0 - volUniforms.phaseFunction * volUniforms.phaseFunction) /
                     pow(1.0 + volUniforms.phaseFunction * volUniforms.phaseFunction -
                         2.0 * volUniforms.phaseFunction * cosTheta, 1.5);

        // Calculate atmospheric contribution
        vec3 atmosphericContribution = sunColor * scattering * phase * stepSize;
        atmosphericColor += atmosphericContribution;
    }

    return atmosphericColor;
}

void main() {
    // Sample scene texture
    vec3 sceneColor = texture(sceneTexture, fragTexCoord).rgb;

    // Reconstruct world position from depth buffer
    float depth = texture(depthTexture, fragTexCoord).r;
    vec3 worldPos = fragPos; // Use fragment position as fallback

    // TODO: Implement proper world position reconstruction from depth
    // This would require inverse projection and view matrices
    // vec2 ndc = fragTexCoord * 2.0 - 1.0;
    // vec4 clipPos = vec4(ndc, depth, 1.0);
    // vec4 viewPos = inverseProjection * clipPos;
    // viewPos /= viewPos.w;
    // vec4 worldPos4 = inverseView * viewPos;
    // worldPos = worldPos4.xyz;

    // Calculate volumetric lighting
    vec3 volumetricLighting = calculateVolumetricLighting(
        volUniforms.cameraPos,
        worldPos,
        volUniforms.lightPos,
        volUniforms.lightColor * volUniforms.lightIntensity
    );

    // Calculate god rays
    vec3 godRays = calculateGodRays(
        volUniforms.cameraPos,
        worldPos,
        volUniforms.lightPos,
        volUniforms.lightColor * volUniforms.lightIntensity
    );

    // Calculate atmospheric scattering
    vec3 sunDir = normalize(volUniforms.lightPos - volUniforms.cameraPos);
    vec3 atmosphericScattering = calculateAtmosphericScattering(
        volUniforms.cameraPos,
        worldPos,
        sunDir,
        volUniforms.lightColor * volUniforms.lightIntensity
    );

    // Calculate fog
    float fogFactor = calculateFog(worldPos, volUniforms.cameraPos, FOG_EXPONENTIAL);
    vec3 fogColor = vec3(0.7, 0.8, 1.0); // Blue-white fog

    // Combine all effects
    vec3 finalColor = sceneColor;
    finalColor += volumetricLighting;
    finalColor += godRays;
    finalColor += atmosphericScattering;

    // Apply fog
    finalColor = mix(fogColor, finalColor, fogFactor);

    // Tone mapping and gamma correction
    finalColor = finalColor / (finalColor + vec3(1.0));
    finalColor = pow(finalColor, vec3(1.0/2.2));

    outColor = vec4(finalColor, 1.0);
}
