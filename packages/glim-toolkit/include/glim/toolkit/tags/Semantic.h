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

template <class H>
std::enable_if_t<std::is_base_of<WithSemantic, H>::value> apply(H& h, SemanticTag t) {
  h.semantic = std::move(t.id);
}

}  // namespace toolkit
}  // namespace glim
