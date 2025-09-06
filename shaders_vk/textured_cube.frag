#version 450

const float PI = 3.14159265359;

// Textured cube fragment shader with PBR lighting
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

// Texture samplers
layout(binding = 1) uniform sampler2D diffuseTexture;
layout(binding = 2) uniform sampler2D normalTexture;

// Lighting uniforms
layout(binding = 3) uniform LightingUniforms {
    vec3 lightPos;
    vec3 lightColor;
    vec3 viewPos;
    float metallic;
    float roughness;
} lighting;

// PBR lighting calculation
vec3 calculatePBR(vec3 albedo, vec3 normal, vec3 viewDir, vec3 lightDir, vec3 lightColor) {
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

    // PBR material properties
    float metallic = lighting.metallic;
    float roughness = lighting.roughness;

    // Fresnel (Schlick approximation)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    // Normal Distribution Function (GGX/Trowbridge-Reitz)
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    float D = alpha2 / (PI * denom * denom);

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

    // Diffuse and specular
    vec3 diffuse = kD * albedo / PI;
    vec3 radiance = lightColor * NdotL;

    return (diffuse + specular) * radiance;
}

void main() {
    // Sample textures
    vec3 albedo = texture(diffuseTexture, fragTexCoord).rgb * fragColor;

    // Sample and transform normal map
    vec3 normalMap = texture(normalTexture, fragTexCoord).rgb * 2.0 - 1.0;
    float normalStrength = 1.0; // Could be made configurable via uniform
    vec3 normal = normalize(mix(fragNormal, normalMap, normalStrength));

    // Calculate lighting
    vec3 viewDir = lighting.viewPos - fragPos;
    vec3 lightDir = lighting.lightPos - fragPos;

    vec3 result = calculatePBR(albedo, normal, viewDir, lightDir, lighting.lightColor);

    // Tone mapping and gamma correction
    result = result / (result + vec3(1.0));
    result = pow(result, vec3(1.0/2.2));

    outColor = vec4(result, 1.0);
}
