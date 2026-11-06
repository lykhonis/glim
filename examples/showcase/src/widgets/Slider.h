#pragma once

#include <algorithm>
#include <utility>

#include "Gel.h"
#include "ShowcaseTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Slider : WithSemantic {
  static constexpr const char* kKind = "Slide";
  static constexpr const char* kKindFill = "Slide.fill";
  static constexpr const char* kKindThumb = "Slide.thumb";
  static constexpr float kW = 240.0f;
  static constexpr float kH = 28.0f;
  static constexpr float kThumb = 20.0f;
  State<float> value = {"value", 0.4f};
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
  void geometry(const Box& box, Box& track, Box& fill, Box& thumb)
      const {
    float v = clamp01(*value);
    float ty = box.y + (box.h - 8.0f) * 0.5f;
    track = {box.x, ty, box.w, 8.0f};
    float cx = box.x + kThumb * 0.5f + v * (box.w - kThumb);
    fill = {box.x, ty, 0, 8.0f};
    if (cx > box.x + 0.5f) fill = {box.x, ty, cx - box.x, 8.0f};
    float nx = detail::snapHalf(cx - kThumb * 0.5f);
    float ny = detail::snapHalf(box.y + (box.h - kThumb) * 0.5f);
    nx = std::max(box.x, std::min(nx, box.x + box.w - kThumb));
    ny = std::max(box.y, std::min(ny, box.y + box.h - kThumb));
    thumb = {nx, ny, kThumb, kThumb};
  }
  void record(const Box& box, Rec& rec) const {
    Box track, fill, thumb;
    geometry(box, track, fill, thumb);
    rec.push(kKind, track, {}, {}, 4);
    if (fill.w > 0) rec.push(kKindFill, fill, {}, {}, 4);
    rec.push(kKindThumb, thumb, {}, {}, 10);
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    Box track, fill, thumb;
    geometry(box, track, fill, thumb);
    sink.fillGradient(
        track,
        4,
        themeColor(e, kShowTrackTop, Show::kTrackTop),
        themeColor(e, kShowTrackBottom, Show::kTrackBottom)
    );
    if (fill.w > 0.5f) {
      sink.fillGradient(
          fill,
          4,
          themeColor(e, kShowAccentTop, Show::kAccentTop),
          themeColor(e, kShowAccentBottom, Show::kAccentBottom)
      );
      gelCoat(sink, fill, 4);
    }
    // Thumb: shadow, face, gloss crown, single gray rim.
    Box shade{thumb.x, thumb.y + 2.0f, thumb.w, thumb.h};
    sink.fillRounded(shade, 10, Show::kShadow);
    sink.fillGradient(
        thumb,
        10,
        themeColor(e, kShowThumbTop, Show::kThumbTop),
        themeColor(e, kShowThumbBottom, Show::kThumbBottom)
    );
    knobGloss(sink, thumb);
    sink.stroke(
        thumb, 10, 1.0f, themeColor(e, kShowThumbRim, Show::kThumbRim)
    );
  }
  bool handleAt(const Box& box, const Event& ev) const {
    if (auto d = ev.as<PointerDown>()) {
      value = at(box, d->get().x);
      return true;
    }
    if (auto m = ev.as<PointerMove>()) {
      if (!m->get().pressed) return false;
      float v = at(box, m->get().x);
      if (v == *value) return false;
      value = v;
      return true;
    }
    if (ev.is<PointerUp>()) return true;
    return false;
  }
  bool handle(const Event& ev) const {
    if (auto k = ev.as<KeyPress>()) {
      if (k->get().key == Key::Left) {
        value = clamp01(*value - 0.1f);
        return true;
      }
      if (k->get().key == Key::Right) {
        value = clamp01(*value + 0.1f);
        return true;
      }
    }
    if (ev.is<Activate>()) return true;
    return false;
  }
};

template <class... As>
Block slider(Slider s, As&&... as) {
  configureTags(s, std::forward<As>(as)...);
  return finish(std::move(s), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
