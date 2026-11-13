#pragma once

// The villa photographs registered with the paint context: one camera,
// three lightings. The backdrop cover-blits and blends them; an id of 0
// means that photo is missing (headless, or not yet loaded).

#include <cstdint>

namespace glim {
namespace toolkit {

inline constexpr const char* kVillaKey = "villa";

struct VillaPlate {
  std::uint32_t id = 0;
  int w = 0;
  int h = 0;
};

struct VillaPlates {
  VillaPlate day;
  VillaPlate night;
  VillaPlate lit;               // night, every house light on
  const void* owner = nullptr;  // ids belong to one Context
};

}  // namespace toolkit
}  // namespace glim
