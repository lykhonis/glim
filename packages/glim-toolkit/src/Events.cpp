#include <type_traits>
#include <variant>

#include "glim/toolkit/Place.h"
#include "glim/toolkit/Toolkit.h"

namespace glim {
namespace toolkit {

bool handle(const Block& b, const Event& ev) {
  if (const auto* c = b.get<Custom>()) return c->handle(ev);
  bool consumed = false;
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_handle<T>::value) consumed = el.handle(ev);
      },
      b.v
  );
  return consumed;
}

bool handleAt(const Block& b, const Box& box, const Event& ev) {
  if (const auto* c = b.get<Custom>()) return c->handleAt(box, ev);
  bool consumed = false;
  std::visit(
      [&](const auto& el) {
        using T = std::decay_t<decltype(el)>;
        if constexpr (has_handle_at<T>::value)
          consumed = el.handleAt(box, ev);
        else if constexpr (has_handle<T>::value)
          consumed = el.handle(ev);
      },
      b.v
  );
  return consumed;
}

bool dispatchPointer(
    const Block& b, const Box& box, float x, float y, const Event& ev, const Engine& e
) {
  auto [t, hitBox] = hitWithBox(b, box, x, y, e);
  if (!t) return false;
  return handleAt(*t, hitBox, ev);
}

bool broadcastEvent(const Block& b, const Box& box, const Event& ev, const Engine& e) {
  bool consumed = false;
  walk(
      b,
      box,
      e,
      [&](const Block& n, const Box&, const Engine&) -> bool {
        if (handle(n, ev)) {
          consumed = true;
          return false;  // handling node prunes its own subtree, not siblings
        }
        return true;
      },
      [](const Block&, const Box&, const Engine&) {}
  );
  return consumed;
}

}  // namespace toolkit
}  // namespace glim
