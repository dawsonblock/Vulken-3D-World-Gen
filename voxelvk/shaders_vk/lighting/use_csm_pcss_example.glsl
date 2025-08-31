
// Example usage in your main lighting shader:

#include "shadows/csm_common.glsl"
#include "lighting/pcss.glsl"

layout(binding=5) uniform sampler2DArray uShadowMap; // wired from code

float computeShadowVisibility(vec3 worldPos, float viewSpaceZ){
    int cas = chooseCascade(viewSpaceZ);
    vec4 lp = uLightViewProj[cas] * vec4(worldPos,1.0);
    vec3 uvw = lp.xyz / lp.w;
    uvw.xy = uvw.xy * 0.5 + 0.5;

    if (any(lessThan(uvw.xy, vec2(0.0))) || any(greaterThan(uvw.xy, vec2(1.0)))) return 1.0;

    return pcss_visibility(uShadowMap, vec3(uvw.xy, cas),
                           uMapTexelSize, uPCSSSearch, uPCSSMin, uPCSSMax, 0.0008);
}
