
#ifndef CSM_COMMON_GLSL
#define CSM_COMMON_GLSL
layout(std140, binding = 3) uniform CSMData {
    mat4  uLightViewProj[4];
    float uCascadeSplits[4];
    int   uCascadeCount;
    float uMapTexelSize;
    float uPCSSMin;
    float uPCSSMax;
    float uPCSSSearch;
};
int chooseCascade(float viewSpaceZ){
    // viewSpaceZ should be positive forward (abs if needed)
    for(int i=0;i<uCascadeCount;i++){
        if(viewSpaceZ < uCascadeSplits[i]) return i;
    }
    return uCascadeCount-1;
}
#endif
