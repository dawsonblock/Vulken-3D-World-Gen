#version 450

// 3D cube fragment shader with Phong lighting
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

// Lighting uniforms
layout(binding = 1) uniform LightingUniforms {
    vec3 lightPos;
    vec3 lightColor;
    vec3 viewPos;
} lighting;

void main() {
    // Ambient lighting
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lighting.lightColor;

    // Diffuse lighting
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(lighting.lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lighting.lightColor;

    // Specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(lighting.viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lighting.lightColor;

    // Combine lighting
    vec3 result = (ambient + diffuse + specular) * fragColor;
    outColor = vec4(pow(result, vec3(1.0/2.2)), 1.0);
}
