#pragma once

#include "glim/toolkit/State.h"

namespace glim {
namespace toolkit {

// Retained handles for the showcase screen. Trees borrow them,
// never own them.
struct Showcase {
  State<std::string> out;
  State<bool> wifi;
  State<bool> sound;
  State<int> range;
  State<float> level;
  State<int> span;
  State<int> mode;
  State<int> viewSel;
  State<int> tabSel;
  State<std::string> notice;
};

}  // namespace toolkit
}  // namespace glim
