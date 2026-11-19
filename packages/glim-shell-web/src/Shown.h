#pragma once

#include <vector>

namespace glim::shell {

class Window;

namespace detail {

// Shown windows, in show order; the main loop sends each a frame.
std::vector<Window*>& shownWindows();

}  // namespace detail
}  // namespace glim::shell
