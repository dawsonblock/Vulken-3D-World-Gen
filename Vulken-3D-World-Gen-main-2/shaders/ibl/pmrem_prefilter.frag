
#version 450
layout(location = 0) out vec3 fragColor;
layout(location = 0) in vec3 vDir;
layout(binding = 0) uniform samplerCube uEnv;
layout(binding = 1) uniform Params {
    float uRoughness;
    int uSampleCount;
};
float RadicalInverse_VdC(uint bits){ bits = (bits<<16u)|(bits>>16u); bits = ((bits & 0x55555555u)<<1u)|((bits & 0xAAAAAAAAu)>>1u);
bits = ((bits & 0x33333333u)<<2u)|((bits & 0xCCCCCCCCu)>>2u); bits = ((bits & 0x0F0F0F0Fu)<<4u)|((bits & 0xF0F0F0F0u)>>4u);
bits = ((bits & 0x00FF00FFu)<<8u)|((bits & 0xFF00FF00u)>>8u); return float(bits)*2.3283064365386963e-10; }
vec2 Hammersley(uint i, uint N){ return vec2(float(i)/float(N), RadicalInverse_VdC(i)); }
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float a){
    float phi = 6.2831853 * Xi.x; float cosTheta = sqrt((1.0 - Xi.y)/(1.0 + (a*a - 1.0)*Xi.y)); float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
    vec3 H = vec3(cos(phi)*sinTheta, sin(phi)*sinTheta, cosTheta);
    vec3 up = abs(N.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 T = normalize(cross(up,N)); vec3 B = cross(N,T);
    return normalize(T*H.x + B*H.y + N*H.z);
}
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
