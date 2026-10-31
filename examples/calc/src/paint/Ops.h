#pragma once

// Serializable UI recording: the same ops the bridge paints, as plain
// data. This is the streaming seam — record locally, encode to bytes,
// ship over the network, decode remotely, replay into a ContextSink.
// Boundary is after place (boxes are concrete), before compose (remote
// owns grouping/sandbox/frame). Actions and State never cross; the
// remote is a pure renderer. Foreign ids cross as ids against a store
// snapshot — remote remapping is parked (T4 tests replay locally).

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "glim/toolkit/Layout.h"
#include "paint/VideoFeed.h"

namespace glim {
namespace toolkit {
namespace paint {

struct UiOp {
  enum class Kind : std::uint8_t {
    Fill,
    FillRounded,
    Text,
    Blit,
    PushGlass,
    PushClip,
    Pop,
    Slot,
    Stroke,
  };
  Kind kind = Kind::Fill;
  Box box{};
  std::uint32_t rgba = 0;
  int radius = 0;
  std::string text;
  std::uint32_t imageId = 0;
  bool foreign = false;
  bool clear = false;  // PushGlass variant
  float width = 0;     // Stroke width
  std::uint32_t slot = 0;
  bool operator==(const UiOp& o) const {
    return kind == o.kind && box.x == o.box.x && box.y == o.box.y &&
           box.w == o.box.w && box.h == o.box.h && rgba == o.rgba &&
           radius == o.radius && text == o.text &&
           imageId == o.imageId && foreign == o.foreign &&
           clear == o.clear && width == o.width && slot == o.slot;
  }
  bool operator!=(const UiOp& o) const { return !(*this == o); }
};

// Inspector sink: records ops instead of painting. Feeds structure
// goldens (order asserts) and the archive below. Video resolves against
// a preset feed id — production id resolution happens in ContextSink.
struct OpsRecorder {
  std::vector<UiOp> ops;
  void fill(const Box& b, std::uint32_t rgba) {
    UiOp o;
    o.kind = UiOp::Kind::Fill;
    o.box = b;
    o.rgba = rgba;
    ops.push_back(o);
  }
  void fillRounded(const Box& b, int radius, std::uint32_t rgba) {
    UiOp o;
    o.kind = UiOp::Kind::FillRounded;
    o.box = b;
    o.rgba = rgba;
    o.radius = radius;
    ops.push_back(o);
  }
  void text(
      float x, float y, const std::string& s, std::uint32_t rgba
  ) {
    UiOp o;
    o.kind = UiOp::Kind::Text;
    o.box = {x, y, 0, 0};
    o.text = s;
    o.rgba = rgba;
    ops.push_back(o);
  }
  void blit(const Box& b, std::uint32_t imageId, bool foreign) {
    UiOp o;
    o.kind = UiOp::Kind::Blit;
    o.box = b;
    o.imageId = imageId;
    o.foreign = foreign;
    ops.push_back(o);
  }
  void blitVideo(const Box& b, VideoFeed& feed) {
    blit(b, feed.id != 0 ? feed.id : 7u, feed.foreign);
  }
  void pushGlass(const Box& b, int radius, bool clear) {
    UiOp o;
    o.kind = UiOp::Kind::PushGlass;
    o.box = b;
    o.radius = radius;
    o.clear = clear;
    ops.push_back(o);
  }
  void pushClip(const Box& b) {
    UiOp o;
    o.kind = UiOp::Kind::PushClip;
    o.box = b;
    ops.push_back(o);
  }
  void pop() {
    UiOp o;
    o.kind = UiOp::Kind::Pop;
    ops.push_back(o);
  }
  void slot(const Box& b, std::uint32_t id) {
    UiOp o;
    o.kind = UiOp::Kind::Slot;
    o.box = b;
    o.slot = id;
    ops.push_back(o);
  }
  void stroke(
      const Box& b, int radius, float width, std::uint32_t rgba
  ) {
    UiOp o;
    o.kind = UiOp::Kind::Stroke;
    o.box = b;
    o.radius = radius;
    o.width = width;
    o.rgba = rgba;
    ops.push_back(o);
  }
};

namespace detail {

inline void putU8(std::vector<std::uint8_t>& out, std::uint8_t v) {
  out.push_back(v);
}
inline void putU16(std::vector<std::uint8_t>& out, std::uint16_t v) {
  out.push_back(static_cast<std::uint8_t>(v & 0xff));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
}
inline void putU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  for (int i = 0; i < 4; ++i)
    out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xff));
}
inline void putI32(std::vector<std::uint8_t>& out, int v) {
  putU32(out, static_cast<std::uint32_t>(v));
}
inline void putF32(std::vector<std::uint8_t>& out, float v) {
  std::uint32_t u = 0;
  std::memcpy(&u, &v, sizeof(u));
  putU32(out, u);
}

struct Reader {
  const std::uint8_t* p;
  std::size_t n;
  bool ok = true;
  std::uint8_t u8() {
    if (n < 1) {
      ok = false;
      return 0;
    }
    std::uint8_t v = *p;
    ++p;
    --n;
    return v;
  }
  std::uint16_t u16() {
    std::uint8_t lo = u8(), hi = u8();
    return static_cast<std::uint16_t>(
        lo | (static_cast<std::uint16_t>(hi) << 8)
    );
  }
  std::uint32_t u32() {
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
      v |= static_cast<std::uint32_t>(u8()) << (8 * i);
    return v;
  }
  int i32() { return static_cast<int>(u32()); }
  float f32() {
    std::uint32_t u = u32();
    float v = 0;
    std::memcpy(&v, &u, sizeof(v));
    return v;
  }
};

}  // namespace detail

