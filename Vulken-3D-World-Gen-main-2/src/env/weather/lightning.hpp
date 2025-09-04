#pragma once
#include <random>

namespace voxelvk {

class Lightning {
public:
  void setEnabled(bool on){ enabled_=on; }
  void tick(float dt, bool stormState){
    if(!enabled_){ intensity_=0.f; return; }
    timer_ -= dt;
    if(timer_<=0.f && stormState){
      // strike
      intensity_ = 1.f;
      decay_ = 6.0f;                   // fast decay factor
      // next strike in 1.5..6.0 s
      timer_ = dist_(rng_)*4.5f + 1.5f;
    }
    // decay curve
    if(intensity_>0.f){
      intensity_ = std::max(0.f, intensity_ - decay_*dt);
    }
  }
  float intensity() const { return intensity_; }
private:
  bool enabled_ = true;
  float intensity_ = 0.f;
  float timer_ = 2.0f;
  float decay_ = 6.0f;
  std::mt19937 rng_{1337};
  std::uniform_real_distribution<float> dist_{0.f,1.f};
};

} // namespace voxelvk