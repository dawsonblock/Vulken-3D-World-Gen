#include "weather_system.hpp"
#include <yaml-cpp/yaml.h>
#include <glm/geometric.hpp>
#include <cmath>

namespace voxelvk {

static glm::vec3 fromDirDeg(float deg){
  float rad = glm::radians(deg);
  return glm::normalize(glm::vec3{-std::sin(rad), 0.0f, -std::cos(rad)});
}

static glm::vec3 sunDirFromElevAz(float elevDeg, float azDeg){
  float el = glm::radians(elevDeg), az = glm::radians(azDeg);
  float y = std::sin(el);
  float r = std::cos(el);
  return glm::normalize(glm::vec3(std::cos(az)*r, y, std::sin(az)*r));
}

void WeatherSystem::loadFromYaml(const std::string& path){
  YAML::Node y = YAML::LoadFile(path);
  auto s = y["state"].as<std::string>("CLEAR");
  if(s=="CLEAR") P_.state=WeatherState::CLEAR;
  else if(s=="CLOUDY") P_.state=WeatherState::CLOUDY;
  else if(s=="RAIN") P_.state=WeatherState::RAIN;
  else if(s=="SNOW") P_.state=WeatherState::SNOW;
  else if(s=="STORM") P_.state=WeatherState::STORM;
  else if(s=="FOG") P_.state=WeatherState::FOG;

  auto w = y["wind"];
  if(w){
    P_.windSpeed = w["speed"].as<float>(P_.windSpeed);
    P_.windDirDeg = w["direction_deg"].as<float>(P_.windDirDeg);
    P_.gustiness = w["gustiness"].as<float>(P_.gustiness);
  }
  auto pr = y["precip"];
  if(pr){
    P_.precipRate = pr["rate"].as<float>(P_.precipRate);
  }
  auto c = y["clouds"];
  if(c){
    P_.cloudCoverage = c["coverage"].as<float>(P_.cloudCoverage);
    P_.cloudSpeedKmh = c["speed"].as<float>(P_.cloudSpeedKmh);
    P_.cloudDensity = c["density"].as<float>(P_.cloudDensity);
  }
  auto sky = y["sky"];
  if(sky){
    P_.turbidity = sky["turbidity"].as<float>(P_.turbidity);
    P_.sunElevDeg = sky["sun_elevation_deg"].as<float>(P_.sunElevDeg);
  }
  auto tt = y["time"];
  if(tt){
    P_.dayNight = tt["enable_day_night"].as<bool>(P_.dayNight);
    P_.daySeconds = tt["day_seconds"].as<float>(P_.daySeconds);
  }
  auto m = y["material"];
  if(m){
    P_.wetRoughness = m["wet_roughness"].as<float>(P_.wetRoughness);
    P_.wetDarkening = m["wet_darkening"].as<float>(P_.wetDarkening);
    P_.snowThresholdDot = m["snow_threshold_dot"].as<float>(P_.snowThresholdDot);
    auto sa = m["snow_albedo"];
    if(sa && sa.IsSequence() && sa.size()==3){
      P_.snowAlbedo = { sa[0].as<float>(), sa[1].as<float>(), sa[2].as<float>() };
    }
  }
  
  // New fog and sun fields
  auto fog = y["fog"];
  if(fog){ 
    P_.fogDensity = fog["density"].as<float>(P_.fogDensity); 
  }
  auto sun = y["sun"]; 
  if(sun){ 
    P_.sunAzimuthDeg = sun["azimuth_deg"].as<float>(P_.sunAzimuthDeg); 
  }
  
  updateDerived_();
}

void WeatherSystem::setState(WeatherState s){ P_.state = s; }
void WeatherSystem::setPrecipRate(float r){ P_.precipRate = r; }
void WeatherSystem::setCloudCoverage(float c){ P_.cloudCoverage = std::clamp(c,0.f,1.f); }
void WeatherSystem::setWind(float sp,float dir,float g){ P_.windSpeed=sp; P_.windDirDeg=dir; P_.gustiness=g; updateDerived_(); }
void WeatherSystem::setFog(float d){ P_.fogDensity = std::max(0.f, d); }
void WeatherSystem::setSunAzimuth(float deg){ P_.sunAzimuthDeg = deg; }

void WeatherSystem::tick(double dt){
  tSeconds_ += dt;
  gustPhase_ += float(dt) * (0.2f + 0.8f*P_.gustiness); // slow gust
  if(P_.dayNight){
    // advance sun elevation sinusoidally
    float phase = float(std::fmod(tSeconds_, P_.daySeconds) / P_.daySeconds);
    P_.sunElevDeg = std::max(1.0f, 180.f * std::sin(phase*6.2831853f) * 0.5f + 45.f);
  }
}

void WeatherSystem::updateDerived_(){ 
  windDirUnit_ = fromDirDeg(P_.windDirDeg); 
}

WeatherUBO WeatherSystem::getUBO() const{
  WeatherUBO u{};
  // Wind with soft gust modulation
  float gust = 1.0f + 0.3f*P_.gustiness*std::sin(gustPhase_*1.7f);
  u.windDir   = windDirUnit_;
  u.windSpeed = P_.windSpeed * gust;
  u.state     = static_cast<uint32_t>(P_.state);
  u.cloudCoverage = P_.cloudCoverage;
  u.cloudDensity  = P_.cloudDensity;
  float phase = P_.dayNight ? float(std::fmod(tSeconds_, P_.daySeconds)/P_.daySeconds) : (P_.sunElevDeg/90.f);
  u.timeOfDay = phase;
  float zenith = glm::radians(90.0f - P_.sunElevDeg);
  u.sunCosZenith = std::cos(zenith);
  u.turbidity = P_.turbidity;
  // default precip by state if zero
  float stateDefault = 0.0f;
  if(P_.state==WeatherState::RAIN) stateDefault = 5.0f;
  if(P_.state==WeatherState::SNOW) stateDefault = 2.0f;
  if(P_.state==WeatherState::STORM) stateDefault = 20.0f;
  u.precipRate = (P_.precipRate>0.0f)?P_.precipRate:stateDefault;
  u.wetRoughness = P_.wetRoughness;
  u.wetDarkening = P_.wetDarkening;
  u.snowThresholdDot = P_.snowThresholdDot;
  u.snowAlbedo = P_.snowAlbedo;
  
  // New fields
  u.fogDensity = P_.fogDensity;
  // sunDir
  glm::vec3 sdir = sunDirFromElevAz(P_.sunElevDeg, P_.sunAzimuthDeg);
  u.sunDir = sdir;
  // lightning intensity is supplied by Lightning system (set 0 here; engine will overwrite)
  u.lightning = 0.0f;
  
  return u;
}

} // namespace voxelvk