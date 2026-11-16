#include <tuple>
#include <type_traits>
#include <variant>

#include "glim/toolkit/Place.h"
#include "glim/toolkit/Toolkit.h"

namespace glim {
namespace toolkit {

bool handle(const Block& b, const Event& ev) {
  if (auto c = b.get<Custom>()) return c->get().handle(ev);
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
  if (auto c = b.get<Custom>()) return c->get().handleAt(box, ev);
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
  const HitAt at = hitWithBox(b, box, x, y, e);
  if (!at.node) return false;
  detail::AutoHandle h(e.store ? &e.store->get() : nullptr, detail::AutoCtx::found());
  return detail::handleIn(*at.node, at.box, at.toNode, ev);
}

namespace detail {

namespace {

template <class E>
bool handleMoved(const Block& b, const Box& box, const Affine& toNode, const E& ev) {
  E local = ev;
  std::tie(local.x, local.y) = toNode.apply(ev.x, ev.y);
  return handleAt(b, box, Event::borrow(local));
}

}  // namespace

bool handleIn(const Block& b, const Box& box, const Affine& toNode, const Event& ev) {
  if (auto p = ev.as<PointerDown>()) return handleMoved(b, box, toNode, p->get());
  if (auto p = ev.as<PointerUp>()) return handleMoved(b, box, toNode, p->get());
  if (auto p = ev.as<PointerMove>()) return handleMoved(b, box, toNode, p->get());
  if (auto p = ev.as<Wheel>()) return handleMoved(b, box, toNode, p->get());
  return handleAt(b, box, ev);
}

}  // namespace detail

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
