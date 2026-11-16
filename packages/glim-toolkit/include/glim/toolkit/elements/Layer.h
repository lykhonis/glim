#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

// Pointer or wheel input for an embedded component, in its own space:
// logical px from its top-left, with its size so a producer can scale to
// its texture.
struct LayerInput {
  enum class Phase { Down, Move, Up, Wheel };
  Phase phase = Phase::Down;
  float x = 0;
  float y = 0;
  float dx = 0;
  float dy = 0;
  float width = 0;
  float height = 0;
};

using LayerInputFn = std::function<bool(const LayerInput&)>;

// Maps a pointer or wheel event (already in the component's paint space)
// to LayerInput; nullopt for other events and hover moves.
std::optional<LayerInput> layerInput(const Box& box, const Event& ev);

struct LayerProps {
  // 0 uses the ambient layer the host binds for the pass.
  std::uint32_t image = 0;
  LayerInputFn onInput;
};

// Foreign-pixel plate: fixed 16:9 box; ids stay embedder-owned.
struct Layer {
  static constexpr const char* kKind = "Layer";
  LayerProps props;
  const char* kind() const { return kKind; }
  Box place(Constraints c, const Engine&) const { return detail::resolveSize(320.0f, 180.0f, c); }
  void paint(const Box& box, AnySink& sink, const Engine&) const {
    if (props.image != 0) {
      sink.image(box, props.image, true);
    } else if (sink.layer.id != 0) {
      sink.image(box, sink.layer.id, sink.layer.foreign);
    }
  }
  bool handleAt(const Box& box, const Event& ev) const {
    if (!props.onInput) return false;
    const std::optional<LayerInput> in = layerInput(box, ev);
    return in && props.onInput(*in);
  }
};

template <class... As>
Block layer(LayerProps props, As&&... as) {
  Layer v{std::move(props)};
  configureTags(v, std::forward<As>(as)...);
  return finish(std::move(v), std::forward<As>(as)...);
}

template <class... As>
Block layer(As&&... as) {
  Layer v;
  configureTags(v, std::forward<As>(as)...);
  return finish(std::move(v), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
