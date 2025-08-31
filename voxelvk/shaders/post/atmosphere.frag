
#version 460 core
out vec4 fragColor;
in vec2 vUV;
uniform vec3 uSunDir;
uniform float uTime;
uniform vec3 uCamPos;
const vec3 betaR = vec3(5.5e-6, 13.0e-6, 22.4e-6);
const vec3 betaM = vec3(21e-6);
void main(){
    float mu = clamp(dot(normalize(vec3(vUV*2.0-1.0, 1.0)), normalize(uSunDir)), 0.0, 1.0);
    vec3 skyCol = mix(vec3(0.02,0.05,0.10), vec3(0.35,0.45,0.65), vUV.y);
    vec3 mie = vec3(pow(mu, 12.0));
    vec3 rayleigh = vec3(pow(mu, 2.0));
    vec3 color = skyCol + 0.35*rayleigh + 0.15*mie;
    fragColor = vec4(pow(color, vec3(1.0/2.2)), 1.0);
}
