#include "../common/weather_ubo.glsl"

// Adjust baseColor/roughness for wet and snow overlay. N must be world-space normal.
void apply_weather_to_pbr(in vec3 N, in vec3 up,
                          inout vec3 baseColor, inout float roughness) {
  // Wetness from precip (clamped)
  float wet = clamp(uWeather.precipRate / 20.0, 0.0, 1.0);
  // darken & smooth when wet
  baseColor *= mix(1.0, uWeather.wetDarkening, wet);
  roughness  = mix(roughness, uWeather.wetRoughness, wet * 0.7);

  // Snow overlay for faces pointing upward
  float ndotUp = max(0.0, dot(normalize(N), normalize(up)));
  float snowMask = smoothstep(uWeather.snowThresholdDot, 1.0, ndotUp);
  if(uWeather.state==WX_SNOW || uWeather.state==WX_STORM){
    float snowAmt = clamp(uWeather.precipRate/10.0, 0.0, 1.0) * snowMask;
    baseColor = mix(baseColor, uWeather.snowAlbedo, snowAmt);
    roughness = mix(roughness, 0.4, snowAmt); // snow slightly rough
  }
}