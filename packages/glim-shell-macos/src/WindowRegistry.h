#pragma once

#include <vector>

namespace glim::shell {

class Window;

namespace detail {

void registerShownWindow(Window*);
void unregisterShownWindow(Window*);
const std::vector<Window*>& shownWindows();

}  // namespace detail
}  // namespace glim::shell
