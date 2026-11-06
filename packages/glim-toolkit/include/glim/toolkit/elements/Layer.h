#pragma once

#include <utility>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

// Foreign-pixel plate: fixed 16:9 box; ids stay embedder-owned.
struct Layer {
  static constexpr const char* kKind = "Layer";
  const char* kind() const { return kKind; }
  Box place(Constraints c, const Engine&) const { return detail::resolveSize(320.0f, 180.0f, c); }
  void paint(const Box& box, AnySink& sink, const Engine&) const {
    if (sink.layer.id != 0) sink.image(box, sink.layer.id, sink.layer.foreign);
  }
};

template <class... As>
Block layer(As&&... as) {
  Layer v;
  configureTags(v, std::forward<As>(as)...);
  return finish(std::move(v), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
