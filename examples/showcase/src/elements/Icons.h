#pragma once

#include <string>
#include <utility>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

// Geometric icon set (T10, example-local). No asset pipeline: every
// icon composes three generic sink kinds — Dot (filled circle),
// Ring (stroked outline), Bar (filled rect) — plus ASCII text where
// the font covers it. Glyph coverage note: the v1 Latin shaper only
// covers ASCII, so icon-adjacent text stays ASCII ("-" not "–", "v"
// not "↓") until the shaper grows (see §9). Each icon is a fixed-size
// record-only leaf; interactivity comes from wrapping in Tap at the
// use site.
struct IconBase {
  static constexpr float kS = 24.0f;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(kS, kS, c);
  }
};

namespace detail {

inline void dot(
    Rec& rec, const Box& b, float x, float y, float d, bool bg = false
) {
  rec.push(bg ? "Dot.bg" : "Dot", {b.x + x, b.y + y, d, d}, {}, {}, 12);
}
inline void ring(Rec& rec, const Box& b, float x, float y, float d) {
  rec.push("Ring", {b.x + x, b.y + y, d, d}, {}, {}, 10);
}
inline void bar(
    Rec& rec,
    const Box& b,
    float x,
    float y,
    float w,
    float h,
    bool bg = false
) {
  rec.push(bg ? "Bar.bg" : "Bar", {b.x + x, b.y + y, w, h}, {}, {}, 1);
}

}  // namespace detail

struct IconMoon : IconBase {
  static constexpr const char* kKind = "Icon.moon";
  const char* kind() const { return kKind; }
  void record(const Box& box, Rec& rec) const {
    detail::dot(rec, box, 1, 1, 22);
    detail::dot(rec, box, 9, -5, 20, true);
  }
};

struct IconSun : IconBase {
  static constexpr const char* kKind = "Icon.sun";
  const char* kind() const { return kKind; }
  void record(const Box& box, Rec& rec) const {
    detail::dot(rec, box, 8, 8, 8);
    detail::bar(rec, box, 11, 1, 2, 5);
    detail::bar(rec, box, 11, 18, 2, 5);
    detail::bar(rec, box, 1, 11, 5, 2);
    detail::bar(rec, box, 18, 11, 5, 2);
  }
};

struct IconGear : IconBase {
  static constexpr const char* kKind = "Icon.gear";
  const char* kind() const { return kKind; }
  void record(const Box& box, Rec& rec) const {
    detail::ring(rec, box, 3, 3, 18);
    detail::bar(rec, box, 11, 0, 2, 5);
    detail::bar(rec, box, 11, 19, 2, 5);
    detail::bar(rec, box, 0, 11, 5, 2);
    detail::bar(rec, box, 19, 11, 5, 2);
    detail::dot(rec, box, 9, 9, 6);
  }
};

struct IconBell : IconBase {
  static constexpr const char* kKind = "Icon.bell";
  const char* kind() const { return kKind; }
  void record(const Box& box, Rec& rec) const {
    detail::dot(rec, box, 7, 2, 10);
    detail::bar(rec, box, 7, 7, 10, 9);
    detail::dot(rec, box, 10, 17, 4);
  }
};

struct IconDownload : IconBase {
  static constexpr const char* kKind = "Icon.download";
  const char* kind() const { return kKind; }
  void record(const Box& box, Rec& rec) const {
    detail::bar(rec, box, 11, 2, 2, 10);
    rec.push("Lines", {box.x + 8, box.y + 8, 24, 14}, "v", {});
  }
};

struct IconExpand : IconBase {
  static constexpr const char* kKind = "Icon.expand";
  const char* kind() const { return kKind; }
  void record(const Box& box, Rec& rec) const {
    detail::bar(rec, box, 3, 3, 6, 2);
    detail::bar(rec, box, 3, 3, 2, 6);
    detail::bar(rec, box, 15, 3, 6, 2);
    detail::bar(rec, box, 19, 3, 2, 6);
    detail::bar(rec, box, 3, 19, 6, 2);
    detail::bar(rec, box, 3, 15, 2, 6);
    detail::bar(rec, box, 15, 19, 6, 2);
    detail::bar(rec, box, 19, 15, 2, 6);
  }
};

struct IconTicket : IconBase {
  static constexpr const char* kKind = "Icon.ticket";
  const char* kind() const { return kKind; }
  void record(const Box& box, Rec& rec) const {
    detail::bar(rec, box, 2, 7, 20, 10);
    detail::dot(rec, box, 0, 10, 4, true);
    detail::dot(rec, box, 20, 10, 4, true);
  }
};

struct IconPerson : IconBase {
  static constexpr const char* kKind = "Icon.person";
  const char* kind() const { return kKind; }
  void record(const Box& box, Rec& rec) const {
    detail::dot(rec, box, 8, 2, 8);
    detail::bar(rec, box, 5, 12, 14, 8);
  }
};

struct IconPie : IconBase {
  static constexpr const char* kKind = "Icon.pie";
  const char* kind() const { return kKind; }
  void record(const Box& box, Rec& rec) const {
    detail::dot(rec, box, 3, 3, 18);
    detail::bar(rec, box, 12, 3, 2, 9, true);
    detail::bar(rec, box, 12, 12, 9, 2, true);
  }
};

// Initials avatar: teal tile + centered initials, no photo assets.
struct Avatar : IconBase {
  const char* kind() const { return "Avatar"; }
  std::string initials;
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(40.0f, 40.0f, c);
  }
  void record(const Box& box, Rec& rec) const {
    rec.push("Tile", box, {}, {}, 20);
    // Centering only: record carries no Engine, so this stays on the
    // fixed advance (a pixel off for wide caps — entry widths are
    // cosmetic; hit/focus use layout boxes).
    Box t = detail::measureText(
        initials, {0, kUnbounded, 0, kUnbounded}, Engine{}
    );
    rec.push(
        "Lines",
        {box.x + (box.w - t.w) * 0.5f,
         box.y + (box.h - Tokens::kTypeLine) * 0.5f,
         t.w,
         Tokens::kTypeLine},
        initials,
        {}
    );
  }
};

template <class T, class... As>
Block icon(As&&... as) {
  T t;
  configureTags(t, std::forward<As>(as)...);
  return finish(std::move(t), std::forward<As>(as)...);
}

inline Block avatar(std::string initials) {
  Avatar a;
  a.initials = std::move(initials);
  return finish(std::move(a));
}

}  // namespace toolkit
}  // namespace glim
