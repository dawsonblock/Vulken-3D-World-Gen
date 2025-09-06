#version 460

// Ray tracing closest hit shader
#extension GL_EXT_ray_tracing : require

// Ray payload
struct RayPayload {
    vec3 color;
    float distance;
    vec3 normal;
    vec3 albedo;
    float metallic;
    float roughness;
};

// Hit attributes
layout(location = 0) in vec3 barycentricCoords;
layout(location = 1) in vec2 texCoords;

// Shader binding table
layout(binding = 0, set = 1) uniform sampler2D diffuseTexture;
layout(binding = 1, set = 1) uniform sampler2D normalTexture;
layout(binding = 2, set = 1) uniform sampler2D metallicRoughnessTexture;

// Ray tracing uniforms
layout(binding = 3, set = 1) uniform RayTracingUniforms {
    vec3 lightPos;
    vec3 lightColor;
    float lightIntensity;
    int maxBounces;
    int samplesPerPixel;
    float roughness;
    float metallic;
} rtUniforms;

// PBR material properties
struct Material {
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
};

// PBR lighting calculation
vec3 calculatePBR(vec3 albedo, vec3 normal, vec3 viewDir, vec3 lightDir, vec3 lightColor, float metallic, float roughness) {
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

    // Diffuse and specular
    vec3 diffuse = kD * albedo / 3.14159;
    vec3 radiance = lightColor * NdotL;

    return (diffuse + specular) * radiance;
}

// Ray tracing reflection calculation
vec3 calculateReflection(vec3 incident, vec3 normal, float roughness) {
    // Calculate reflection direction with roughness
    vec3 reflection = reflect(incident, normal);

    // Add roughness-based random perturbation
    vec3 random = vec3(
        fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453),
        fract(sin(dot(gl_FragCoord.xy, vec2(37.719, 15.937))) * 43758.5453),
        fract(sin(dot(gl_FragCoord.xy, vec2(73.123, 91.456))) * 43758.5453)
    );

    // Apply roughness to reflection direction
    vec3 perturbed = normalize(reflection + (random - 0.5) * roughness);
    return perturbed;
}

void main() {
    // Get hit information
    vec3 hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 normal = gl_WorldRayDirectionEXT; // Simplified normal calculation

    // Sample textures
    vec3 albedo = texture(diffuseTexture, texCoords).rgb;
    vec3 normalMap = texture(normalTexture, texCoords).rgb * 2.0 - 1.0;
    vec3 metallicRoughness = texture(metallicRoughnessTexture, texCoords).rgb;

    // Calculate material properties
    Material material;
    material.albedo = albedo;
    material.metallic = metallicRoughness.b;
    material.roughness = metallicRoughness.g;
    material.ao = metallicRoughness.r;

    // Calculate lighting
    vec3 viewDir = normalize(gl_WorldRayOriginEXT - hitPoint);
    vec3 lightDir = normalize(rtUniforms.lightPos - hitPoint);

    vec3 color = calculatePBR(
        material.albedo,
        normal,
        viewDir,
        lightDir,
        rtUniforms.lightColor * rtUniforms.lightIntensity,
        material.metallic,
        material.roughness
    );

    // Apply ambient occlusion
    color *= material.ao;

    // Store ray payload
    RayPayload payload;
    payload.color = color;
    payload.distance = gl_HitTEXT;
    payload.normal = normal;
    payload.albedo = material.albedo;
    payload.metallic = material.metallic;
    payload.roughness = material.roughness;

    // Store payload (simplified for this example)
    // In a real implementation, this would be stored in the ray payload
}
