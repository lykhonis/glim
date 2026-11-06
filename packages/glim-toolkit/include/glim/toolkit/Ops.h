#pragma once

// Serializable UI recording; new elements paint primitives, never new op kinds.
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
    Pop,
    Slot,
  };
  Kind kind = Kind::Pop;
  Rec::Entry entry;  // valid when kind == Entry
  Box box{};
  int radius = 0;
  bool clear = false;      // PushGlass variant
  bool plus = false;       // PushBlend variant (additive light)
  bool foreign = false;    // Image variant
  std::uint8_t edge = 15;  // PushGlass edge mask (toolkit GlassEdge bits)
  std::uint32_t tint = 0;  // PushGlass plate tint (0xRRGGBBAA, 0 = milk)
  std::uint32_t rgba = 0;
  std::uint32_t rgba2 = 0;  // FillGradient bottom
  std::uint32_t image = 0;  // Image variant
  std::uint32_t slot = 0;
  float width = 0;   // Stroke width
  float size = 0;    // Text size
  std::string text;  // Text variant
  std::string mark;  // semantic mark for every op
  bool operator==(const Op& o) const {
    return kind == o.kind && entry.kind == o.entry.kind && entry.box.x == o.entry.box.x &&
           entry.box.y == o.entry.box.y && entry.box.w == o.entry.box.w &&
           entry.box.h == o.entry.box.h && entry.text == o.entry.text &&
           entry.mark == o.entry.mark && entry.radius == o.entry.radius && box.x == o.box.x &&
           box.y == o.box.y && box.w == o.box.w && box.h == o.box.h && radius == o.radius &&
           clear == o.clear && plus == o.plus && foreign == o.foreign && edge == o.edge &&
           tint == o.tint && rgba == o.rgba && rgba2 == o.rgba2 && image == o.image &&
           slot == o.slot && width == o.width && size == o.size && text == o.text && mark == o.mark;
  }
  bool operator!=(const Op& o) const { return !(*this == o); }
};

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
  void image(const Box& b, std::uint32_t id, bool foreign, const std::string& mark = {}) {
    Op o;
    o.kind = Op::Kind::Image;
    o.box = b;
    o.image = id;
    o.foreign = foreign;
    o.mark = mark;
    ops.push_back(o);
  }
  void pushGlass(const Box& b, int radius, bool clear, std::uint8_t edge, std::uint32_t tint) {
    Op o;
    o.kind = Op::Kind::PushGlass;
    o.box = b;
    o.radius = radius;
    o.clear = clear;
    o.edge = edge;
    o.tint = tint;
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
