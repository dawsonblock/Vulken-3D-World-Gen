
#version 460
layout(location=0) out vec4 outColor;
layout(location=0) in vec2 vUV;
layout(push_constant) uniform PC { vec3 uSunDir; float uTime; vec3 uCamPos; } pc;
void main(){
    float mu = clamp(dot(normalize(vec3(vUV*2.0-1.0, 1.0)), normalize(pc.uSunDir)), 0.0, 1.0);
    vec3 skyCol = mix(vec3(0.02,0.05,0.10), vec3(0.35,0.45,0.65), vUV.y);
    vec3 mie = vec3(pow(mu, 12.0));
    vec3 rayleigh = vec3(pow(mu, 2.0));
    vec3 color = skyCol + 0.35*rayleigh + 0.15*mie;
    outColor = vec4(pow(color, vec3(1.0/2.2)), 1.0);
}
