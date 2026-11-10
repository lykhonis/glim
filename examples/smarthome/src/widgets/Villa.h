#pragma once

// The villa photograph registered with the paint context. The backdrop
// cover-blits it; 0 means the wash stands in (headless, or not yet
// loaded).

#include <cstdint>

namespace glim {
namespace toolkit {

inline constexpr const char* kVillaKey = "villa";

struct VillaPlate {
  std::uint32_t id = 0;
  int w = 0;
  int h = 0;
  const void* owner = nullptr;  // ids belong to one Context
};

}  // namespace toolkit
}  // namespace glim
