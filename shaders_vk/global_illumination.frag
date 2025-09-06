#version 450

// Global illumination fragment shader with light probes
layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

// Light probe textures
layout(binding = 0) uniform samplerCube irradianceMap;
layout(binding = 1) uniform samplerCube prefilterMap;
layout(binding = 2) uniform sampler2D brdfLUT;

// Material textures
layout(binding = 3) uniform sampler2D albedoMap;
layout(binding = 4) uniform sampler2D normalMap;
layout(binding = 5) uniform sampler2D metallicRoughnessMap;

// Global illumination uniforms
layout(binding = 6) uniform GlobalIlluminationUniforms {
    vec3 cameraPos;
    vec3 lightPos;
    vec3 lightColor;
    float lightIntensity;
    float exposure;
    int lightProbeCount;
    float probeRadius;
    float prefilterMaxMipLevel;  // Maximum mip level for prefiltered environment map
} giUniforms;

// Light probe data
struct LightProbe {
    vec3 position;
    float radius;
    float intensity;
};

// Light probe buffer
layout(binding = 7) readonly buffer LightProbeBuffer {
    LightProbe lightProbes[];
} probeBuffer;

// PBR material properties
struct Material {
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
};

// Calculate light probe influence
float calculateProbeInfluence(vec3 worldPos, vec3 probePos, float probeRadius) {
    float distance = length(worldPos - probePos);
    float influence = 1.0 - smoothstep(0.0, probeRadius, distance);
    return influence;
}

// Sample irradiance from light probes
vec3 sampleIrradiance(vec3 normal, vec3 worldPos) {
    vec3 irradiance = vec3(0.0);
    float totalWeight = 0.0;

    // Sample from all light probes
    for(int i = 0; i < giUniforms.lightProbeCount; ++i) {
        LightProbe probe = probeBuffer.lightProbes[i];
        float influence = calculateProbeInfluence(worldPos, probe.position, probe.radius);

        if(influence > 0.0) {
            // Sample irradiance map
            vec3 probeToWorld = normalize(worldPos - probe.position);
            vec3 irradianceSample = texture(irradianceMap, probeToWorld).rgb;
            irradiance += irradianceSample * influence * probe.intensity;
            totalWeight += influence;
        }
    }

    // Normalize by total weight
    if(totalWeight > 0.0) {
        irradiance /= totalWeight;
    }

    return irradiance;
}

// Sample prefiltered environment map
vec3 samplePrefilteredEnvironment(vec3 reflectionDir, float roughness) {
    // Calculate mip level based on roughness
    float mipLevel = roughness * giUniforms.prefilterMaxMipLevel; // Use actual max mip level

    // Sample prefiltered environment map
    vec3 prefilteredColor = textureLod(prefilterMap, reflectionDir, mipLevel).rgb;

    return prefilteredColor;
}

// Calculate BRDF lookup
vec2 calculateBRDF(float NdotV, float roughness) {
    return texture(brdfLUT, vec2(NdotV, roughness)).rg;
}

// PBR lighting with global illumination
vec3 calculatePBRWithGI(vec3 albedo, vec3 normal, vec3 viewDir, vec3 lightDir, vec3 lightColor,
                       float metallic, float roughness, vec3 worldPos) {
    // Normalize vectors
    normal = normalize(normal);
    viewDir = normalize(viewDir);
    lightDir = normalize(lightDir);

    // Calculate half vector
    vec3 halfDir = normalize(lightDir + viewDir);

    // Calculate dot products
    float NdotL = max(dot(normal, lightDir), 0.0);
    float NdotV = max(dot(normal, viewDir), 0.0);
    float NdotH = max(dot(normal, halfDir), 0.0);
    float VdotH = max(dot(viewDir, halfDir), 0.0);

    // Fresnel (Schlick approximation)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    // Normal Distribution Function (GGX/Trowbridge-Reitz)
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    float D = alpha2 / (3.14159 * denom * denom);

    // Geometry Function (Smith's method)
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G1L = NdotL / (NdotL * (1.0 - k) + k);
    float G1V = NdotV / (NdotV * (1.0 - k) + k);
    float G = G1L * G1V;

    // Cook-Torrance BRDF
    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular = numerator / denominator;

    // Energy conservation
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);

    // Direct lighting
    vec3 diffuse = kD * albedo / 3.14159;
    vec3 directLighting = (diffuse + specular) * lightColor * NdotL;

    // Global illumination from light probes
    vec3 irradiance = sampleIrradiance(normal, worldPos);
    vec3 diffuseGI = kD * albedo * irradiance;

    // Specular global illumination
    vec3 reflectionDir = reflect(-viewDir, normal);
    vec3 prefilteredColor = samplePrefilteredEnvironment(reflectionDir, roughness);
    vec2 brdf = calculateBRDF(NdotV, roughness);
    vec3 specularGI = prefilteredColor * (F * brdf.x + brdf.y);

    // Combine direct and indirect lighting
    return directLighting + diffuseGI + specularGI;
}

void main() {
    // Sample material textures
    vec3 albedo = texture(albedoMap, fragTexCoord).rgb;
    vec3 normalMap = texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0;
    vec3 metallicRoughness = texture(metallicRoughnessMap, fragTexCoord).rgb;

    // Calculate material properties
    Material material;
    material.albedo = albedo;
    material.metallic = metallicRoughness.b;
    material.roughness = metallicRoughness.g;
    material.ao = metallicRoughness.r;

    // Calculate lighting
    vec3 viewDir = normalize(giUniforms.cameraPos - fragPos);
    vec3 lightDir = normalize(giUniforms.lightPos - fragPos);

    vec3 color = calculatePBRWithGI(
        material.albedo,
        fragNormal,
        viewDir,
        lightDir,
        giUniforms.lightColor * giUniforms.lightIntensity,
        material.metallic,
        material.roughness,
        fragPos
    );

    // Apply ambient occlusion
    color *= material.ao;

    // Tone mapping and gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}
