#pragma once

// Loads the villa photograph once per paint context and parks the id in
// the store, where Backdrop reads it. The file sits next to the example
// (assets/villa.jpg); a missing file leaves the wash in place.

#include <cstdio>
#include <string>

#include "glim/image/Load.h"
#include "glim/paint/Context.h"
#include "widgets/Villa.h"

namespace glim {
namespace toolkit {

inline void ensureVilla(glim::paint::Context& ctx, Store& store) {
  State<VillaPlate> st = store.state(kVillaKey, VillaPlate{});
  const VillaPlate cur = st.get();
  if (cur.id != 0 && cur.owner == &ctx && ctx.images().get(cur.id))
    return;

  std::string file = __FILE__;
  const auto slash = file.find_last_of("/\\");
  file = file.substr(0, slash) + "/../assets/villa.jpg";
  glim::image::Image im;
  if (!glim::image::load(file.c_str(), &im) || im.empty()) return;

  VillaPlate next;
  next.id = ctx.addImage(im.width, im.height, im.rgba.data());
  if (next.id == 0) return;
  next.w = im.width;
  next.h = im.height;
  next.owner = &ctx;
  st.set(next);
}

}  // namespace toolkit
}  // namespace glim
