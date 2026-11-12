#include <glim/toolkit/host/Power.h>

#include <algorithm>

namespace glim {
namespace toolkit {
namespace host {

float targetFps(const PowerPolicy& policy) {
  if (!policy.lowPower || !policy.lowPower()) return policy.maxFps;
  return policy.maxFps > 0.f ? std::min(policy.maxFps, policy.lowPowerFps) : policy.lowPowerFps;
}

bool FramePacer::admit(double seconds, float fps) {
  if (fps > 0.f && seconds - last_ < 0.9 / fps) return false;
  last_ = seconds;
  return true;
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
