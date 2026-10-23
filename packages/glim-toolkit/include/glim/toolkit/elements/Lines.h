#pragma once

#include <string>
#include <utility>

#include "glim/toolkit/ConfigureFwd.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

struct Lines : WithAlign, WithSemantic {
  static constexpr const char* kKind = "Lines";
  std::string text;
  Box place(Constraints c, const Engine&) const { return detail::measureText(text, c); }
  void record(const Box& box, Rec& rec, const Engine&) const {
    rec.push(kKind, box, text, semantic);
  }
};

template <class... As>
auto lines(std::string text, As&&... as) {
  Lines l;
  l.text = std::move(text);
  configureTags(l, std::forward<As>(as)...);
  return l;
}

}  // namespace toolkit
}  // namespace glim
