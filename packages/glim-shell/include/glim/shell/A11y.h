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
};

inline bool operator==(const A11yElement& a, const A11yElement& b) {
  return a.role == b.role && a.label == b.label && a.value == b.value &&
         a.frame.origin.x == b.frame.origin.x && a.frame.origin.y == b.frame.origin.y &&
         a.frame.size.x == b.frame.size.x && a.frame.size.y == b.frame.size.y &&
         a.parent == b.parent && a.checked == b.checked && a.disabled == b.disabled;
}

// A screen reader activated the element at this index of the last list.
using A11yActivate = std::function<void(int index)>;

}  // namespace glim::shell
