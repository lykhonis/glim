#pragma once

// Glide: Anim-backed display values for readings that arrive as
// jumps. Each keeps its goal in a cell of its own, so a rebuild in
// mid-flight leaves the curve alone and only a new goal re-targets.

#include <algorithm>
#include <cmath>
#include <string>

#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

// Readings and control positions ease over the same short curve.
inline constexpr AnimSpec kGlideSpec{.dur = 0.24f, .ease = easeOut};

// The displayed value of `goal`, gliding to each new goal.
inline float glide(
    const Scope& scope,
    const std::string& key,
    float goal,
    AnimSpec spec = kGlideSpec
) {
  State<float> last = scope.state(key + "/goal", goal);
  Anim<float> shown(scope, key, goal, spec);
  if (*last != goal) {
    last = goal;
    shown = goal;
  }
  return *shown;
}

// A glide whose ease lengthens with the jump to each new goal: a
// fling's tail lasts as long as it travels while a nudge stays brisk.
// A jump across the whole 0..1 range adds `perRange` seconds.
inline float coast(
    const Scope& scope,
    const std::string& key,
    float goal,
    float perRange = 1.0f
) {
  State<float> last = scope.state(key + "/goal", goal);
  AnimSpec spec = kGlideSpec;
  spec.dur += std::fabs(goal - *last) * perRange;
  return glide(scope, key, goal, spec);
}

// Scroll offset, in points, of a series whose newest point is `head`:
// each new point slides in over one `period` instead of jumping. The
// curve is linear and a touch longer than the period, so back-to-back
// points run as one steady scroll. Capped at `maxLead` points.
inline float slide(
    const Scope& scope,
    const std::string& key,
    long head,
    float period,
    float maxLead
) {
  State<long> seen = scope.state(key + "/head", head);
  Anim<float> offset(
      scope, key, 0.0f, {.dur = period * 1.1f, .ease = easeLinear}
  );
  if (*seen != head) {
    const float lead = head > *seen
                           ? *offset + static_cast<float>(head - *seen)
                           : 0.0f;
    seen = head;
    offset.snap(std::min(lead, maxLead));
    offset = 0.0f;
  }
  return *offset;
}

}  // namespace toolkit
}  // namespace glim
