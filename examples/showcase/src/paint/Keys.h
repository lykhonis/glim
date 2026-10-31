#pragma once

// Shell-to-toolkit key mapping, shared by the windowed run and the
// golden test (pure function, no window needed). No Tab on this
// shell: pointer taps set focus, Dpads move, Enter/DpadCenter
// activate. Unmapped keys (Escape included — harness-owned) yield
// nullopt and fall through to built-in behavior.
#include <glim/shell/Event.h>

#include <optional>

#include "glim/toolkit/Focus.h"

namespace glim {
namespace toolkit {
namespace paint {

inline std::optional<Key> showcaseKey(shell::Key k) {
  switch (k) {
    case shell::Key::DpadLeft:
      return Key::Left;
    case shell::Key::DpadRight:
      return Key::Right;
    case shell::Key::DpadUp:
      return Key::Up;
    case shell::Key::DpadDown:
      return Key::Down;
    case shell::Key::Enter:
    case shell::Key::DpadCenter:
      return Key::Activate;
    default:
      return std::nullopt;
  }
}

}  // namespace paint
}  // namespace toolkit
}  // namespace glim
