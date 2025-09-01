#version 460
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec4 inCurrentPos;
layout(location = 2) in vec4 inPreviousPos;

layout(location = 0) out vec2 outMotionVector;

void main() {
    // Convert to NDC
    vec2 currentNDC = inCurrentPos.xy / inCurrentPos.w;
    vec2 previousNDC = inPreviousPos.xy / inPreviousPos.w;
    
    // Convert to screen coordinates [0,1]
    vec2 currentScreen = currentNDC * 0.5 + 0.5;
    vec2 previousScreen = previousNDC * 0.5 + 0.5;
    
    // Motion vector in screen space
    vec2 motion = currentScreen - previousScreen;
    
    outMotionVector = motion;
}