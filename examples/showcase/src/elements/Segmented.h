#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Configure.h"
#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/State.h"
#include "glim/toolkit/Tags.h"
#include "glim/toolkit/Tokens.h"

namespace glim {
namespace toolkit {

// Segmented control (T8 catalog, example-local). A navy track with a
// teal thumb under the selected option; each option is a Tap child so
// hit-testing and focus fall out of the existing machinery (one
// focusable per segment, arrows move spatially, Activate selects).
// The kinds below are interpreted by the showcase sink — zero codec
// edits.
struct Segmented : WithSemantic {
  static constexpr const char* kKind = "Seg";
  static constexpr const char* kKindActive = "Seg.active";
  static constexpr float kSegH = 36.0f;
  std::vector<std::string> options;
  std::vector<Block> children;
  State<int> selected;
  float segWidth(const Engine& e) const {
    float w = 0;
    for (const auto& o : options) {
      Box t = detail::measureText(o, {0, kUnbounded, 0, kUnbounded}, e);
      if (t.w > w) w = t.w;
    }
    return w + 32.0f;
  }
  Box place(Constraints c, const Engine& e) const {
    float w = segWidth(e) * static_cast<float>(options.size());
    return detail::resolveSize(w, kSegH, c);
  }
  template <class F>
  void eachChild(F&& f) const {
    detail::forEachBlock(children, std::forward<F>(f));
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    float w = segWidth(e);
    return {box.x + static_cast<float>(i) * w, box.y, w, kSegH};
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, semantic, 18);
    int n = static_cast<int>(options.size());
    int s = *selected < 0 ? 0 : (*selected >= n ? n - 1 : *selected);
    float w = options.empty() ? 0.0f : box.w / static_cast<float>(n);
    rec.push(
        kKindActive,
        {box.x + static_cast<float>(s) * w,
         box.y + 2.0f,
         w,
         kSegH - 4.0f},
        {},
        semantic,
        14
    );
  }
};

template <class... As>
Block segmented(
    std::vector<std::string> options, State<int> sel, As&&... as
) {
  Segmented s;
  s.options = options;
  s.selected = std::move(sel);
  for (std::size_t i = 0; i < options.size(); ++i) {
    State<int> cell = s.selected;
    s.children.push_back(tap(
        lines(options[i]),
        [cell, i] { cell = static_cast<int>(i); },
        pad(16, 10)
    ));
  }
  configure(s, s.children, std::forward<As>(as)...);
  return finish(std::move(s), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
