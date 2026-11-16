#pragma once

#include <functional>
#include <string>
#include <vector>

#include <glim/math.h>

namespace glim::shell {

enum class A11yRole { Group, Text, Heading, Button, Switch, Slider, TextField, Image };

// One element of the OS accessibility tree; frames are window-logical.
struct A11yElement {
  A11yRole role = A11yRole::Group;
  std::string label;
  std::string value;
  Rect frame{};
  // Index of the enclosing element in the same list; -1 at the top.
  int parent = -1;
  bool checked = false;
  bool disabled = false;
  // Takes increment / decrement (a slider, a stepper).
  bool adjustable = false;
  // Holds the app's keyboard focus; the OS cursor follows it.
  bool focused = false;
};

inline bool operator==(const A11yElement& a, const A11yElement& b) {
  return a.role == b.role && a.label == b.label && a.value == b.value &&
         a.frame.origin.x == b.frame.origin.x && a.frame.origin.y == b.frame.origin.y &&
         a.frame.size.x == b.frame.size.x && a.frame.size.y == b.frame.size.y &&
         a.parent == b.parent && a.checked == b.checked && a.disabled == b.disabled &&
         a.adjustable == b.adjustable && a.focused == b.focused;
}

// Index of the focused element; -1 when none is.
inline int focusedIndex(const std::vector<A11yElement>& elements) {
  for (std::size_t i = 0; i < elements.size(); ++i) {
    if (elements[i].focused) return static_cast<int>(i);
  }
  return -1;
}

// Focus: the OS cursor (VoiceOver) moved onto the element.
enum class A11yCommand { Activate, Increment, Decrement, Focus };

// A screen reader acted on the element at this index of the last list.
using A11yPerform = std::function<void(int index, A11yCommand)>;

}  // namespace glim::shell
