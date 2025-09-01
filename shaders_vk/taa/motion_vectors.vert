#version 460
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec4 outCurrentPos;
layout(location = 2) out vec4 outPreviousPos;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 currentMVP;
    mat4 previousMVP;
    vec2 jitterCurrent;
    vec2 jitterPrevious;
} uCamera;

void main() {
    vec4 worldPos = vec4(inPosition, 1.0);
    
    // Current frame position
    outCurrentPos = uCamera.currentMVP * worldPos;
    
    // Previous frame position  
    outPreviousPos = uCamera.previousMVP * worldPos;
    
    outTexCoord = inTexCoord;
    gl_Position = outCurrentPos;
}