
#version 460
layout(location=0) in vec3 inPosition;      // world-space position preferred (or model space with external model matrix)
layout(push_constant) uniform Push { int uCascadeIndex; } pc;
layout(std140, binding=3) uniform CSMData {
    mat4  uLightViewProj[4];
    float uCascadeSplits[4];
    int   uCascadeCount;
    float uMapTexelSize;
    float uPCSSMin;
    float uPCSSMax;
    float uPCSSSearch;
};
// If your vertices are model-space, multiply by your model matrix here (can be added later)
void main(){
    gl_Position = uLightViewProj[pc.uCascadeIndex] * vec4(inPosition, 1.0);
}
