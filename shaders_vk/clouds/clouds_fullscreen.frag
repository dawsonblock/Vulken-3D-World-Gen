#version 460
#include "../common/weather_ubo.glsl"
layout(location=0) in vec2 vUv;
layout(location=0) out vec4 outColor;

// Simple fbm for 2D clouds. Time-of-day modulates albedo subtly.
float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7)))*43758.5453); }
float noise(vec2 p){
  vec2 i=floor(p), f=fract(p);
  float a=hash(i), b=hash(i+vec2(1,0)), c=hash(i+vec2(0,1)), d=hash(i+vec2(1,1));
  vec2 u=f*f*(3.0-2.0*f);
  return mix(a,b,u.x)+ (c-a)*u.y*(1.0-u.x) + (d-b)*u.x*u.y;
}
float fbm(vec2 p){
  float v=0.0; float a=0.5;
  for(int i=0;i<5;i++){ v+=a*noise(p); p*=2.02; a*=0.55; }
  return v;
}

void main(){
  // world drift from wind + cloud speed
  float t = uWeather.timeOfDay * 6.28318; // reuse tod as phase driver for demo
  vec2 drift = vec2(uWeather.windDir.x, uWeather.windDir.z) * (uWeather.windSpeed*0.02 + 0.2);
  vec2 uv = vUv * 4.0 + drift + vec2(t*0.05, t*0.03);

  float base = fbm(uv);
  float cover = clamp(uWeather.cloudCoverage, 0.0, 1.0);
  float dens = clamp(uWeather.cloudDensity, 0.0, 1.5);
  float clouds = smoothstep(1.0-cover*1.2, 0.75-cover*0.9, base) * dens;

  // darker in storms
  float storm = (uWeather.state==WX_STORM)? 0.5 : 0.0;
  vec3 ccol = mix(vec3(1.0), vec3(0.7,0.75,0.8), clouds + storm);
  float alpha = clouds * (0.55 + storm*0.35);
  outColor = vec4(ccol, alpha);
}