#version 450

// 3D triangle fragment shader with basic lighting
layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    // Simple color output with slight gamma correction
    outColor = vec4(pow(fragColor, vec3(1.0/2.2)), 1.0);
}
