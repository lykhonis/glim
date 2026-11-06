#pragma once

#include <cassert>
#include <iostream>
#include <sstream>
#include <utility>

#include "glim/toolkit/Toolkit.h"

using namespace glim::toolkit;

static_assert(has_children<Row>::value, "");
static_assert(has_children<Column>::value, "");
static_assert(has_children<Stack>::value, "");
static_assert(has_children<List>::value, "");
static_assert(has_children<Roll>::value, "");
static_assert(has_children<Tap>::value, "");
static_assert(has_child_vector<Row>::value, "");
static_assert(!has_eachChild<Row>::value, "");
static_assert(has_child_vector<Tap>::value && has_childBox<Tap>::value, "");
static_assert(!has_children<Empty>::value, "");
static_assert(!has_children<Lines>::value, "");
static_assert(!has_children<Seat>::value, "");
static_assert(!has_inner_box<Seat>::value, "");
static_assert(has_handle<Seat>::value, "");
static_assert(has_kind<Seat>::value, "");
static_assert(!has_record<Seat>::value, "");
static_assert(has_inner_box<Tap>::value, "");
static_assert(!has_inner_box<Row>::value, "");
static_assert(!has_handle_at<Tap>::value, "");
static_assert(!has_handle_at<Row>::value, "");

namespace glim {
namespace toolkit {

struct Radial : WithGap, WithRadius {
  const char* kind() const { return "Radial"; }
  std::vector<Block> children;
  Box place(Constraints c, const Engine& e) const {
    float w = 0, h = 0;
    for (const auto& child : children) {
      Box b = e.place(child, {0, c.maxW, 0, c.maxH});
      w = w > b.w ? w : b.w;
      h += b.h + static_cast<float>(gap);
    }
    if (!children.empty()) h -= static_cast<float>(gap);
    float d = w > h ? w : h;
    d += static_cast<float>(radius) * 2.0f;
    if (d < c.minW) d = c.minW;
    if (d > c.maxW) d = c.maxW;
    return {0, 0, d, d};
  }
  Box childBox(std::size_t i, const Box& b, const Engine& e) const {
    float y = b.y;
    for (std::size_t k = 0; k < children.size(); ++k) {
      Box kb = e.place(children[k], {0, b.w, 0, b.h});
      kb.x = b.x;
      kb.y = y;
      if (k == i) return kb;
      y += kb.h + static_cast<float>(gap);
    }
    return b;
  }
};

struct Pin {};

template <>
struct is_tag<Pin> : std::true_type {};

template <>
struct is_slot_tag<Pin> : std::true_type {};

struct Tint {
  int v = 0;
};

template <>
struct is_tag<Tint> : std::true_type {};

template <>
struct is_slot_tag<Tint> : std::true_type {};

struct TintProbe {
  static constexpr const char* kKind = "TintProbe";
  const char* kind() const { return kKind; }
  State<int> seen;
  Box place(Constraints c, const Engine& e) const {
    auto t = e.get<Tint>();
    seen = t ? t->get().v : -1;
    return detail::resolveSize(10, 10, c);
  }
};

}  // namespace toolkit
}  // namespace glim

namespace {

struct Counter {
  glim::toolkit::State<int> count;
  glim::toolkit::Block view() const {
    auto c = count;
    using namespace glim::toolkit;
    return column(
        gap(8),
        lines("Count: " + std::to_string(*c)),
        tap(
            lines("Inc"), [c] { c = *c + 1; }, primary()
        )
    );
  }
};

template <class... As>
glim::toolkit::Block myRadial(glim::toolkit::State<int> sel, As&&... as) {
  using namespace glim::toolkit;
  Radial m;
  m.children.push_back(lines("sel=" + std::to_string(*sel)));
  configure(m, m.children, std::forward<As>(as)...);
  return finish(std::move(m), std::forward<As>(as)...);
}

// Test capture only.
struct AtBox {
  const char* kind() const { return "AtBox"; }
  mutable Box seen{0, 0, 0, 0};
  Box place(Constraints c, const Engine&) const {
    return glim::toolkit::detail::resolveSize(50.0f, 20.0f, c);
  }
  bool handleAt(const Box& b, const Event& ev) const {
    if (ev.is<PointerDown>()) {
      seen = b;
      return true;
    }
    return false;
  }
};

}  // namespace
