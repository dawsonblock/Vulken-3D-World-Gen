#ifndef WEATHER_SET
#define WEATHER_SET 0
#endif
#ifndef WEATHER_BINDING
#define WEATHER_BINDING 6
#endif

layout(set = WEATHER_SET, binding = WEATHER_BINDING) uniform WeatherUBO {
  vec3  windDir;        float windSpeed;
  uint  state;          float cloudCoverage;
  float cloudDensity;   float timeOfDay;     // 0..1
  float turbidity;      float sunCosZenith;
  float precipRate;     float wetRoughness;
  float wetDarkening;   float snowThresholdDot;
  vec3  snowAlbedo;     float fogDensity;   // NEW: e.g. 0..0.04
  vec3  sunDir;         float lightning;    // NEW: unit dir, 0..1 flash
} uWeather;

// States
const uint WX_CLEAR=0u, WX_CLOUDY=1u, WX_RAIN=2u, WX_SNOW=3u, WX_STORM=4u, WX_FOG=5u;