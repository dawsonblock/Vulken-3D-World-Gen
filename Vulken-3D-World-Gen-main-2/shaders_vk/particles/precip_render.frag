#version 460
#extension GL_GOOGLE_include_directive : enable
#include "../common/weather_ubo.glsl"
layout(location=0) out vec4 outColor;
void main(){
  // circular sprite fade
  vec2 p = gl_PointCoord*2.0-1.0;
  float r = dot(p,p);
  float alpha = exp(-r*3.0);
  vec3 col = (uWeather.state==WX_SNOW)? vec3(0.95) : vec3(0.6,0.7,0.9);
  outColor = vec4(col, alpha * clamp(uWeather.precipRate/20.0,0.1,1.0));
}