
#version 460 core
out vec2 fragColor; in vec2 vUV;
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
float GeometrySchlickGGX(float NdotV,float k){ return NdotV/(NdotV*(1.0-k)+k); }
float GeometrySmith(float NoV, float NoL, float k){ return GeometrySchlickGGX(NoV,k)*GeometrySchlickGGX(NoL,k); }
void main(){
    float NoV = max(vUV.x, 1e-4);
    float rough = clamp(vUV.y, 0.0, 1.0);
    float a = rough*rough;
    vec3 V = vec3(sqrt(1.0-NoV*NoV), 0.0, NoV);
    float A=0.0, B=0.0; const uint N=1024u;
    for(uint i=0u;i<N;i++){
        vec2 Xi = Hammersley(i,N);
        vec3 H = ImportanceSampleGGX(Xi, vec3(0,0,1), a);
        vec3 L = normalize(2.0*dot(V,H)*H - V);
        float NoL = max(L.z,0.0), NoH = max(H.z,0.0), VoH = max(dot(V,H),0.0);
        if(NoL>0.0){
            float k = (a+1.0); k = (k*k)/8.0;
            float G = GeometrySmith(NoV, NoL, k);
            float G_Vis = (G * VoH)/(NoH*NoV + 1e-5);
            float Fc = pow(1.0 - VoH, 5.0);
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(N); B /= float(N);
    fragColor = vec2(A, B);
}
