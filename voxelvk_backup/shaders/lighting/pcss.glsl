
#ifndef PCSS_GLSL
#define PCSS_GLSL
float pcss_shadow_factor(sampler2DShadow map, vec3 projCoord, float mapSize, float searchRadiusPx, float minFilterPx, float maxFilterPx){
    float r = searchRadiusPx / mapSize;
    float receiver = projCoord.z;
    const int K=16; float avgBlocker=0.0; int blockers=0;
    for(int i=0;i<K;i++){
        float a = 6.2831853 * (i / float(K));
        vec2 o = vec2(cos(a), sin(a)) * r * (float((i%4)+1));
        float d = texture(map, vec3(projCoord.xy + o, receiver));
        if(d < 0.5){ blockers++; avgBlocker += d; }
    }
    if(blockers==0) return 1.0;
    avgBlocker /= float(blockers);
    float penumbra = clamp((receiver - avgBlocker) * 200.0, minFilterPx, maxFilterPx) / mapSize;
    const int S=24; int lit=0;
    for(int i=0;i<S;i++){
        float a = 6.2831853 * (i / float(S));
        vec2 o = vec2(cos(a), sin(a)) * penumbra;
        lit += int(texture(map, vec3(projCoord.xy + o, receiver)) > 0.5);
    }
    return float(lit)/float(S);
}
#endif
