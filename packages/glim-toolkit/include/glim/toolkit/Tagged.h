#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Event.h"
#include "glim/toolkit/Layout.h"
#include "glim/toolkit/Place.h"

namespace glim {
namespace toolkit {

template <class Tag, class Inner>
struct Tagged {
  using tag_type = Tag;
  using inner_type = Inner;
  Inner inner_;
  Tag tag_;
  const char* kind() const { return inner_.kind(); }
  Box place(Constraints c, const Engine& e) const {
    Env node{&tag_, &typeid(Tag), e.env};
    Engine ce{&node};
    return inner_.place(c, ce);
  }
  void record(const Box& box, Rec& rec, const Engine& e) const {
    Env node{&tag_, &typeid(Tag), e.env};
    Engine ce{&node};
    inner_.record(box, rec, ce);
  }
  template <class I = Inner, std::enable_if_t<has_handle<I>::value, int> = 0>
  bool handle(const Event& ev) const {
    return inner_.handle(ev);
  }
  template <class F>
  void eachChild(F&& f) const {
    if constexpr (has_children<Inner>::value) inner_.eachChild(std::forward<F>(f));
  }
  Box childBox(std::size_t i, const Box& box, const Engine& e) const {
    if constexpr (has_children<Inner>::value) {
      Env node{&tag_, &typeid(Tag), e.env};
      Engine ce{&node};
      return inner_.childBox(i, box, ce);
    }
    return box;
  }
};

template <class Tag, class Inner>
struct is_tagged<Tagged<Tag, Inner>> : std::true_type {};

}  // namespace toolkit
}  // namespace glim
