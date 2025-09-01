
#version 460 core
out vec3 fragColor; in vec3 vDir; uniform samplerCube uEnv;
uniform float uRoughness; uniform int uSampleCount;
float RadicalInverse_VdC(uint); vec2 Hammersley(uint,uint);
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float a);
void main(){
    vec3 N = normalize(vDir); vec3 V = N; vec3 col=vec3(0); float a=max(uRoughness*uRoughness, 1e-4);
    const uint Nsamples=1024u;
    for(uint i=0u;i<Nsamples;i++){
        vec2 Xi = Hammersley(i,Nsamples);
        vec3 H = ImportanceSampleGGX(Xi, N, a);
        vec3 L = normalize(2.0*dot(V,H)*H - V);
        float NoL = max(dot(N,L), 0.0);
        if(NoL>0.0){ col += textureLod(uEnv, L, 0.0).rgb * NoL; }
    }
    col /= float(Nsamples);
    fragColor = col;
}
