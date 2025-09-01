
#ifndef IBL_GLSL
#define IBL_GLSL
vec3 ibl_specular(vec3 N, vec3 V, vec3 F0, float roughness, samplerCube prefilterMap, sampler2D brdfLut, float maxMip){
    float NoV = max(dot(N,V), 0.0);
    vec3 R = reflect(-V, N);
    vec2 brdf = texture(brdfLut, vec2(NoV, roughness)).rg;
    vec3 prefiltered = textureLod(prefilterMap, R, roughness * maxMip).rgb;
    return prefiltered * (F0*brdf.x + brdf.y);
}
#endif
