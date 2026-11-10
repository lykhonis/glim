#pragma once

// FUI primitives for the reference cluster: measured palette, text
// placed by cap box (x, cap top, cap height) with tracking, and
// point-list paths. Geometry in the widgets is measured against
// paQys.jpg with the probe
// (`--probe`), so widgets speak in reference pixels.

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include "../InfoTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {
namespace fui {

// Sampled from the reference (probe `color`).
struct Ref {
  static constexpr std::uint32_t kCard = 0x010b11ff;
  static constexpr std::uint32_t kFrame = 0x34393cff;
  static constexpr std::uint32_t kBracket = 0x2a3036ff;
  static constexpr std::uint32_t kCopper = 0x6b4a33ff;
  static constexpr std::uint32_t kRule = 0x1a232aff;
  static constexpr std::uint32_t kLabel = 0x9da4aaff;
  static constexpr std::uint32_t kValue = 0x86939bff;
  static constexpr std::uint32_t kNumeral = 0xe4e3e1ff;
  static constexpr std::uint32_t kOrange = 0xea882aff;
  static constexpr std::uint32_t kOrangeText = 0xf8780fff;
  static constexpr std::uint32_t kSegment = 0x354042ff;
  static constexpr std::uint32_t kSegmentDim = 0x141d22ff;
};

// Inter metrics as stb scales them: sizePx spans ascent-descent.
inline constexpr float kCapPerSize = 0.6012f;
inline constexpr float kCapTopPerSize = 0.1990f;

enum class Align { Left, Center, Right };

struct Type {
  float cap = 10.f;      // cap height in px
  float track = 0.f;     // extra px after each glyph
  float condense = 1.f;  // horizontal squeeze
  float weight = 0.f;    // double-strike px
  std::uint32_t color = Ref::kLabel;
};

inline float sizeFor(float cap) { return cap / kCapPerSize; }

inline float measure(
    const Engine& e, const std::string& s, const Type& t
) {
  auto m = e.get<Measurer>();
  const float size = sizeFor(t.cap);
  float w = 0.f;
  for (char ch : s) {
    const char one[2] = {ch, 0};
    w += (m ? m->get().measure(one, size) : size * 0.55f) * t.condense;
  }
  return w + t.track * static_cast<float>(s.empty() ? 0 : s.size() - 1);
}

// Draws `s` with its cap box at (x, capTop); x is the left, center or
// right edge.
inline float text(
    AnySink& sink,
    const Engine& e,
    const std::string& s,
    float x,
    float capTop,
    const Type& t,
    Align align = Align::Left
) {
  const float w = measure(e, s, t);
  if (align == Align::Center) x -= w * 0.5f;
  if (align == Align::Right) x -= w;
  const float size = sizeFor(t.cap);
  const float y = capTop - kCapTopPerSize * size;
  if (t.track == 0.f) {
    sink.textStyled(x, y, s, t.color, size, t.condense, t.weight);
    return w;
  }
  auto m = e.get<Measurer>();
  float pen = x;
  for (char ch : s) {
    const std::string one(1, ch);
    sink.textStyled(pen, y, one, t.color, size, t.condense, t.weight);
    pen += (m ? m->get().measure(one.c_str(), size) : size * 0.55f) *
               t.condense +
           t.track;
  }
  return w;
}

// Reference-pixel coordinates mapped onto a laid-out box whose
// reference origin is (refX, refY); identity when the box sits at its
// reference rect.
struct At {
  float dx = 0.f, dy = 0.f;
  At(const Box& b, float refX, float refY)
      : dx(b.x - refX), dy(b.y - refY) {}
  float x(float v) const { return v + dx; }
  float y(float v) const { return v + dy; }
  Box box(float x0, float y0, float w, float h) const {
    return {x(x0), y(y0), w, h};
  }
  std::vector<float> pts(std::initializer_list<float> xy) const {
    std::vector<float> out(xy);
    for (std::size_t i = 0; i < out.size(); ++i)
      out[i] += (i % 2) ? dy : dx;
    return out;
  }
};

// Inclusive ink bounds in reference pixels (probe `bbox`).
struct Ink {
  float x0, y0, x1, y1;
};

// Side bearing per glyph edge as a fraction of size (probe `items`: ink
// sits ~0.05 size inside the advance box on each side).
inline constexpr float kBearingPerSize = 0.05f;
// Numerals sit tighter than tracked labels.
inline constexpr float kNumeralTrack = 0.06f;

// Fits `s` to measured ink: cap height from the box height; condense
// solved so tracking is `trackPerCap` x cap (reference type is narrow
// and tracked), clamped to [0.7, 1.1] with the remainder spread as
// tracking.
struct Fitted {
  Type type;
  float x, capTop;  // pen origin for text()
};

inline Fitted fitLayout(
    const Engine& e,
    const At& at,
    const std::string& s,
    Ink ink,
    std::uint32_t color,
    float trackPerCap = 0.12f,
    float weight = 0.f
) {
  const float cap = ink.y1 - ink.y0 + 1.f;
  const float bearing = kBearingPerSize * sizeFor(cap);
  const float inkW = ink.x1 - ink.x0 + 1.f;
  const float gaps =
      static_cast<float>(s.size() > 1 ? s.size() - 1 : 0);
  Type t{cap, 0.f, 1.f, weight, color};
  const float natural = measure(e, s, t);
  float track = trackPerCap * cap;
  // Advance box = ink + both bearings; bearings scale with condense
  // too.
  float c = (inkW - track * gaps) / (natural - 2.f * bearing);
  c = std::clamp(c, 0.7f, 1.1f);
  t.condense = c;
  const float adv = measure(e, s, t);
  if (gaps > 0) t.track = (inkW + 2.f * bearing * c - adv) / gaps;
  return {t, at.x(ink.x0) - bearing * c, at.y(ink.y0)};
}

inline void fit(
    AnySink& sink,
    const Engine& e,
    const At& at,
    const std::string& s,
    Ink ink,
    std::uint32_t color,
    float trackPerCap = 0.12f,
    float weight = 0.f
) {
  const Fitted f = fitLayout(e, at, s, ink, color, trackPerCap, weight);
  text(sink, e, s, f.x, f.capTop, f.type);
}

inline void path(
    AnySink& sink,
    std::initializer_list<float> xy,
    float width,
    std::uint32_t rgba,
    float dot = 0.f
) {
  const std::vector<float> pts(xy);
  sink.polyline(pts.data(), pts.size() / 2, width, dot, rgba);
}

inline void path(
    AnySink& sink,
    const std::vector<float>& pts,
    float width,
    std::uint32_t rgba,
    float dot = 0.f
) {
  sink.polyline(pts.data(), pts.size() / 2, width, dot, rgba);
}

// Horizontal hairline [x0, x1) at row y.
inline void hline(
    AnySink& sink,
    float x0,
    float x1,
    float y,
    std::uint32_t rgba,
    float h = 1.f
) {
  sink.fill({x0, y, x1 - x0, h}, rgba);
}

}  // namespace fui
}  // namespace toolkit
}  // namespace glim
