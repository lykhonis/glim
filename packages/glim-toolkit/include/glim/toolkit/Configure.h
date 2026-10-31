#pragma once

#include <type_traits>
#include <utility>
#include <vector>

#include "glim/toolkit/Block.h"
#include "glim/toolkit/Tagged.h"
#include "glim/toolkit/Tags.h"

namespace glim {
namespace toolkit {

template <class... Ts>
struct NoDup : std::true_type {};
template <class A, class... R>
struct NoDup<A, R...> : std::integral_constant<
                            bool,
                            ((!is_tag<std::decay_t<A>>::value ||
                              !(std::is_same<std::decay_t<A>, std::decay_t<R>>::value || ...)) &&
                             NoDup<R...>::value)> {};

template <class H, class A>
void dispatchOne(H& h, std::vector<Block>& children, A&& a) {
  using T = std::decay_t<A>;
  if constexpr (is_slot_tag<T>::value) {
  } else if constexpr (is_tag<T>::value) {
    apply(h, std::forward<A>(a));
  } else if constexpr (is_child_type<A>::value) {
    children.push_back(toBlock(std::forward<A>(a)));
  } else {
    static_assert(
        always_false<T>::value,
        "factory argument is not a tag or child: pass tags (gap(8), primary()) or "
        "Blocks/viewables/elements"
    );
  }
}

template <class H, class... As>
void configure(H& h, std::vector<Block>& children, As&&... as) {
  static_assert(NoDup<As...>::value, "duplicate tag in one factory call (e.g. gap(8), gap(4))");
  (dispatchOne(h, children, std::forward<As>(as)), ...);
}

template <class H, class A>
void applyElementTag(H& h, A&& a) {
  using T = std::decay_t<A>;
  if constexpr (!is_slot_tag<T>::value) apply(h, std::forward<A>(a));
}

template <class H, class... As>
void configureTags(H& h, As&&... as) {
  static_assert(NoDup<As...>::value, "duplicate tag in one factory call (e.g. gap(8), gap(4))");
  static_assert(
      (is_tag<std::decay_t<As>>::value && ...), "leaf factory takes tags only, no children"
  );
  (applyElementTag(h, std::forward<As>(as)), ...);
}

template <class H>
Block wrapCustomTags(H&& h) {
  return toBlock(std::forward<H>(h));
}

template <class H, class A, class... R>
Block wrapCustomTags(H&& h, A&& a, R&&... r) {
  using T = std::decay_t<A>;
  if constexpr (is_slot_tag<T>::value) {
    return wrapCustomTags(
        Tagged<T, std::decay_t<H>>{std::forward<H>(h), std::forward<A>(a)}, std::forward<R>(r)...
    );
  } else {
    return wrapCustomTags(std::forward<H>(h), std::forward<R>(r)...);
  }
}

template <class H, class... As>
Block finish(H&& h, As&&... as) {
  return wrapCustomTags(std::forward<H>(h), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
