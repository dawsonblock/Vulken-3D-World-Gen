#version 450

// PI constant for consistent precision
const float PI = 3.14159265359;

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in mat3 fragTBN; // Tangent-Bitangent-Normal matrix from vertex shader

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D albedoTexture;
layout(binding = 2) uniform sampler2D normalTexture;
layout(binding = 3) uniform sampler2D metallicRoughnessTexture;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat3 normalMatrix; // Precomputed normal matrix from CPU
    vec3 lightPos;
    vec3 lightColor;
    vec3 viewPos;
} ubo;

// PBR lighting functions
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / denom;
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // Sample textures
    vec3 albedo = texture(albedoTexture, fragTexCoord).rgb;
    vec3 metallicRoughness = texture(metallicRoughnessTexture, fragTexCoord).rgb;
    float metallic = metallicRoughness.b;
    float roughness = metallicRoughness.g;
    
    // Sample and transform normal map properly
    vec3 sampledNormal = texture(normalTexture, fragTexCoord).rgb * 2.0 - 1.0; // Convert [0,1] to [-1,1]
    vec3 normal = normalize(fragTBN * sampledNormal); // Transform to world space using TBN matrix
    
    // If TBN matrix is not available, fall back to interpolated normal
    // This check ensures compatibility when tangent/bitangent are not provided
    if (length(fragTBN[0]) < 0.1) {
        normal = normalize(fragNormal);
    }
    
    // PBR calculations
    vec3 N = normal;
    vec3 V = normalize(ubo.viewPos - fragPosition);
    
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    // Light contribution
    vec3 L = normalize(ubo.lightPos - fragPosition);
    vec3 H = normalize(V + L);
    float distance = length(ubo.lightPos - fragPosition);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = ubo.lightColor * attenuation;
    
    // Cook-Torrance BRDF
    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    
    float NdotL = max(dot(N, L), 0.0);
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;
    
    // Ambient lighting
    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo;
    
    // HDR tonemapping and gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
    
    outColor = vec4(color, 1.0);
}