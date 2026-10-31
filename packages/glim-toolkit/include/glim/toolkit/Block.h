#pragma once

#include <type_traits>
#include <utility>
#include <variant>

#include "glim/toolkit/Custom.h"
#include "glim/toolkit/Layout.h"
#include "glim/toolkit/elements/Column.h"
#include "glim/toolkit/elements/Empty.h"
#include "glim/toolkit/elements/Fill.h"
#include "glim/toolkit/elements/Lines.h"
#include "glim/toolkit/elements/List.h"
#include "glim/toolkit/elements/Roll.h"
#include "glim/toolkit/elements/Row.h"
#include "glim/toolkit/elements/Stack.h"
#include "glim/toolkit/elements/Tap.h"

namespace glim {
namespace toolkit {

struct Block {
  using Variant = std::variant<Empty, Row, Column, Stack, Lines, List, Roll, Fill, Tap, Custom>;
  Variant v;
  Block() : v(Empty{}) {}
  Block(Empty e) : v(std::move(e)) {}
  Block(Row r) : v(std::move(r)) {}
  Block(Column c) : v(std::move(c)) {}
  Block(Stack s) : v(std::move(s)) {}
  Block(Lines l) : v(std::move(l)) {}
  Block(List l) : v(std::move(l)) {}
  Block(Roll r) : v(std::move(r)) {}
  Block(Fill f) : v(std::move(f)) {}
  Block(Tap t) : v(std::move(t)) {}
  Block(Custom c) : v(std::move(c)) {}
  template <class T>
  const T* get() const {
    return std::get_if<T>(&v);
  }
};

template <class T, class = void>
struct has_view : std::false_type {};
template <class T>
struct has_view<T, std::void_t<decltype(std::declval<const T&>().view())>>
    : std::is_same<decltype(std::declval<const T&>().view()), Block> {};

template <class T, class = void>
struct has_place : std::false_type {};
template <class T>
struct has_place<T, std::void_t<decltype(std::declval<const T&>().place(
                       std::declval<Constraints>(), std::declval<const Engine&>()))>>
    : std::is_same<decltype(std::declval<const T&>().place(
                       std::declval<Constraints>(), std::declval<const Engine&>())),
                   Box> {};

template <class T, class = void>
struct has_record : std::false_type {};
template <class T>
struct has_record<T, std::void_t<decltype(std::declval<const T&>().record(
                        std::declval<const Box&>(), std::declval<Rec&>(),
                        std::declval<const Engine&>()))>> : std::true_type {};

template <class T>
struct is_block : std::is_same<std::decay_t<T>, Block> {};
template <class T>
struct is_block_arm
    : std::integral_constant<bool,
                             std::is_same<std::decay_t<T>, Empty>::value ||
                                 std::is_same<std::decay_t<T>, Row>::value ||
                                 std::is_same<std::decay_t<T>, Column>::value ||
                                 std::is_same<std::decay_t<T>, Stack>::value ||
                                 std::is_same<std::decay_t<T>, Lines>::value ||
                                 std::is_same<std::decay_t<T>, List>::value ||
                                 std::is_same<std::decay_t<T>, Roll>::value ||
                                 std::is_same<std::decay_t<T>, Fill>::value ||
                                 std::is_same<std::decay_t<T>, Tap>::value ||
                                 std::is_same<std::decay_t<T>, Custom>::value> {};
template <class T>
struct is_child_type
    : std::integral_constant<bool, is_block<T>::value || is_block_arm<T>::value ||
                                       has_view<std::decay_t<T>>::value ||
                                       (has_place<std::decay_t<T>>::value &&
                                        has_record<std::decay_t<T>>::value)> {};

template <class T>
struct always_false : std::false_type {};

template <class T>
Block toBlock(T&& t) {
  using U = std::decay_t<T>;
  if constexpr (is_block<T>::value) {
    return std::forward<T>(t);
  } else if constexpr (is_block_arm<T>::value) {
    return Block{std::forward<T>(t)};
  } else if constexpr (has_view<U>::value) {
    return std::forward<T>(t).view();
  } else if constexpr (has_place<U>::value && has_record<U>::value) {
    return Block{Custom{std::forward<T>(t)}};
  } else {
    static_assert(always_false<U>::value,
                  "child must be Block, an element, provide 'Block view() const', or "
                  "provide 'Box place(Constraints, const Engine&) const' + 'void record(const "
                  "Box&, Rec&, const Engine&) const' (see elements/Row.h)");
  }
}

}  // namespace toolkit
}  // namespace glim
