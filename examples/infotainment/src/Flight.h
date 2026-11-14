#pragma once

// Simulated flight along the city route: one clock in, every cluster
// readout out. Pure; t = 0 is the reference frame (paQys.jpg values),
// so headless goldens stay fixed while the live app advances t each
// vsync. Readouts keep their digit counts (ink boxes are fitted per
// string).

#include <cmath>

namespace glim {
namespace toolkit {

struct Flight {
  float travel = 0.f;  // route distance covered, city units
  int speedKmh = 178;
  // Continuous readout position: floor() is the shown value, the
  // fraction drives the digit roll toward the next one.
  float speedRoll = 178.5f;
  float needleKmh = 218.4f;
  float distanceKm = 12.4f;  // to waypoint, 10.0..12.4
  int headingDeg = 128;
  int altM = 285;
  float vsiRate = 0.23f;  // -1..1 -> -10..+10 m/s
  // Flight plan and pack: constant over the route.
  int targetKmh = 180;
  int targetAltM = 300;
  float batteryLevel = 0.72f;
  int rangeKm = 412;
};

inline Flight flightAt(float t) {
  constexpr float kUnitsPerSec = 70.f;
  constexpr float kLegKm = 2.4f;
  Flight d;
  d.travel = t * kUnitsPerSec;
  const float speed =
    178.f + 5.f * std::sin(t * 0.37f) + 2.f * std::sin(t * 1.3f);
  d.speedKmh = static_cast<int>(std::lround(speed));
  d.speedRoll = speed + 0.5f;
  d.needleKmh = 218.4f + (speed - 178.f);
  // Countdown loops over one leg so the readout keeps "dd.d".
  const float covered = std::fmod(t * 178.f / 3600.f, kLegKm);
  d.distanceKm = std::round((12.4f - covered) * 10.f) / 10.f;
  d.headingDeg =
    128 + static_cast<int>(std::lround(4.f * std::sin(t * 0.21f)));
  d.altM =
    285 + static_cast<int>(std::lround(6.f * std::sin(t * 0.17f)));
  d.vsiRate = 0.23f + 0.12f * std::sin(t * 0.5f);
  return d;
}

}  // namespace toolkit
}  // namespace glim
