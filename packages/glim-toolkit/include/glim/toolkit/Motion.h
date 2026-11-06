#pragma once

#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

// Run time in seconds; every motion sample takes `now` here.
struct Clock {
  float now = 0.0f;
  void advance(float dt) {
    if (dt > 0.0f) now += dt;
  }
};

enum class Ease { Linear, Out, InOut };

inline float easeAt(Ease e, float t) {
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  switch (e) {
    case Ease::Linear:
      return t;
    case Ease::Out: {
      const float u = 1.0f - t;
      return 1.0f - u * u * u;
    }
    case Ease::InOut:
      if (t < 0.5f) return 4.0f * t * t * t;
      {
        const float u = -2.0f * t + 2.0f;
        return 1.0f - u * u * u * 0.5f;
      }
  }
  return t;
}

// Pure value motion over Clock time; zero duration snaps to `to`.
struct Tween {
  float from = 0.0f;
  float to = 0.0f;
  float start = 0.0f;
  float dur = 0.0f;
  Ease ease = Ease::Out;
  float at(float now) const {
    if (dur <= 0.0f) return to;
    const float t = (now - start) / dur;
    if (t <= 0.0f) return from;
    if (t >= 1.0f) return to;
    return from + (to - from) * easeAt(ease, t);
  }
  bool done(float now) const { return dur <= 0.0f || now >= start + dur; }
};

// Release at p0 with v0 coasts v0 * tau, then settles; dur matches initial slope to v0.
// Zero-velocity and edge-pinned releases stay put.
inline Tween flingTo(float p0, float v0, float lo, float hi, float now, float tau = 0.15f) {
  const float to = detail::clampF(p0 + v0 * tau, lo, hi);
  return Tween{p0, to, now, 3.0f * tau, Ease::Out};
}

// Animation description per value; `set(t)` uses the latest spec, `set(t, spec)` overrides once.
using EaseFn = float (*)(float);
inline float easeLinear(float t) { return easeAt(Ease::Linear, t); }
inline float easeOut(float t) { return easeAt(Ease::Out, t); }
inline float easeInOut(float t) { return easeAt(Ease::InOut, t); }

struct AnimSpec {
  float dur = 0.22f;
  float delay = 0.0f;
  EaseFn ease = easeOut;
  // Spring: semi-implicit Euler; dur/ease ignored, delay still applies; requires damping > 0.
  bool spring = false;
  float stiffness = 170.0f;
  float damping = 22.0f;
};

}  // namespace toolkit
}  // namespace glim
