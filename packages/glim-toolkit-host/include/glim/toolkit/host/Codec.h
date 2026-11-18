#pragma once

// Op archive (transport/embed side): magic + version; decode rejects wrong
// magic, wrong major, truncation. Byte layout lives here, next to the host.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "glim/toolkit/Color.h"
#include "glim/toolkit/Ops.h"
#include "glim/toolkit/Tags.h"

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

inline void encodeOps(const std::vector<Op>& ops, std::vector<std::uint8_t>& out) {
  const char* magic = "GLIMUI02";
  for (int i = 0; i < 8; ++i) detail::putU8(out, static_cast<std::uint8_t>(magic[i]));
  detail::putU16(out, 10);  // v10: clear glass is a material; v9: PushTransform matrix
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
      detail::putU8(out, o.edge);
      detail::putU32(out, o.tint);
      detail::putU8(out, o.material);
      detail::putU8(out, o.glassMerge ? 1 : 0);
    } else if (o.kind == K::PushClip) {
    } else if (o.kind == K::PushBlend) {
      detail::putU8(out, o.plus ? 1 : 0);
    } else if (o.kind == K::PushOpacity) {
      detail::putF32(out, o.opacity);
    } else if (o.kind == K::PushTransform) {
      for (const float f : o.matrix.m) detail::putF32(out, f);
    } else if (o.kind == K::PushFade) {
      for (const float f :
           {o.fade.from.x, o.fade.from.y, o.fade.to.x, o.fade.to.y, o.fade.fromAlpha, o.fade.toAlpha
           })
        detail::putF32(out, f);
    } else if (o.kind == K::FillRadial) {
      detail::putI32(out, o.radius);
      detail::putU32(out, o.rgba);
      detail::putU32(out, o.rgba2);
    } else if (o.kind == K::FillChamfer) {
      detail::putF32(out, o.cut);
      detail::putU32(out, o.rgba);
    } else if (o.kind == K::StrokeChamfer) {
      detail::putF32(out, o.cut);
      detail::putF32(out, o.width);
      detail::putU32(out, o.rgba);
    } else if (o.kind == K::Arc) {
      detail::putF32(out, o.cut);
      detail::putF32(out, o.thickness);
      detail::putF32(out, o.startDeg);
      detail::putF32(out, o.sweepDeg);
      detail::putU32(out, o.rgba);
    } else if (o.kind == K::Polyline) {
      detail::putF32(out, o.width);
      detail::putF32(out, o.dotPitch);
      detail::putU32(out, o.rgba);
      detail::putU32(out, static_cast<std::uint32_t>(o.pts.size()));
      for (float v : o.pts) detail::putF32(out, v);
    } else if (o.kind == K::PushShadow) {
      detail::putF32(out, o.sigma);
      detail::putF32(out, o.sox);
      detail::putF32(out, o.soy);
      detail::putU32(out, o.shadowColor);
      detail::putF32(out, o.expand);
      detail::putF32(out, o.casterOpacity);
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
      detail::putF32(out, o.condense);
      detail::putF32(out, o.textWeight);
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
  const std::uint16_t ver = r.u16();
  if (ver < 1 || ver > 10) return false;
  std::uint32_t count = r.u32();
  if (!r.ok) return false;
  out.clear();
  out.reserve(count);
  using K = Op::Kind;
  constexpr std::uint8_t kLegacyGlow = 20;
  for (std::uint32_t i = 0; i < count; ++i) {
    Op o;
    const std::uint8_t kind = r.u8();
    o.kind = static_cast<K>(kind);
    // Before v9 rotate (13) and tilt (22) had their own kinds.
    const bool legacyRotate = ver < 9 && kind == 13;
    const bool legacyTilt = ver < 9 && kind == 22;
    if (legacyRotate || legacyTilt) o.kind = K::PushTransform;
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
      // Before v10 clear glass was a flag beside the material.
      const bool clear = ver < 10 && r.u8() != 0;
      o.edge = r.u8();
      o.tint = r.u32();
      // Pre-v4 archives carry no material byte: default refractive glass.
      o.material = (ver >= 4 && r.ok) ? r.u8() : 0;
      if (clear && o.material == static_cast<std::uint8_t>(GlassMaterial::Glass))
        o.material = static_cast<std::uint8_t>(GlassMaterial::Clear);
      o.glassMerge = (ver >= 8 && r.ok) ? r.u8() != 0 : true;
    } else if (o.kind == K::PushClip) {
    } else if (o.kind == K::PushBlend) {
      o.plus = r.u8() != 0;
    } else if (o.kind == K::PushOpacity) {
      if (ver < 6) return false;
      o.opacity = r.f32();
    } else if (legacyRotate) {
      TransformTag t{.rotate = r.f32()};
      const float cx = r.f32();
      o.matrix = transformAt(t, {cx, r.f32()}, 1.f);
    } else if (legacyTilt) {
      if (ver < 7) return false;
      TransformTag t;
      t.tilt.x = r.f32();
      t.tilt.y = r.f32();
      t.perspective = r.f32();
      const float s = ver >= 8 ? r.f32() : 1.f;
      t.scale = {s, s};
      const float cx = r.f32();
      o.matrix = transformAt(t, {cx, r.f32()}, 1.f);
    } else if (o.kind == K::PushFade) {
      for (float* f :
           {&o.fade.from.x,
            &o.fade.from.y,
            &o.fade.to.x,
            &o.fade.to.y,
            &o.fade.fromAlpha,
            &o.fade.toAlpha})
        *f = r.f32();
    } else if (o.kind == K::PushTransform) {
      if (ver >= 9) {
        for (float& f : o.matrix.m) f = r.f32();
      } else {
        const float dx = r.f32();
        o.matrix = Mat4::translate(dx, r.f32());
      }
    } else if (o.kind == K::FillRadial) {
      o.radius = r.i32();
      o.rgba = r.u32();
      o.rgba2 = r.u32();
    } else if (o.kind == K::FillChamfer) {
      o.cut = r.f32();
      o.rgba = r.u32();
    } else if (o.kind == K::StrokeChamfer) {
      o.cut = r.f32();
      o.width = r.f32();
      o.rgba = r.u32();
    } else if (o.kind == K::Arc) {
      o.cut = r.f32();
      o.thickness = r.f32();
      o.startDeg = r.f32();
      o.sweepDeg = r.f32();
      o.rgba = r.u32();
    } else if (o.kind == K::Polyline) {
      o.width = r.f32();
      o.dotPitch = r.f32();
      o.rgba = r.u32();
      std::uint32_t n = r.u32();
      o.pts.clear();
      o.pts.reserve(n);
      for (std::uint32_t i = 0; i < n; ++i) o.pts.push_back(r.f32());
    } else if (o.kind == K::PushShadow) {
      o.sigma = r.f32();
      o.sox = r.f32();
      o.soy = r.f32();
      o.shadowColor = r.u32();
      o.expand = r.f32();
      o.casterOpacity = ver >= 5 ? r.f32() : 1.f;
    } else if (kind == kLegacyGlow) {
      // A glow scope is a Plus group whose first draw is its light.
      if (ver < 3) return false;
      Op light;
      light.kind = K::FillRadial;
      light.box = o.box;
      light.mark = o.mark;
      light.radius = r.i32();
      r.u32();  // effect kind: glow was the only one
      const std::uint32_t color = r.u32();
      light.rgba = scaleAlpha(color, r.f32());
      light.rgba2 = color & 0xffffff00u;
      o = Op{};
      o.kind = K::PushBlend;
      o.plus = true;
      out.push_back(o);
      o = light;
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
      if (ver >= 2) {
        o.condense = r.f32();
        o.textWeight = r.f32();
      }
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
