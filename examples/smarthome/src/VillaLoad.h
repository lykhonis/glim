#pragma once

// Loads the villa photographs once per paint context and parks the ids
// in the store, where Backdrop reads them. The files sit next to the
// example (assets/villa-*.jpg); a missing file leaves the wash in
// place.

#include <string>

#include "glim/image/Load.h"
#include "glim/paint/Context.h"
#include "widgets/Villa.h"

namespace glim {
namespace toolkit {

inline VillaPlate loadVilla(
  glim::paint::Context& ctx, const char* name
) {
  std::string file = __FILE__;
  file =
    file.substr(0, file.find_last_of("/\\")) + "/../assets/" + name;
  glim::image::Image im;
  if (!glim::image::load(file.c_str(), &im) || im.empty()) return {};
  const std::uint32_t id =
    ctx.addImage(im.width, im.height, im.rgba.data());
  if (id == 0) return {};
  return {id, im.width, im.height};
}

inline void ensureVilla(glim::paint::Context& ctx, Store& store) {
  State<VillaPlates> st = store.state(kVillaKey, VillaPlates{});
  const VillaPlates cur = st.get();
  if (cur.owner == &ctx && cur.day.id != 0 &&
      ctx.images().get(cur.day.id))
    return;
  st.set({
    .day = loadVilla(ctx, "villa-day.jpg"),
    .night = loadVilla(ctx, "villa-night.jpg"),
    .lit = loadVilla(ctx, "villa-night-lit.jpg"),
    .owner = &ctx,
  });
}

}  // namespace toolkit
}  // namespace glim
