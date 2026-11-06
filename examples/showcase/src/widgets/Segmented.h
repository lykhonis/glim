#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "Gel.h"
#include "ShowcaseTheme.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Segmented : WithSemantic {
  static constexpr const char* kKind = "Seg";
  static constexpr const char* kKindActive = "Seg.active";
  static constexpr float kSegH = 36.0f;
  std::vector<std::string> options;
  std::vector<Block> children;
  // Written by child taps at their own paths, so sharing needs one
  // explicit cell, not a node-local default.
  State<int> selected = {"selected", 0};
  Anim<float> selPos = {"sel-pos", static_cast<float>(*selected)};
  float segWidth(const Engine& e) const {
    float w = 0;
    for (const auto& o : options) {
      Box t = text::measureText(o, {0, kUnbounded, 0, kUnbounded}, e);
      if (t.w > w) w = t.w;
    }
    return w + 32.0f;
  }
  Box place(Constraints c, const Engine& e) const {
    float w = segWidth(e) * static_cast<float>(options.size());
    return detail::resolveSize(w, kSegH, c);
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    float w = segWidth(e);
    return {box.x + static_cast<float>(i) * w, box.y, w, kSegH};
  }
  void record(const Box& box, Rec& rec) const {
    rec.push(kKind, box, {}, {}, 18);
    rec.push(kKindActive, activeBox(box), {}, {}, 16);
  }
  Box activeBox(const Box& box) const {
    int n = static_cast<int>(options.size());
    int s = *selected < 0 ? 0 : (*selected >= n ? n - 1 : *selected);
    return activeBoxAt(box, static_cast<float>(s));
  }
  Box activeBoxAt(const Box& box, float sel) const {
    int n = static_cast<int>(options.size());
    if (n <= 0) return {box.x, box.y + 2.0f, 0.0f, kSegH - 4.0f};
    float w = options.empty() ? 0.0f : box.w / static_cast<float>(n);
    return {box.x + sel * w, box.y + 2.0f, w, kSegH - 4.0f};
  }
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    sink.fillGradient(
        box,
        18,
        themeColor(e, kShowTrackTop, Show::kTrackTop),
        themeColor(e, kShowTrackBottom, Show::kTrackBottom)
    );
    Box act = activeBoxAt(box, *selPos);
    sink.fillGradient(
        act,
        16,
        themeColor(e, kShowAccentTop, Show::kAccentTop),
        themeColor(e, kShowAccentBottom, Show::kAccentBottom)
    );
    gelCoat(sink, act, 16);
    sink.stroke(
        act, 16, 1.0f, themeColor(e, kShowCardStroke, Show::kCardStroke)
    );
  }
};

template <class... As>
Block segmented(Segmented s, As&&... as) {
  for (std::size_t i = 0; i < s.options.size(); ++i) {
    State<int> cell = s.selected;
    Anim<float> indicator = s.selPos;
    s.children.push_back(tap(
        lines(s.options[i]),
        [cell, indicator, i] {
          cell = static_cast<int>(i);
          indicator = static_cast<float>(i);
        },
        pad(16, 10)
    ));
  }
  configure(s, s.children, std::forward<As>(as)...);
  return finish(std::move(s), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
