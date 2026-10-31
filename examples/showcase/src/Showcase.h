#pragma once

#include "glim/toolkit/State.h"

namespace glim {
namespace toolkit {

// Retained handles for the showcase screen. Trees borrow them,
// never own them.
struct Showcase {
  State<std::string> out;
};

}  // namespace toolkit
}  // namespace glim
