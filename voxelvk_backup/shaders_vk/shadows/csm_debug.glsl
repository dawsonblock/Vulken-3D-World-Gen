
#ifndef CSM_DEBUG_GLSL
#define CSM_DEBUG_GLSL

#include "csm_common.glsl"

// Returns a stable, vivid tint per cascade (up to 4).
vec3 csm_debug_tint(int cas){
    const vec3 cols[4] = vec3[4](
        vec3(1.0, 0.2, 0.2), // red
        vec3(0.2, 1.0, 0.2), // green
        vec3(0.2, 0.6, 1.0), // blue
        vec3(1.0, 1.0, 0.2)  // yellow
    );
    cas = clamp(cas, 0, uCascadeCount-1);
    return cols[cas];
}

// Compute cascade index from view-space Z (positive depth) and return tint.
vec3 csm_debug_tint_from_depth(float viewSpaceZ){
    int cas = chooseCascade(viewSpaceZ);
    return csm_debug_tint(cas);
}

// Full world-space path: project to cascade, returns tint and validity.
vec3 csm_debug_tint_from_world(vec3 worldPos, float viewSpaceZ, out bool valid){
    int cas = chooseCascade(viewSpaceZ);
    vec4 lp = uLightViewProj[cas] * vec4(worldPos,1.0);
    vec3 uvw = lp.xyz / lp.w;
    uvw.xy = uvw.xy*0.5 + 0.5;
    valid = all(greaterThanEqual(uvw.xy, vec2(0.0))) && all(lessThanEqual(uvw.xy, vec2(1.0)));
    return csm_debug_tint(cas);
}

#endif // CSM_DEBUG_GLSL
