#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "glim/toolkit/A11ySet.h"

namespace glim {
namespace toolkit {

// Each enumerator is named in A11y.cpp nameOf; -Wswitch catches a missing one.
enum class A11yRole : std::uint8_t { None, Text, Heading, Button, Switch, Slider, TextField };
enum class A11yState : std::uint8_t { Checkable, Checked, Selected, Disabled };
enum class A11yAction : std::uint8_t { Activate, Increment, Decrement };
enum class A11yLive : std::uint8_t { Off, Polite, Assertive };

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
