
#ifndef PBR_COMMON_GLSL
#define PBR_COMMON_GLSL
float saturate(float x){ return clamp(x,0.0,1.0); }
vec3  saturate(vec3 v){ return clamp(v,0.0,1.0); }
float D_GGX(float NoH, float a){ float a2=a*a; float d=(NoH*NoH)*(a2-1.0)+1.0; return a2/(3.14159265*d*d + 1e-6); }
float V_SmithGGX(float NoV, float NoL, float a){
    float a2=a*a;
    float gv = NoV + sqrt(a2 + (1.0-a2)*NoV*NoV);
    float gl = NoL + sqrt(a2 + (1.0-a2)*NoL*NoL);
    return 1.0 / (gv*gl + 1e-6);
}
vec3 F_Schlick(vec3 F0, float VoH){ return F0 + (1.0 - F0) * pow(1.0 - VoH, 5.0); }
vec3 lambert(vec3 albedo){ return albedo / 3.14159265; }
#endif
