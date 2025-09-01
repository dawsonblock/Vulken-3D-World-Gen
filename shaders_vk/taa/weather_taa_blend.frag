#version 460
#extension GL_GOOGLE_include_directive : enable
#include "../common/weather_ubo.glsl"

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uTAAResult;        // TAA resolved geometry
layout(set = 0, binding = 1) uniform sampler2D uWeatherClouds;    // Weather TRP clouds
layout(set = 0, binding = 2) uniform sampler2D uWeatherPrecip;    // Weather TRP precipitation

layout(push_constant) uniform WeatherTAABlend {
    float weatherBlendRatio;     // How much weather affects TAA (0=separate, 1=full blend)
    float cloudAlphaThreshold;   // Threshold for cloud alpha blending
    float precipAlphaThreshold;  // Threshold for precipitation alpha
} uBlend;

void main() {
    vec2 uv = vUv;
    
    // Sample TAA resolved geometry
    vec3 taaColor = texture(uTAAResult, uv).rgb;
    
    // Sample weather effects (these use separate temporal reprojection)
    vec4 clouds = texture(uWeatherClouds, uv);
    vec4 precip = texture(uWeatherPrecip, uv);
    
    // Preserve weather temporal stability by using separate TRP
    // Only blend based on user-configured ratio
    
    vec3 finalColor = taaColor;
    
    // Composite clouds over TAA result
    if (clouds.a > uBlend.cloudAlphaThreshold) {
        // Weather clouds use their own temporal reprojection (from weather system)
        // Apply clouds with preserved temporal stability
        vec3 cloudContrib = clouds.rgb * clouds.a;
        finalColor = mix(finalColor, cloudContrib, clouds.a);
    }
    
    // Composite precipitation over result
    if (precip.a > uBlend.precipAlphaThreshold) {
        // Weather precipitation uses separate temporal reprojection
        vec3 precipContrib = precip.rgb * precip.a;
        finalColor = mix(finalColor, precipContrib, precip.a * 0.5); // Softer blend for precip
    }
    
    // Optional: Slight TAA influence on weather based on blend ratio
    if (uBlend.weatherBlendRatio > 0.0) {
        // Allow minimal TAA influence on weather edges for integration
        vec3 weatherBlended = mix(clouds.rgb, taaColor, uBlend.weatherBlendRatio * 0.1);
        finalColor = mix(finalColor, weatherBlended, clouds.a * uBlend.weatherBlendRatio);
    }
    
    outColor = vec4(finalColor, 1.0);
}