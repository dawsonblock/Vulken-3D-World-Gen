#version 450
#extension GL_GOOGLE_include_directive : enable
#include "../common/weather_ubo.glsl"
layout(location=0) in vec2 vUv;
layout(location=0) out vec4 outColor;

layout(set=0,binding=1) uniform Camera { mat4 invViewProj; vec3 camPos; float _pad; } uCam;

// --- Helpers
vec3 tonemap(vec3 x){ return x/(1.0+max(vec3(0.0),x)); }

// XYZ->sRGB
mat3 M = mat3( 3.2406,-1.5372,-0.4986,
              -0.9689, 1.8758, 0.0415,
               0.0557,-0.2040, 1.0570);

// Perez/Preetham sky model (RGB via chromaticity fits)
struct Perez { float A; float B; float C; float D; float E; };
Perez perezL(float T){ // luminance
  return Perez( 0.1787*T - 1.4630,
               -0.3554*T + 0.4275,
               -0.0227*T + 5.3251,
                0.1206*T - 2.5771,
               -0.0670*T + 0.3703);
}
Perez perezX(float T){
  return Perez(-0.0193*T - 0.2592,
               -0.0665*T + 0.0008,
               -0.0004*T + 0.2125,
               -0.0641*T - 0.8989,
               -0.0033*T + 0.0452);
}
Perez perezY(float T){
  return Perez(-0.0167*T - 0.2608,
               -0.0950*T + 0.0092,
               -0.0079*T + 0.2102,
               -0.0441*T - 1.6537,
               -0.0109*T + 0.0529);
}

float perezF(Perez P, float theta, float gamma){
  float cosTheta = cos(theta);
  return (1.0 + P.A*exp(P.B/(cosTheta+0.01)))*(1.0 + P.C*exp(P.D*gamma) + P.E*cos(gamma)*cos(gamma));
}

// Preetham zenith values
float zenithL(float T, float thetaS){
  float chi = (4.0/9.0 - T/120.0)*(3.14159265 - 2.0*thetaS);
  return ( (4.0453*T - 4.9710)*tan(chi) - 0.2155*T + 2.4192 ) * 1000.0;
}
float zenithX(float T, float thetaS){
  float t2=T*T, ts=thetaS, ts2=ts*ts;
  return (  0.00165*ts2 + -0.00374*ts + 0.00208 )*t2
       + ( -0.02902*ts2 +  0.06377*ts - 0.03202)*T
       + (  0.11693*ts2 + -0.21196*ts + 0.06052);
}
float zenithY(float T, float thetaS){
  float t2=T*T, ts=thetaS, ts2=ts*ts;
  return (  0.00275*ts2 + -0.00610*ts + 0.00316 )*t2
       + ( -0.04214*ts2 +  0.08970*ts - 0.04153)*T
       + (  0.15346*ts2 + -0.26756*ts + 0.06670);
}

vec3 sky_preetham(vec3 viewDir, vec3 sunDir, float turbidity){
  float theta = acos(clamp(viewDir.y, -0.999, 0.999));            // angle from zenith
  float thetaS= acos(clamp(sunDir.y,  -0.999, 0.999));
  float gamma = acos(clamp(dot(viewDir, sunDir), -0.999, 0.999)); // angle between directions

  Perez PL = perezL(turbidity);
  Perez PX = perezX(turbidity);
  Perez PY = perezY(turbidity);

  float FthetaGamma_L = perezF(PL, theta, gamma);
  float FthetaGamma_X = perezF(PX, theta, gamma);
  float FthetaGamma_Y = perezF(PY, theta, gamma);

  float ZL = zenithL(turbidity, thetaS);
  float ZX = zenithX(turbidity, thetaS);
  float ZY = zenithY(turbidity, thetaS);

  float denomL = perezF(PL, 0.0, thetaS);  // normalize at zenith
  float denomX = perezF(PX, 0.0, thetaS);
  float denomY = perezF(PY, 0.0, thetaS);

  float Y = ZL * (FthetaGamma_L / max(0.01, denomL));
  float x = ZX * (FthetaGamma_X / max(0.01, denomX));
  float y = ZY * (FthetaGamma_Y / max(0.01, denomY));

  // chromaticity to XYZ (Y is luminance)
  float X = (x / max(1e-3, y)) * Y;
  float Z = ((1.0 - x - y) / max(1e-3, y)) * Y;
  vec3 rgb = max(vec3(0.0), M * vec3(X, Y, Z));

  // soft sun bloom & lightning boost
  float sunDisc = smoothstep(0.015, 0.0, acos(dot(viewDir, sunDir)));
  vec3 sunCol = vec3(40.0, 35.0, 30.0) * sunDisc;
  float flash = uWeather.lightning;           // 0..1
  rgb = rgb * (1.0 + flash*3.0) + sunCol;

  return tonemap(rgb * 0.0018); // scale to fit tonemapper nicely
}

void main(){
  // reconstruct view ray from fullscreen tri
  vec2 ndc = vUv*2.0 - 1.0;
  vec4 hpos = vec4(ndc, 1.0, 1.0);
  vec4 wpos = uCam.invViewProj * hpos; wpos /= wpos.w;
  vec3 viewDir = normalize(wpos.xyz - uCam.camPos);

  vec3 col = sky_preetham(viewDir, normalize(uWeather.sunDir), max(1.8, uWeather.turbidity));
  // quick fog tint if heavy fog
  float fog = clamp(uWeather.fogDensity*25.0, 0.0, 0.5);
  col = mix(col, vec3(0.85), fog);
  outColor = vec4(col, 1.0);
}