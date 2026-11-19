#include <glim/shell/Event.h>
#include <glim/shell/Window.h>

#include "Shown.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/html5.h>
#endif

namespace glim::shell::detail {

#if defined(__EMSCRIPTEN__)
namespace {

PointerButton button(unsigned short b) {
  switch (b) {
    case 0:
      return PointerButton::Left;
    case 1:
      return PointerButton::Middle;
    case 2:
      return PointerButton::Right;
    default:
      return PointerButton::None;
  }
}

EM_BOOL onMouse(int type, const EmscriptenMouseEvent* m, void* user) {
  auto* window = static_cast<Window*>(user);
  const EventType t = type == EMSCRIPTEN_EVENT_MOUSEDOWN ? EventType::PointerDown
                      : type == EMSCRIPTEN_EVENT_MOUSEUP ? EventType::PointerUp
                                                         : EventType::PointerMove;
  Event e(t);
  e.setPoint(static_cast<float>(m->targetX), static_cast<float>(m->targetY));
  e.setButton(button(m->button));
  e.setShift(m->shiftKey);
  window->dispatch(e);
  // A handled press keeps the page from moving focus off the text bridge.
  return t == EventType::PointerDown;
}

EM_BOOL onWheel(int, const EmscriptenWheelEvent* w, void* user) {
  Event e(EventType::Wheel);
  e.setPoint(static_cast<float>(w->mouse.targetX), static_cast<float>(w->mouse.targetY));
  e.setScroll(static_cast<float>(w->deltaX), static_cast<float>(w->deltaY));
  static_cast<Window*>(user)->dispatch(e);
  return true;
}

Key key(const char* name, bool editing) {
  const std::string k = name;
  if (k == "Enter") return Key::Enter;
  if (k == "Escape") return Key::Escape;
  // While a field edits, arrows move its caret in the text bridge.
  if (editing) return Key::Unknown;
  if (k == "ArrowUp") return Key::DpadUp;
  if (k == "ArrowDown") return Key::DpadDown;
  if (k == "ArrowLeft") return Key::DpadLeft;
  if (k == "ArrowRight") return Key::DpadRight;
  return Key::Unknown;
}

EM_BOOL onKey(int type, const EmscriptenKeyboardEvent* k, void* user) {
  const Key mapped = key(k->key, textInputFocused());
  if (mapped == Key::Unknown) return false;
  Event e(type == EMSCRIPTEN_EVENT_KEYDOWN ? EventType::KeyDown : EventType::KeyUp);
  e.setKey(mapped);
  static_cast<Window*>(user)->dispatch(e);
  return true;
}

}  // namespace

void listen(Window* window, const std::string& selector) {
  const char* target = selector.c_str();
  emscripten_set_mousedown_callback(target, window, true, onMouse);
  emscripten_set_mouseup_callback(target, window, true, onMouse);
  emscripten_set_mousemove_callback(target, window, true, onMouse);
  emscripten_set_wheel_callback(target, window, true, onWheel);
  emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, window, true, onKey);
  emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, window, true, onKey);
}
#else
void listen(Window*, const std::string&) {}
#endif

}  // namespace glim::shell::detail
