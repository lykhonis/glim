#pragma once

#include <string>
#include <type_traits>
#include <utility>

namespace glim {
namespace toolkit {

struct SemanticTag {
  std::string id;
};

inline SemanticTag semantic(std::string id) { return {std::move(id)}; }

template <>
struct is_tag<SemanticTag> : std::true_type {};

struct WithSemantic {
  std::string semantic;
};

// Single-source detector for the a11y id carried on an element.
// Used by semanticOf (see Semantics.h) so record marks and paint
// marks read one home instead of each duplicating the field access.
template <class T, class = void>
struct has_semantic : std::false_type {};
template <class T>
struct has_semantic<T, std::void_t<decltype(std::declval<const T&>().semantic)>>
    : std::is_convertible<decltype(std::declval<const T&>().semantic), std::string> {};

template <class H>
std::enable_if_t<std::is_base_of<WithSemantic, H>::value> apply(H& h, SemanticTag t) {
  h.semantic = std::move(t.id);
}

}  // namespace toolkit
}  // namespace glim
