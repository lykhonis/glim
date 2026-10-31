#pragma once

#include "glim/toolkit/State.h"

namespace glim {
namespace toolkit {

// Retained handles for the showcase screen. Built once per process
// (test) or per run (windowed); trees borrow them, never own them.
struct Showcase {
  State<std::string> out;
  State<int> answer;
};

}  // namespace toolkit
}  // namespace glim
