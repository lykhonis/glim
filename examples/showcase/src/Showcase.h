#pragma once

#include <cstdint>

#include "glim/toolkit/Scope.h"

namespace glim {
namespace toolkit {

// The showcase screen: one free function of Scope.
Block showcaseTree(Scope app);

inline constexpr std::uint32_t kShowcaseSeatId = 7;

}  // namespace toolkit
}  // namespace glim
