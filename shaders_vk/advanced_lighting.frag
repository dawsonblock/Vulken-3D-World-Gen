#version 450

const float PI = 3.141592653589793;

// Advanced lighting models fragment shader
layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragTangent;
layout(location = 4) in vec3 fragBitangent;

layout(location = 0) out vec4 outColor;

// Material textures
layout(binding = 0) uniform sampler2D albedoMap;
layout(binding = 1) uniform sampler2D normalMap;
layout(binding = 2) uniform sampler2D metallicRoughnessMap;
layout(binding = 3) uniform sampler2D emissiveMap;
layout(binding = 4) uniform sampler2D occlusionMap;

// Lighting uniforms
layout(binding = 5) uniform AdvancedLightingUniforms {
    vec3 cameraPos;
    vec3 lightPos;
    vec3 lightColor;
    float lightIntensity;
    float lightRadius;
    int lightType; // 0 = directional, 1 = point, 2 = spot, 3 = area
    vec3 lightDirection;
    float lightConeAngle;
    float lightFalloff;
    int lightingModel; // 0 = PBR, 1 = Blinn-Phong, 2 = Cook-Torrance, 3 = Oren-Nayar
    float ambientIntensity;
    vec3 ambientColor;
    int shadowType; // 0 = none, 1 = hard, 2 = soft, 3 = PCF
    float shadowBias;
    float shadowSoftness;
    mat4 lightSpaceMatrix;  // Light view-projection matrix for shadow mapping
} lighting;

// Shadow map
layout(binding = 6) uniform sampler2D shadowMap;

// IBL textures
layout(binding = 7) uniform samplerCube irradianceMap;
layout(binding = 8) uniform samplerCube prefilterMap;
layout(binding = 9) uniform sampler2D brdfLUT;

// Advanced lighting models
vec3 calculateBlinnPhong(vec3 albedo, vec3 normal, vec3 viewDir, vec3 lightDir, vec3 lightColor, float shininess) {
    vec3 halfDir = normalize(lightDir + viewDir);

    float NdotL = max(dot(normal, lightDir), 0.0);
    float NdotH = max(dot(normal, halfDir), 0.0);

    vec3 diffuse = albedo * NdotL;
    vec3 specular = lightColor * pow(NdotH, shininess);

    return diffuse + specular;
}

vec3 calculateCookTorrance(vec3 albedo, vec3 normal, vec3 viewDir, vec3 lightDir, vec3 lightColor,
                          float metallic, float roughness) {
    vec3 halfDir = normalize(lightDir + viewDir);

    float NdotL = max(dot(normal, lightDir), 0.0);
    float NdotV = max(dot(normal, viewDir), 0.0);
    float NdotH = max(dot(normal, halfDir), 0.0);
    float VdotH = max(dot(viewDir, halfDir), 0.0);

    // Fresnel
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    // Normal Distribution Function (GGX)
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    float D = alpha2 / (PI * denom * denom);

    // Geometry Function (Smith)
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

    vec3 diffuse = kD * albedo / 3.14159;

    return (diffuse + specular) * lightColor * NdotL;
}

vec3 calculateOrenNayar(vec3 albedo, vec3 normal, vec3 viewDir, vec3 lightDir, vec3 lightColor, float roughness) {
    float NdotL = max(dot(normal, lightDir), 0.0);
    float NdotV = max(dot(normal, viewDir), 0.0);

    float sigma = roughness * roughness;
    float A = 1.0 - 0.5 * sigma / (sigma + 0.33);
    float B = 0.45 * sigma / (sigma + 0.09);

    float alpha = max(acos(NdotL), acos(NdotV));
    float beta = min(acos(NdotL), acos(NdotV));

    vec3 diffuse = albedo * NdotL * (A + B * max(0.0, cos(alpha - beta)) * sin(alpha) * tan(beta));

    return diffuse * lightColor;
}

