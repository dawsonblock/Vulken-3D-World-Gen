#version 460
#extension GL_GOOGLE_include_directive : enable
#include "../common/weather_ubo.glsl"
layout(location=0) in vec2 vUv;
layout(location=0) out vec4 outColor;

layout(set=0,binding=0) uniform sampler2D Scene;   // color before fog
layout(set=0,binding=1) uniform sampler2D Depth;   // linear 0..1 depth
layout(set=0,binding=2) uniform CameraFog { mat4 invProj; mat4 invView; vec3 camPos; float zFar; } uCF;

float height_fog_factor(float viewY, float dist, float H, float density){
  // Exponential height fog: σ(y) = density * exp(-(y/H))
  float k = density;
  float h = H;
  float f = 1.0 - exp(-k * dist * clamp(exp(-viewY/h), 0.2, 5.0));
  return clamp(f, 0.0, 1.0);
}

void main(){
  vec2 uv = vUv;
  vec3 col = texture(Scene, uv).rgb;
  float d  = texture(Depth, uv).r;          // 0 near .. 1 far (ensure linearized)
  if(d>=1.0){ outColor = vec4(col,1.0); return; }

  // Reconstruct view ray for height check
  vec2 ndc = uv*2.0-1.0;
  vec4 vpos = uCF.invProj * vec4(ndc, d*2.0-1.0, 1.0);
  vpos /= vpos.w;
  vec4 wpos = uCF.invView * vpos;

  float dist = length(wpos.xyz - uCF.camPos);
  float viewY = wpos.y;

  float baseDensity = uWeather.fogDensity;
  // boost in storm/fog states
  if(uWeather.state==WX_STORM) baseDensity = max(baseDensity, 0.02);
  if(uWeather.state==WX_FOG)   baseDensity = max(baseDensity, 0.04);

  float F = height_fog_factor(viewY, dist, 60.0, baseDensity);
  vec3 fogCol = mix(vec3(0.85), vec3(0.75,0.78,0.82), clamp(uWeather.cloudCoverage,0.0,1.0));
  vec3 outC = mix(col, fogCol, F);
  outColor = vec4(outC, 1.0);
}