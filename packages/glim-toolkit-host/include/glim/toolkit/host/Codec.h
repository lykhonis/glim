#pragma once

// Op archive (transport/embed side): encode ops to bytes / decode
// back. Magic + version; decode rejects wrong magic, wrong major,
// truncation. The toolkit owns Op + the recorder; the byte layout
// lives here, next to the host that ships it.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "glim/toolkit/Ops.h"

namespace glim {
namespace toolkit {
namespace host {

namespace detail {

inline void putU8(std::vector<std::uint8_t>& out, std::uint8_t v) { out.push_back(v); }
inline void putU16(std::vector<std::uint8_t>& out, std::uint16_t v) {
  out.push_back(static_cast<std::uint8_t>(v & 0xff));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
}
inline void putU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xff));
}
inline void putI32(std::vector<std::uint8_t>& out, int v) {
  putU32(out, static_cast<std::uint32_t>(v));
}
inline void putF32(std::vector<std::uint8_t>& out, float v) {
  std::uint32_t u = 0;
  std::memcpy(&u, &v, sizeof(u));
  putU32(out, u);
}
inline void putStr(std::vector<std::uint8_t>& out, const std::string& s) {
  putU16(out, static_cast<std::uint16_t>(s.size()));
  for (char c : s) putU8(out, static_cast<std::uint8_t>(c));
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
    return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
  }
  std::uint32_t u32() {
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(u8()) << (8 * i);
    return v;
  }
  int i32() { return static_cast<int>(u32()); }
  float f32() {
    std::uint32_t u = u32();
    float v = 0;
    std::memcpy(&v, &u, sizeof(v));
    return v;
  }
  std::string str() {
    std::uint16_t len = u16();
    std::string s;
    for (std::uint16_t k = 0; k < len; ++k) s.push_back(static_cast<char>(u8()));
    return s;
  }
};

inline void putBox(std::vector<std::uint8_t>& out, const Box& b) {
  putF32(out, b.x);
  putF32(out, b.y);
  putF32(out, b.w);
  putF32(out, b.h);
}

inline Box getBox(Reader& r) {
  Box b;
  b.x = r.f32();
  b.y = r.f32();
  b.w = r.f32();
  b.h = r.f32();
  return b;
}

}  // namespace detail

// Archive: encode ops to bytes / decode back. Magic + version; decode
// rejects wrong magic, wrong major, truncation.
inline void encodeOps(const std::vector<Op>& ops, std::vector<std::uint8_t>& out) {
  const char* magic = "GLIMUI02";
  for (int i = 0; i < 8; ++i) detail::putU8(out, static_cast<std::uint8_t>(magic[i]));
  detail::putU16(out, 1);  // version
  detail::putU32(out, static_cast<std::uint32_t>(ops.size()));
  for (const auto& o : ops) {
    detail::putU8(out, static_cast<std::uint8_t>(o.kind));
    using K = Op::Kind;
    if (o.kind == K::Pop) continue;
    if (o.kind == K::Entry) {
      detail::putStr(out, o.entry.kind);
      detail::putBox(out, o.entry.box);
      detail::putStr(out, o.entry.text);
      detail::putStr(out, o.entry.mark);
      detail::putI32(out, o.entry.radius);
      continue;
    }
    detail::putBox(out, o.box);
    detail::putStr(out, o.mark);
    if (o.kind == K::PushGlass) {
      detail::putI32(out, o.radius);
      detail::putU8(out, o.clear ? 1 : 0);
    } else if (o.kind == K::PushClip) {
      // box + mark only
    } else if (o.kind == K::PushBlend) {
      detail::putU8(out, o.plus ? 1 : 0);
    } else if (o.kind == K::Slot) {
      detail::putU32(out, o.slot);
    } else if (o.kind == K::Fill) {
      detail::putU32(out, o.rgba);
    } else if (o.kind == K::FillRounded) {
      detail::putI32(out, o.radius);
      detail::putU32(out, o.rgba);
    } else if (o.kind == K::FillGradient) {
      detail::putI32(out, o.radius);
      detail::putU32(out, o.rgba);
      detail::putU32(out, o.rgba2);
    } else if (o.kind == K::Stroke) {
      detail::putI32(out, o.radius);
      detail::putF32(out, o.width);
      detail::putU32(out, o.rgba);
    } else if (o.kind == K::Text) {
      detail::putStr(out, o.text);
      detail::putU32(out, o.rgba);
      detail::putF32(out, o.size);
    } else if (o.kind == K::Image) {
      detail::putU32(out, o.image);
      detail::putU8(out, o.foreign ? 1 : 0);
    }
  }
}

inline bool decodeOps(const std::uint8_t* data, std::size_t size, std::vector<Op>& out) {
  detail::Reader r{data, size};
  const char* magic = "GLIMUI02";
  for (int i = 0; i < 8; ++i)
    if (r.u8() != static_cast<std::uint8_t>(magic[i])) return false;
  if (r.u16() != 1) return false;
  std::uint32_t count = r.u32();
  if (!r.ok) return false;
  out.clear();
  out.reserve(count);
  using K = Op::Kind;
  for (std::uint32_t i = 0; i < count; ++i) {
    Op o;
    o.kind = static_cast<K>(r.u8());
    if (o.kind == K::Pop) {
      if (!r.ok) return false;
      out.push_back(o);
      continue;
    }
    if (o.kind == K::Entry) {
      o.entry.kind = r.str();
      o.entry.box = detail::getBox(r);
      o.entry.text = r.str();
      o.entry.mark = r.str();
      o.entry.radius = r.i32();
      if (!r.ok) return false;
      out.push_back(o);
      continue;
    }
    o.box = detail::getBox(r);
    o.mark = r.str();
    if (o.kind == K::PushGlass) {
      o.radius = r.i32();
      o.clear = r.u8() != 0;
    } else if (o.kind == K::PushClip) {
      // box + mark only
    } else if (o.kind == K::PushBlend) {
      o.plus = r.u8() != 0;
    } else if (o.kind == K::Slot) {
      o.slot = r.u32();
    } else if (o.kind == K::Fill) {
      o.rgba = r.u32();
    } else if (o.kind == K::FillRounded) {
      o.radius = r.i32();
      o.rgba = r.u32();
    } else if (o.kind == K::FillGradient) {
      o.radius = r.i32();
      o.rgba = r.u32();
      o.rgba2 = r.u32();
    } else if (o.kind == K::Stroke) {
      o.radius = r.i32();
      o.width = r.f32();
      o.rgba = r.u32();
    } else if (o.kind == K::Text) {
      o.text = r.str();
      o.rgba = r.u32();
      o.size = r.f32();
    } else if (o.kind == K::Image) {
      o.image = r.u32();
      o.foreign = r.u8() != 0;
    } else {
      return false;
    }
    if (!r.ok) return false;
    out.push_back(o);
  }
  return r.ok && r.n == 0;
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