// ArchiveSink core: encode ops to bytes / decode back. Magic + version;
// decode rejects wrong magic, wrong major, or truncation.
inline void encodeOps(
    const std::vector<UiOp>& ops, std::vector<std::uint8_t>& out
) {
  const char* magic = "GLIMUI01";
  for (int i = 0; i < 8; ++i)
    detail::putU8(out, static_cast<std::uint8_t>(magic[i]));
  detail::putU16(out, 1);  // version
  detail::putU32(out, static_cast<std::uint32_t>(ops.size()));
  for (const auto& o : ops) {
    detail::putU8(out, static_cast<std::uint8_t>(o.kind));
    using K = UiOp::Kind;
    if (o.kind == K::Pop) continue;
    detail::putF32(out, o.box.x);
    detail::putF32(out, o.box.y);
    if (o.kind == K::PushClip) {
      detail::putF32(out, o.box.w);
      detail::putF32(out, o.box.h);
      continue;
    }
    if (o.kind == K::Text) {
      detail::putU32(out, o.rgba);
      detail::putU16(out, static_cast<std::uint16_t>(o.text.size()));
      for (char c : o.text)
        detail::putU8(out, static_cast<std::uint8_t>(c));
      continue;
    }
    detail::putF32(out, o.box.w);
    detail::putF32(out, o.box.h);
    if (o.kind == K::Fill) {
      detail::putU32(out, o.rgba);
    } else if (o.kind == K::FillRounded) {
      detail::putI32(out, o.radius);
      detail::putU32(out, o.rgba);
    } else if (o.kind == K::Blit) {
      detail::putU32(out, o.imageId);
      detail::putU8(out, o.foreign ? 1 : 0);
    } else if (o.kind == K::PushGlass) {
      detail::putI32(out, o.radius);
      detail::putU8(out, o.clear ? 1 : 0);
    } else if (o.kind == K::Slot) {
      detail::putU32(out, o.slot);
    } else if (o.kind == K::Stroke) {
      detail::putI32(out, o.radius);
      detail::putF32(out, o.width);
      detail::putU32(out, o.rgba);
    }
  }
}

inline bool decodeOps(
    const std::uint8_t* data, std::size_t size, std::vector<UiOp>& out
) {
  detail::Reader r{data, size};
  const char* magic = "GLIMUI01";
  for (int i = 0; i < 8; ++i)
    if (r.u8() != static_cast<std::uint8_t>(magic[i])) return false;
  if (r.u16() != 1) return false;
  std::uint32_t count = r.u32();
  if (!r.ok) return false;
  out.clear();
  out.reserve(count);
  using K = UiOp::Kind;
  for (std::uint32_t i = 0; i < count; ++i) {
    UiOp o;
    o.kind = static_cast<K>(r.u8());
    if (o.kind == K::Pop) {
      if (!r.ok) return false;
      out.push_back(o);
      continue;
    }
    o.box.x = r.f32();
    o.box.y = r.f32();
    if (o.kind == K::PushClip) {
      o.box.w = r.f32();
      o.box.h = r.f32();
      if (!r.ok) return false;
      out.push_back(o);
      continue;
    }
    if (o.kind == K::Text) {
      o.rgba = r.u32();
      std::uint16_t len = r.u16();
      o.text.clear();
      for (std::uint16_t k = 0; k < len; ++k)
        o.text.push_back(static_cast<char>(r.u8()));
      if (!r.ok) return false;
      out.push_back(o);
      continue;
    }
    o.box.w = r.f32();
    o.box.h = r.f32();
    if (o.kind == K::Fill) {
      o.rgba = r.u32();
    } else if (o.kind == K::FillRounded) {
      o.radius = r.i32();
      o.rgba = r.u32();
    } else if (o.kind == K::Blit) {
      o.imageId = r.u32();
      o.foreign = r.u8() != 0;
    } else if (o.kind == K::PushGlass) {
      o.radius = r.i32();
      o.clear = r.u8() != 0;
    } else if (o.kind == K::Slot) {
      o.slot = r.u32();
    } else if (o.kind == K::Stroke) {
      o.radius = r.i32();
      o.width = r.f32();
      o.rgba = r.u32();
    } else {
      return false;
    }
    if (!r.ok) return false;
    out.push_back(o);
  }
  return r.ok && r.n == 0;
}

}  // namespace paint
}  // namespace toolkit
}  // namespace glim
