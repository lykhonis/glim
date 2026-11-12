#pragma once

#include <functional>
#include <limits>

namespace glim {
namespace toolkit {
namespace host {

// Frame-rate policy for the host loop. Zero maxFps runs at the display
// rate; lowPower is polled once per vsync, so apps can bridge OS battery
// saver or thermal state without restarting the loop.
struct PowerPolicy {
  float maxFps = 0.f;
  float lowPowerFps = 30.f;
  std::function<bool()> lowPower;
};

// Effective cap for this vsync; zero means uncapped.
float targetFps(const PowerPolicy& policy);

// Admits vsync ticks no faster than the target rate. A tick within 10%
// of the interval counts as due, so a 30 fps cap on a 60 Hz display
// admits every second vsync despite timer jitter.
class FramePacer {
 public:
  bool admit(double seconds, float fps);

 private:
  double last_ = -std::numeric_limits<double>::infinity();
};

}  // namespace host
}  // namespace toolkit
}  // namespace glim
