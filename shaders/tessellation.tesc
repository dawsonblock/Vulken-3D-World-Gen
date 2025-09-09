#version 450

// Tessellation control shader
layout(vertices = 3) out;

// Input from vertex shader
layout(location = 0) in vec3 fragColor[];
layout(location = 1) in vec3 fragNormal[];
layout(location = 2) in vec2 fragTexCoord[];

// Output to tessellation evaluation shader
layout(location = 0) out vec3 tessColor[];
layout(location = 1) out vec3 tessNormal[];
layout(location = 2) out vec2 tessTexCoord[];

// Tessellation uniforms
layout(binding = 0) uniform TessellationUniforms {
    float tessellationLevel;
    float displacementScale;
    float edgeTessellationFactor;
    float innerTessellationFactor;
    vec3 cameraPos;
    float maxTessellationLevel;
    float minTessellationLevel;
} tessUniforms;

// Calculate tessellation level based on distance and screen space
float calculateTessellationLevel(vec3 worldPos, vec3 cameraPos) {
    float distance = length(worldPos - cameraPos);

    // Calculate screen space tessellation factor
    float screenSpaceFactor = 1.0 / max(distance, 0.1);

    // Calculate tessellation level
    float tessLevel = tessUniforms.tessellationLevel * screenSpaceFactor;

    // Clamp to min/max values
    tessLevel = clamp(tessLevel, tessUniforms.minTessellationLevel, tessUniforms.maxTessellationLevel);

    return tessLevel;
}

// Calculate edge tessellation level
float calculateEdgeTessellationLevel(int edge) {
    vec3 worldPos = gl_in[edge].gl_Position.xyz;
    return calculateTessellationLevel(worldPos, tessUniforms.cameraPos);
}

void main() {
    // Pass data to tessellation evaluation shader
    tessColor[gl_InvocationID] = fragColor[gl_InvocationID];
    tessNormal[gl_InvocationID] = fragNormal[gl_InvocationID];
    tessTexCoord[gl_InvocationID] = fragTexCoord[gl_InvocationID];

    // Calculate tessellation levels
    float tessLevel0 = calculateEdgeTessellationLevel(0);
    float tessLevel1 = calculateEdgeTessellationLevel(1);
    float tessLevel2 = calculateEdgeTessellationLevel(2);

    // Set outer tessellation levels
    gl_TessLevelOuter[0] = tessLevel0;
    gl_TessLevelOuter[1] = tessLevel1;
    gl_TessLevelOuter[2] = tessLevel2;

    // Set inner tessellation level
    gl_TessLevelInner[0] = (tessLevel0 + tessLevel1 + tessLevel2) / 3.0;
}
