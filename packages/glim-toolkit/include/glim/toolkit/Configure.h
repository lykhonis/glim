#pragma once

#include <type_traits>
#include <utility>
#include <vector>

#include "glim/toolkit/Block.h"

namespace glim {
namespace toolkit {

template <class... Ts>
struct NoDup : std::true_type {};
template <class A, class... R>
struct NoDup<A, R...>
    : std::integral_constant<bool, ((!is_tag<std::decay_t<A>>::value ||
                                     !(std::is_same<std::decay_t<A>, std::decay_t<R>>::value ||
                                       ...)) &&
                                    NoDup<R...>::value)> {};

template <class H, class A>
void dispatchOne(H& h, std::vector<Block>& children, A&& a) {
  using T = std::decay_t<A>;
  if constexpr (is_tag<T>::value) {
    apply(h, std::forward<A>(a));
  } else if constexpr (is_child_type<A>::value) {
    children.push_back(toBlock(std::forward<A>(a)));
  } else {
    static_assert(always_false<T>::value,
                  "factory argument is not a tag or child: pass tags (gap(8), primary()) or "
                  "Blocks/viewables/elements");
  }
}

template <class H, class... As>
void configure(H& h, std::vector<Block>& children, As&&... as) {
  static_assert(NoDup<As...>::value, "duplicate tag in one factory call (e.g. gap(8), gap(4))");
  (dispatchOne(h, children, std::forward<As>(as)), ...);
}

template <class H, class... As>
void configureTags(H& h, As&&... as) {
  static_assert(NoDup<As...>::value, "duplicate tag in one factory call (e.g. gap(8), gap(4))");
  static_assert((is_tag<std::decay_t<As>>::value && ...),
                "leaf factory takes tags only, no children");
  (apply(h, std::forward<As>(as)), ...);
}

}  // namespace toolkit
}  // namespace glim
