#pragma once

// Serializable UI recording. Elements paint compositor primitives;
// PushRotate/FillRadial are first-class ops so custom elements get
// GPU rotation + radial light without side-channel drawing.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "glim/toolkit/Layout.h"

namespace glim {
namespace toolkit {

struct Op {
  enum class Kind : std::uint8_t {
    Entry,
    Fill,
    FillRounded,
    FillGradient,
    Stroke,
    Text,
    Image,
    PushGlass,
    PushClip,
    PushBlend,
    PushTransform,
    Pop,
    Slot,
    PushRotate,
    FillRadial,
    FillChamfer,
    StrokeChamfer,
    Arc,
    Polyline,
    PushShadow,
    PushEffect,
    PushOpacity,
  };
  Kind kind = Kind::Pop;
  Rec::Entry entry;  // valid when kind == Entry
  Box box{};
  int radius = 0;
  bool clear = false;         // PushGlass variant
  bool plus = false;          // PushBlend variant (additive light)
  bool foreign = false;       // Image variant
  std::uint8_t edge = 15;     // PushGlass edge mask (toolkit GlassEdge bits)
  std::uint32_t tint = 0;     // PushGlass plate tint (0xRRGGBBAA, 0 = milk)
  std::uint8_t material = 0;  // PushGlass lens preset (toolkit GlassMaterial bits)
  std::uint32_t rgba = 0;
  std::uint32_t rgba2 = 0;  // FillGradient bottom
  std::uint32_t image = 0;  // Image variant
  std::uint32_t slot = 0;
  float width = 0;                // Stroke width / Polyline width / Arc thickness
  float size = 0;                 // Text size
  float condense = 1.f;           // Text x-squeeze (1 = normal)
  float textWeight = 0.f;         // Text double-strike px (0 = regular)
  float dx = 0.f;                 // PushTransform translate
  float dy = 0.f;                 // PushTransform translate
  float angleDeg = 0.f;           // PushRotate clockwise-on-screen
  float rcx = 0.f;                // PushRotate pivot logical px
  float rcy = 0.f;                // PushRotate pivot logical px
  float cut = 0.f;                // FillChamfer / StrokeChamfer diagonal cut / Arc radius
  float thickness = 0.f;          // Arc ring thickness
  float startDeg = 0.f;           // Arc start (clockwise from +x)
  float sweepDeg = 0.f;           // Arc sweep
  float dotPitch = 0.f;           // Polyline dotted pitch (0 = solid)
  float sigma = 0.f;              // PushShadow blur sigma
  float sox = 0.f;                // PushShadow offset x
  float soy = 0.f;                // PushShadow offset y
  float expand = 0.f;             // PushShadow expand px
  std::uint32_t shadowColor = 0;  // PushShadow color
  float casterOpacity = 1.f;      // PushShadow caster fade; the shadow stays
  std::uint32_t effect = 0;       // PushEffect kind (EffectKind bits)
  float e0 = 0.f;                 // PushEffect intensity (Glow alpha mul)
  float opacity = 1.f;            // PushOpacity group fade
  std::vector<float> pts;         // Polyline flat points
  std::string text;               // Text variant
  std::string mark;               // semantic mark for every op
  bool operator==(const Op& o) const {
    return kind == o.kind && entry.kind == o.entry.kind && entry.box.x == o.entry.box.x &&
           entry.box.y == o.entry.box.y && entry.box.w == o.entry.box.w &&
           entry.box.h == o.entry.box.h && entry.text == o.entry.text &&
           entry.mark == o.entry.mark && entry.radius == o.entry.radius && box.x == o.box.x &&
           box.y == o.box.y && box.w == o.box.w && box.h == o.box.h && radius == o.radius &&
           clear == o.clear && plus == o.plus && foreign == o.foreign && edge == o.edge &&
           tint == o.tint && material == o.material && rgba == o.rgba && rgba2 == o.rgba2 &&
           image == o.image && slot == o.slot && width == o.width && size == o.size &&
           condense == o.condense && textWeight == o.textWeight && dx == o.dx && dy == o.dy &&
           angleDeg == o.angleDeg && rcx == o.rcx && rcy == o.rcy && cut == o.cut &&
           thickness == o.thickness && startDeg == o.startDeg && sweepDeg == o.sweepDeg &&
           dotPitch == o.dotPitch && sigma == o.sigma && sox == o.sox && soy == o.soy &&
           expand == o.expand && shadowColor == o.shadowColor && casterOpacity == o.casterOpacity &&
           effect == o.effect && e0 == o.e0 && opacity == o.opacity && pts == o.pts &&
           text == o.text && mark == o.mark;
  }
  bool operator!=(const Op& o) const { return !(*this == o); }
};

inline const char* opKindName(Op::Kind k) {
  static constexpr const char* kNames[] = {
      "Entry",    "Fill",       "FillRounded", "FillGradient", "Stroke",        "Text",
      "Image",    "PushGlass",  "PushClip",    "PushBlend",    "PushTransform", "Pop",
      "Slot",     "PushRotate", "FillRadial",  "FillChamfer",  "StrokeChamfer", "Arc",
      "Polyline", "PushShadow", "PushEffect",  "PushOpacity",
  };
  static_assert(sizeof(kNames) / sizeof(kNames[0]) == static_cast<int>(Op::Kind::PushOpacity) + 1);
  return kNames[static_cast<int>(k)];
}

// Inspector sink: records ops instead of painting.
struct OpsRecorder {
  std::vector<Op> ops;
  template <class F>
  void entry(const Rec::Entry& en, F&) {
    Op o;
    o.kind = Op::Kind::Entry;
    o.entry = en;
    ops.push_back(o);
  }
  void fill(const Box& b, std::uint32_t rgba, const std::string& mark = {}) {
    Op o;
    o.kind = Op::Kind::Fill;
    o.box = b;
    o.rgba = rgba;
    o.mark = mark;
    ops.push_back(o);
  }
  void fillRounded(const Box& b, int radius, std::uint32_t rgba, const std::string& mark = {}) {
    Op o;
    o.kind = Op::Kind::FillRounded;
    o.box = b;
    o.radius = radius;
    o.rgba = rgba;
    o.mark = mark;
    ops.push_back(o);
  }
  void fillGradient(
      const Box& b,
      int radius,
      std::uint32_t top,
      std::uint32_t bottom,
      const std::string& mark = {}
  ) {
    Op o;
    o.kind = Op::Kind::FillGradient;
    o.box = b;
    o.radius = radius;
    o.rgba = top;
    o.rgba2 = bottom;
    o.mark = mark;
    ops.push_back(o);
  }
  void stroke(
      const Box& b, int radius, float width, std::uint32_t rgba, const std::string& mark = {}
  ) {
    Op o;
    o.kind = Op::Kind::Stroke;
    o.box = b;
    o.radius = radius;
    o.width = width;
    o.rgba = rgba;
    o.mark = mark;
    ops.push_back(o);
  }
  void text(
      float x,
      float y,
      const std::string& s,
      std::uint32_t rgba,
      float size,
      const std::string& mark = {}
  ) {
    Op o;
    o.kind = Op::Kind::Text;
    o.box = {x, y, 0, 0};
    o.text = s;
    o.rgba = rgba;
    o.size = size;
    o.mark = mark;
    ops.push_back(o);
  }
  void textStyled(
      float x,
      float y,
      const std::string& s,
      std::uint32_t rgba,
      float size,
      float condense,
      float weight,
      const std::string& mark = {}
  ) {
    Op o;
    o.kind = Op::Kind::Text;
    o.box = {x, y, 0, 0};
    o.text = s;
    o.rgba = rgba;
    o.size = size;
    o.condense = condense;
    o.textWeight = weight;
    o.mark = mark;
    ops.push_back(o);
  }
  void image(const Box& b, std::uint32_t id, bool foreign, const std::string& mark = {}) {
    Op o;
    o.kind = Op::Kind::Image;
    o.box = b;
    o.image = id;
    o.foreign = foreign;
    o.mark = mark;
    ops.push_back(o);
  }
  void pushGlass(
      const Box& b,
      int radius,
      bool clear,
      std::uint8_t edge,
      std::uint32_t tint,
      std::uint8_t material = 0
  ) {
    Op o;
    o.kind = Op::Kind::PushGlass;
    o.box = b;
    o.radius = radius;
    o.clear = clear;
    o.edge = edge;
    o.tint = tint;
    o.material = material;
    ops.push_back(o);
  }
  void pushClip(const Box& b) {
    Op o;
    o.kind = Op::Kind::PushClip;
    o.box = b;
    ops.push_back(o);
  }
  void pushBlend(BlendMode m) {
    Op o;
    o.kind = Op::Kind::PushBlend;
    o.plus = m == BlendMode::Plus;
    ops.push_back(o);
  }
  void pushOpacity(float opacity) {
    Op o;
    o.kind = Op::Kind::PushOpacity;
    o.opacity = opacity;
    ops.push_back(o);
  }
  void pushTransform(float dx, float dy) {
    Op o;
    o.kind = Op::Kind::PushTransform;
    o.dx = dx;
    o.dy = dy;
    ops.push_back(o);
  }
  void pushRotate(float angleDeg, float cx, float cy) {
    Op o;
    o.kind = Op::Kind::PushRotate;
    o.angleDeg = angleDeg;
    o.rcx = cx;
    o.rcy = cy;
    ops.push_back(o);
  }
  void fillRadial(
      const Box& b,
      int radius,
      std::uint32_t center,
      std::uint32_t edge,
      const std::string& mark = {}
  ) {
    Op o;
    o.kind = Op::Kind::FillRadial;
    o.box = b;
    o.radius = radius;
    o.rgba = center;
    o.rgba2 = edge;
    o.mark = mark;
    ops.push_back(o);
  }
  void fillChamfer(const Box& b, float cut, std::uint32_t rgba, const std::string& mark = {}) {
    Op o;
    o.kind = Op::Kind::FillChamfer;
    o.box = b;
    o.cut = cut;
    o.rgba = rgba;
    o.mark = mark;
    ops.push_back(o);
  }
  void strokeChamfer(
      const Box& b, float cut, float width, std::uint32_t rgba, const std::string& mark = {}
  ) {
    Op o;
    o.kind = Op::Kind::StrokeChamfer;
    o.box = b;
    o.cut = cut;
    o.width = width;
    o.rgba = rgba;
    o.mark = mark;
    ops.push_back(o);
  }
  void arc(
      float cx,
      float cy,
      float radius,
      float thickness,
      float startDeg,
      float sweepDeg,
      std::uint32_t rgba,
      const std::string& mark = {}
  ) {
    Op o;
    o.kind = Op::Kind::Arc;
    o.box = {cx, cy, 0, 0};
    o.thickness = thickness;
    o.rgba = rgba;
    o.mark = mark;
    // Pack circle into box.w/radius slot-free fields: reuse cut for radius.
    o.cut = radius;
    o.startDeg = startDeg;
    o.sweepDeg = sweepDeg;
    ops.push_back(o);
  }
  void polyline(
      const float* xy,
      std::size_t n,
      float width,
      float dotPitch,
      std::uint32_t rgba,
      const std::string& mark = {}
  ) {
    Op o;
    o.kind = Op::Kind::Polyline;
    o.width = width;
    o.dotPitch = dotPitch;
    o.rgba = rgba;
    o.mark = mark;
    o.pts.reserve(n * 2);
    for (std::size_t i = 0; i < n * 2; ++i) o.pts.push_back(xy[i]);
    ops.push_back(o);
  }
  void pushShadow(
      float sigma, float ox, float oy, std::uint32_t color, float expand, float opacity = 1.f
  ) {
    Op o;
    o.kind = Op::Kind::PushShadow;
    o.sigma = sigma;
    o.sox = ox;
    o.soy = oy;
    o.shadowColor = color;
    o.expand = expand;
    o.casterOpacity = opacity;
    ops.push_back(o);
  }
  void pushEffect(
      const Box& b, int radius, std::uint32_t kind, std::uint32_t color, float intensity
  ) {
    Op o;
    o.kind = Op::Kind::PushEffect;
    o.box = b;
    o.radius = radius;
    o.effect = kind;
    o.rgba = color;
    o.e0 = intensity;
    ops.push_back(o);
  }
  void pop() {
    Op o;
    o.kind = Op::Kind::Pop;
    ops.push_back(o);
  }
  void slot(const Box& b, std::uint32_t id) {
    Op o;
    o.kind = Op::Kind::Slot;
    o.box = b;
    o.slot = id;
    ops.push_back(o);
  }
};

}  // namespace toolkit
}  // namespace glim
