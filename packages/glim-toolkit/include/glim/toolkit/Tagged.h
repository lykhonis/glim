#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Event.h"
#include "glim/toolkit/Layout.h"
#include "glim/toolkit/Place.h"
#include "glim/toolkit/theme/Keys.h"

namespace glim {
namespace toolkit {

template <class Tag, class Inner>
struct Tagged {
  using tag_type = Tag;
  using inner_type = Inner;
  static constexpr bool kClipsChildren = has_clip_flag<Inner>::value;
  Inner inner_;
  Tag tag_;
  const char* kind() const { return inner_.kind(); }
  Box place(Constraints c, const Engine& e) const {
    Env node{&tag_, &typeid(Tag), e.env};
    Engine ce{&node};
    return inner_.place(c, ce);
  }
  void record(const Box& box, Rec& rec) const {
    if constexpr (has_record<Inner>::value) {
      inner_.record(box, rec);
    } else {
      rec.push(inner_.kind(), box);
    }
  }
  template <class I = Inner, std::enable_if_t<has_handle<I>::value, int> = 0>
  bool handle(const Event& ev) const {
    return inner_.handle(ev);
  }
  template <class I = Inner, std::enable_if_t<has_handle_at<I>::value, int> = 0>
  bool handleAt(const Box& box, const Event& ev) const {
    return inner_.handleAt(box, ev);
  }
  template <class I = Inner, std::enable_if_t<has_paint<I>::value, int> = 0>
  void paint(const Box& box, AnySink& sink, const Engine& e) const {
    inner_.paint(box, sink, e);
  }
  template <class F>
  void eachChild(F&& f) const {
    if constexpr (has_children<Inner>::value) eachChildOf(inner_, std::forward<F>(f));
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    if constexpr (has_children<Inner>::value) {
      Env node{&tag_, &typeid(Tag), e.env};
      Engine ce{&node};
      return inner_.childBox(i, box, ce);
    }
    return box;
  }
  // Theme scopes ride through slot tags the way clip flags do: a
  // themed subtree wrapped in expand()/etc. keeps its entries.
  ThemeScope themeScope() const {
    if constexpr (has_theme_entries<Inner>::value) return inner_.themeScope();
    return {nullptr, 0};
  }
};

template <class Tag, class Inner>
struct is_tagged<Tagged<Tag, Inner>> : std::true_type {};

}  // namespace toolkit
}  // namespace glim
