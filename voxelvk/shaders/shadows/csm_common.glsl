
#version 460 core
layout(std140, binding=3) uniform CSMData {
    mat4 uLightViewProj[4];
    float uCascadeSplits[4];
    int   uCascadeCount;
    float uMapTexelSize;
    float uPCSSMin; float uPCSSMax; float uPCSSSearch;
};
uniform sampler2DArray uShadowMap;
int chooseCascade(float viewSpaceDepth){
    for(int i=0;i<uCascadeCount;i++){
        if(viewSpaceDepth < uCascadeSplits[i]) return i;
    } return uCascadeCount-1;
}