// Shadow calculation
float calculateShadow(vec3 fragPos, vec3 lightPos, vec3 lightDir) {
    if(lighting.shadowType == 0) {
        return 1.0;
    }

    // Transform to light space
    vec4 lightSpacePos = lighting.lightSpaceMatrix * vec4(fragPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;

    if(projCoords.x < 0.0 || projCoords.x > 1.0 ||
       projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }

    float currentDepth = projCoords.z;
    float closestDepth = texture(shadowMap, projCoords.xy).r;

    if(lighting.shadowType == 1) {
        // Hard shadows
        return currentDepth - lighting.shadowBias > closestDepth ? 0.0 : 1.0;
    } else if(lighting.shadowType == 2) {
        // Soft shadows
        float shadow = 0.0;
        vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

        for(int x = -1; x <= 1; ++x) {
            for(int y = -1; y <= 1; ++y) {
                float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
                shadow += currentDepth - lighting.shadowBias > pcfDepth ? 0.0 : 1.0;
            }
        }

        return shadow / 9.0;
    } else if(lighting.shadowType == 3) {
        // PCF shadows
        float shadow = 0.0;
        vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

        for(int x = -2; x <= 2; ++x) {
            for(int y = -2; y <= 2; ++y) {
                float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
                shadow += currentDepth - lighting.shadowBias > pcfDepth ? 0.0 : 1.0;
            }
        }

        return shadow / 25.0;
    }

    return 1.0;
}

// IBL calculation
vec3 calculateIBL(vec3 normal, vec3 viewDir, vec3 albedo, float metallic, float roughness) {
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Diffuse IBL
    vec3 irradiance = texture(irradianceMap, normal).rgb;
    vec3 kS = F0 + (1.0 - F0) * pow(1.0 - max(dot(normal, viewDir), 0.0), 5.0);
    vec3 kD = (1.0 - kS) * (1.0 - metallic);
    vec3 diffuse = kD * albedo * irradiance;

    // Specular IBL
    vec3 reflectionDir = reflect(-viewDir, normal);
    vec3 prefilteredColor = textureLod(prefilterMap, reflectionDir, roughness * 4.0).rgb;
    vec2 brdf = texture(brdfLUT, vec2(max(dot(normal, viewDir), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (F0 * brdf.x + brdf.y);

    return diffuse + specular;
}

void main() {
    // Sample material textures
    vec3 albedo = texture(albedoMap, fragTexCoord).rgb;
    vec3 tangentNormal = texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0;
    vec3 metallicRoughness = texture(metallicRoughnessMap, fragTexCoord).rgb;
    vec3 emissive = texture(emissiveMap, fragTexCoord).rgb;
    float occlusion = texture(occlusionMap, fragTexCoord).r;

    // Calculate material properties
    float metallic = metallicRoughness.b;
    float roughness = metallicRoughness.g;

    // Transform normal from tangent space to world space
    mat3 TBN = mat3(normalize(fragTangent), normalize(fragBitangent), normalize(fragNormal));
    vec3 normal = normalize(TBN * tangentNormal);
    vec3 viewDir = normalize(lighting.cameraPos - fragPos);
    vec3 lightDir = normalize(lighting.lightPos - fragPos);

    // Calculate light attenuation
    float distance = length(lighting.lightPos - fragPos);
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

    // Calculate shadow
    float shadow = calculateShadow(fragPos, lighting.lightPos, lightDir);

    // Calculate lighting based on model
    vec3 lightingResult = vec3(0.0);

    if(lighting.lightingModel == 0) {
        // PBR
        lightingResult = calculateCookTorrance(albedo, normal, viewDir, lightDir,
                                             lighting.lightColor * lighting.lightIntensity * attenuation,
                                             metallic, roughness);
    } else if(lighting.lightingModel == 1) {
        // Blinn-Phong
        lightingResult = calculateBlinnPhong(albedo, normal, viewDir, lightDir,
                                           lighting.lightColor * lighting.lightIntensity * attenuation,
                                           32.0);
    } else if(lighting.lightingModel == 2) {
        // Cook-Torrance
        lightingResult = calculateCookTorrance(albedo, normal, viewDir, lightDir,
                                             lighting.lightColor * lighting.lightIntensity * attenuation,
                                             metallic, roughness);
    } else if(lighting.lightingModel == 3) {
        // Oren-Nayar
        lightingResult = calculateOrenNayar(albedo, normal, viewDir, lightDir,
                                          lighting.lightColor * lighting.lightIntensity * attenuation,
                                          roughness);
    }

    // Apply shadow
    lightingResult *= shadow;

    // Add ambient lighting
    vec3 ambient = lighting.ambientColor * lighting.ambientIntensity * albedo;

    // Add IBL
    vec3 ibl = calculateIBL(normal, viewDir, albedo, metallic, roughness);

    // Add emissive
    vec3 emissiveLighting = emissive;

    // Combine all lighting
    vec3 finalColor = lightingResult + ambient + ibl + emissiveLighting;

    // Apply occlusion
    finalColor *= occlusion;

    // Tone mapping and gamma correction
    finalColor = finalColor / (finalColor + vec3(1.0));
    finalColor = pow(finalColor, vec3(1.0/2.2));

    outColor = vec4(finalColor, 1.0);
}
