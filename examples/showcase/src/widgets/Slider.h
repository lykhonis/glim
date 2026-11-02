#pragma once

#include <algorithm>
#include <utility>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Theme.h"

namespace glim {
namespace toolkit {

// Slider: handleAt maps x to value; arrows nudge when focused.
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
  // Track + fill + thumb boxes. Fill runs to the THUMB CENTER, not
  // box.w * v: the thumb travels box.w - kThumb, so a full-width fill
  // would lead the thumb by up to kThumb/2 at the ends. Progress
  // (thumb-less) keeps full-width fill; the two agree at 0.5.
  void geometry(const Box& box, Box& track, Box& fill, Box& thumb)
      const {
    float v = clamp01(*value);
    float ty = box.y + (box.h - 6.0f) * 0.5f;
    track = {box.x, ty, box.w, 6.0f};
    float cx = box.x + kThumb * 0.5f + v * (box.w - kThumb);
    fill = {box.x, ty, 0, 6.0f};
    if (cx > box.x + 0.5f) fill = {box.x, ty, cx - box.x, 6.0f};
    thumb = {
        cx - kThumb * 0.5f,
        box.y + (box.h - kThumb) * 0.5f,
        kThumb,
        kThumb
    };
  }
  void record(const Box& box, Rec& rec) const {
    Box track, fill, thumb;
    geometry(box, track, fill, thumb);
    rec.push(kKind, track, {}, semantic, 3);
    if (fill.w > 0) rec.push(kKindFill, fill, {}, semantic, 3);
    rec.push(kKindThumb, thumb, {}, semantic, 10);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    const Theme& t = theme(e);
    Box track, fill, thumb;
    geometry(box, track, fill, thumb);
    sink.fillGradient(track, 3, t.trackTop, t.trackBottom, semantic);
    if (fill.w > 0.5f) {
      sink.fillGradient(fill, 3, t.accentTop, t.accentBottom, semantic);
      // Gloss stops a thumb radius before the fill cap so it never
      // slides under the thumb.
      const float end = fill.x + fill.w - kThumb * 0.5f;
      if (end > fill.x + 4.0f) {
        Box sheen{
            detail::snapHalf(fill.x + 2.0f),
            detail::snapHalf(fill.y + 1.0f),
            end - fill.x - 2.0f,
            1.0f
        };
        sink.fillRounded(sheen, 1, t.glossLine, semantic);
      }
    }
    Box shade{thumb.x, thumb.y + 1.0f, thumb.w, thumb.h};
    sink.fillRounded(shade, 10, t.thumbRim, semantic);
    sink.fillGradient(thumb, 10, t.thumbTop, t.thumbBottom, semantic);
    sink.stroke(thumb, 10, 1.0f, t.thumbRim, semantic);
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
