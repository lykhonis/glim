#pragma once

#include <type_traits>

namespace glim {
namespace toolkit {

struct ExpandTag {};

inline ExpandTag expand() { return {}; }

template <>
struct is_tag<ExpandTag> : std::true_type {};

template <>
struct is_slot_tag<ExpandTag> : std::true_type {};

}  // namespace toolkit
}  // namespace glim
