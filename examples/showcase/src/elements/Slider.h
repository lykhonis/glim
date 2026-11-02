#pragma once

#include <algorithm>
#include <utility>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

// Slider (T8/T9, example-local). The first consumer of the core
// `handleAt` hook: taps and pressed-drags map x → value through the
// laid-out box, hover moves are ignored, arrows nudge when focused.
// Track + fill + thumb are entry kinds interpreted by the showcase
// sink — zero codec edits.
struct Slider : WithSemantic {
  static constexpr const char* kKind = "Slide";
  static constexpr const char* kKindFill = "Slide.fill";
  static constexpr const char* kKindThumb = "Slide.thumb";
  static constexpr float kW = 240.0f;
  static constexpr float kH = 28.0f;
  static constexpr float kThumb = 20.0f;
  State<float> value;
  static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
  }
  float at(const Box& box, float x) const {
    if (box.w <= kThumb) return *value;
    return clamp01((x - box.x - kThumb * 0.5f) / (box.w - kThumb));
  }
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(kW, kH, c);
  }
  void record(const Box& box, Rec& rec) const {
    float v = clamp01(*value);
    float ty = box.y + (box.h - 6.0f) * 0.5f;
    rec.push(kKind, {box.x, ty, box.w, 6.0f}, {}, semantic, 3);
    // Fill runs to the THUMB CENTER, not box.w * v: the thumb travels
    // box.w - kThumb, so a full-width fill would lead the thumb by up
    // to kThumb/2 at the ends (the reported "progress lags drag").
    // Progress (thumb-less) keeps full-width fill; the two agree at
    // 0.5 and differ honestly elsewhere.
    float cx = box.x + kThumb * 0.5f + v * (box.w - kThumb);
    if (cx > box.x + 0.5f)
      rec.push(
          kKindFill, {box.x, ty, cx - box.x, 6.0f}, {}, semantic, 3
      );
    rec.push(
        kKindThumb,
        {cx - kThumb * 0.5f,
         box.y + (box.h - kThumb) * 0.5f,
         kThumb,
         kThumb},
        {},
        semantic,
        10
    );
  }
  bool handleAt(const Box& box, const Event& ev) const {
    if (const auto* d = ev.as<PointerDown>()) {
      value = at(box, d->x);
      return true;
    }
    if (const auto* m = ev.as<PointerMove>()) {
      if (!m->pressed) return false;
      float v = at(box, m->x);
      if (v == *value) return false;
      value = v;
      return true;
    }
    if (ev.is<PointerUp>()) return true;
    return false;
  }
  bool handle(const Event& ev) const {
    if (const auto* k = ev.as<KeyPress>()) {
      if (k->key == Key::Left) {
        value = clamp01(*value - 0.1f);
        return true;
      }
      if (k->key == Key::Right) {
        value = clamp01(*value + 0.1f);
        return true;
      }
    }
    if (ev.is<Activate>()) return true;
    return false;
  }
};

template <class... As>
Block slider(State<float> v, As&&... as) {
  Slider s;
  s.value = std::move(v);
  configureTags(s, std::forward<As>(as)...);
  return finish(std::move(s), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
