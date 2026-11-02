#pragma once

#include <cstdint>

#include "glim/toolkit/Scope.h"

namespace glim {
namespace toolkit {

// The showcase screen: one free function of Scope. States allocate
// where they render; the host owns the Store (or tests inject one).
Block showcaseTree(Scope app);

inline constexpr std::uint32_t kShowcaseSeatId = 7;

}  // namespace toolkit
}  // namespace glim
