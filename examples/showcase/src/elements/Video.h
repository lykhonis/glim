#pragma once

#include <utility>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

// Foreign-video plate (T4 scaffold). Fixed 16:9 box; the paint bridge
// resolves the texture through VideoFeed and blits it under glass.
// Carries no texture id itself — ids are embedder-owned (ImageStore
// rule).
struct Video {
  static constexpr const char* kKind = "Video";
  const char* kind() const { return kKind; }
  Box place(Constraints c, const Engine&) const {
    return detail::resolveSize(320.0f, 180.0f, c);
  }
  void record(const Box& box, Rec& rec, const Engine&) const {
    rec.push(kKind, box);
  }
};

template <class... As>
Block video(As&&... as) {
  Video v;
  configureTags(v, std::forward<As>(as)...);
  return finish(std::move(v), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
