#pragma once

#include <functional>
#include <optional>
#include <typeinfo>

namespace glim {
namespace toolkit {

enum class Key { Left, Right, Up, Down, Next, Prev, Activate };

struct PointerDown {
  float x = 0;
  float y = 0;
};
struct PointerUp {
  float x = 0;
  float y = 0;
};
struct PointerMove {
  float x = 0;
  float y = 0;
  // Set on drags; hover moves arrive clear and must be ignored.
  bool pressed = false;
};
struct Wheel {
  float x = 0;
  float y = 0;
  // Logical-pixel deltas; positive dy reveals content below.
  float dx = 0;
  float dy = 0;
};
struct KeyPress {
  Key key = Key::Activate;
};
struct Activate {};

class Event {
 public:
  template <class E>
  static Event borrow(const E& e) {
    return Event(static_cast<const void*>(&e), &typeid(E));
  }
  template <class E>
  std::optional<std::reference_wrapper<const E>> as() const {
    if (*type_ == typeid(E)) return std::cref(*static_cast<const E*>(ptr_));
    return std::nullopt;
  }
  template <class E>
  bool is() const {
    return *type_ == typeid(E);
  }

 private:
  Event(const void* ptr, const std::type_info* type) : ptr_(ptr), type_(type) {}
  const void* ptr_;
  const std::type_info* type_;
};

}  // namespace toolkit
}  // namespace glim
