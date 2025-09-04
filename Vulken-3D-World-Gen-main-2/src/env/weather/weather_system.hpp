#pragma once
#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>

namespace voxelvk {

enum class WeatherState : uint32_t { CLEAR, CLOUDY, RAIN, SNOW, STORM, FOG };

struct WeatherParams {
  WeatherState state = WeatherState::CLEAR;
  float windSpeed = 6.0f;          // m/s
  float windDirDeg = 245.0f;       // meteo degrees
  float gustiness = 0.35f;         // 0..1
  float precipRate = 0.0f;         // mm/hr
  float cloudCoverage = 0.25f;     // 0..1
  float cloudSpeedKmh = 20.0f;
  float cloudDensity = 0.6f;
  float turbidity = 2.0f;          // sky haze
  float sunElevDeg = 45.0f;
  bool  dayNight = true;
  float daySeconds = 720.0f;
  // material responses
  float wetRoughness = 0.08f;
  float wetDarkening = 0.85f;
  float snowThresholdDot = 0.65f;
  glm::vec3 snowAlbedo = {0.85f, 0.88f, 0.92f};
  // new fields
  float fogDensity = 0.0f;    // 0..~0.04 nice range
  float sunAzimuthDeg = 90.f; // E->W
};

struct WeatherUBO {
  alignas(16) glm::vec3 windDir;   // normalized
  float windSpeed;
  alignas(4) uint32_t state;       // WeatherState
  float cloudCoverage;
  float cloudDensity;
  float timeOfDay;                 // 0..1
  float turbidity;
  float sunCosZenith;              // cos(zenith)
  float precipRate;                // mm/hr
  float wetRoughness;
  float wetDarkening;
  float snowThresholdDot;
  alignas(16) glm::vec3 snowAlbedo;
  float fogDensity;                // NEW: e.g. 0..0.04
  alignas(16) glm::vec3 sunDir;    // NEW: unit dir
  float lightning;                 // NEW: 0..1 flash
};

class WeatherSystem {
public:
  void loadFromYaml(const std::string& path);
  void setState(WeatherState s);
  void setPrecipRate(float mmph);
  void setCloudCoverage(float c);
  void setWind(float speed, float dirDeg, float gustiness);
  void setFog(float density);
  void setSunAzimuth(float deg);
  void tick(double dtSeconds);          // advance sim clock
  WeatherUBO getUBO() const;            // pack for GPU

private:
  WeatherParams P_;
  double tSeconds_ = 0.0;
  float gustPhase_ = 0.0f;
  glm::vec3 windDirUnit_ = { -0.73f, 0.0f, -0.68f };

  void updateDerived_();
};

} // namespace voxelvk