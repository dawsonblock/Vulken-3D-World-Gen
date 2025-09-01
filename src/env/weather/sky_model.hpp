#pragma once
namespace voxelvk {
struct SkyState { float sunElevDeg=45.f; float turbidity=2.f; };
class SkyModel {
public:
  void set(float elevDeg, float turb){ s_.sunElevDeg=elevDeg; s_.turbidity=turb; }
  SkyState get() const { return s_; }
private:
  SkyState s_;
};
}