#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>

namespace glim {
namespace toolkit {

// Each enumerator is named in A11y.cpp nameOf; -Wswitch catches a missing one.
enum class A11yRole : std::uint8_t { None, Text, Heading, Button, Switch, Slider, TextField };
enum class A11yState : std::uint8_t { Checkable, Checked, Selected, Disabled };
enum class A11yAction : std::uint8_t { Activate, Increment, Decrement };
enum class A11yLive : std::uint8_t { Off, Polite, Assertive };

template <class E>
struct A11ySet {
  std::uint32_t bits = 0;
  constexpr A11ySet() = default;
  constexpr A11ySet(std::initializer_list<E> es) {
    for (E e : es) bits |= bit(e);
  }
  static constexpr std::uint32_t bit(E e) { return 1u << static_cast<std::uint32_t>(e); }
  constexpr bool has(E e) const { return (bits & bit(e)) != 0; }
  constexpr bool empty() const { return bits == 0; }
  constexpr A11ySet with(E e, bool on = true) const {
    A11ySet s = *this;
    s.bits = on ? (bits | bit(e)) : (bits & ~bit(e));
    return s;
  }
  constexpr A11ySet operator|(A11ySet o) const {
    A11ySet s;
    s.bits = bits | o.bits;
    return s;
  }
  constexpr bool operator==(A11ySet o) const { return bits == o.bits; }
};

using A11yStates = A11ySet<A11yState>;
using A11yActions = A11ySet<A11yAction>;

struct A11yRange {
  float now = 0;
  float min = 0;
  float max = 1;
};

// What a control is beyond its name: bridges map these onto OS roles and states.
struct A11yTraits {
  A11yRole role = A11yRole::None;
  std::string value;
  std::optional<A11yRange> range;
  A11yStates states;
  A11yActions actions;
  A11yLive live = A11yLive::Off;
};

// Set fields of over win; states and actions accumulate.
inline A11yTraits overlay(A11yTraits base, const A11yTraits& over) {
  if (over.role != A11yRole::None) base.role = over.role;
  if (!over.value.empty()) base.value = over.value;
  if (over.range) base.range = over.range;
  base.states = base.states | over.states;
  base.actions = base.actions | over.actions;
  if (over.live != A11yLive::Off) base.live = over.live;
  return base;
}

}  // namespace toolkit
}  // namespace glim
