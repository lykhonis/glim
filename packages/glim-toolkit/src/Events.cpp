#include <tuple>
#include <type_traits>
#include <variant>

#include "glim/toolkit/Place.h"
#include "glim/toolkit/Toolkit.h"
#include "glim/toolkit/elements/Layer.h"

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

std::optional<LayerInput> layerInput(const Box& box, const Event& ev) {
  const auto at = [&](LayerInput::Phase phase, float x, float y) {
    return LayerInput{
      .phase = phase, .x = x - box.x, .y = y - box.y, .width = box.w, .height = box.h
    };
  };
  if (auto p = ev.as<PointerDown>()) return at(LayerInput::Phase::Down, p->get().x, p->get().y);
  if (auto p = ev.as<PointerUp>()) return at(LayerInput::Phase::Up, p->get().x, p->get().y);
  if (auto p = ev.as<PointerMove>()) {
    if (!p->get().pressed) return std::nullopt;
    return at(LayerInput::Phase::Move, p->get().x, p->get().y);
  }
  if (auto p = ev.as<Wheel>()) {
    LayerInput in = at(LayerInput::Phase::Wheel, p->get().x, p->get().y);
    in.dx = p->get().dx;
    in.dy = p->get().dy;
    return in;
  }
  return std::nullopt;
}

namespace detail {

namespace {

template <class E>
bool handleMoved(const Block& b, const Box& box, const PlaneMap& toNode, const E& ev) {
  E local = ev;
  std::tie(local.x, local.y) = toNode.apply(ev.x, ev.y);
  return handleAt(b, box, Event::borrow(local));
}

}  // namespace

bool handleIn(const Block& b, const Box& box, const PlaneMap& toNode, const Event& ev) {
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
