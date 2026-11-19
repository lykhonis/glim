#pragma once

#include <string>
#include <vector>

namespace glim::shell {

class Window;

namespace detail {

// Shown windows, in show order; the main loop sends each a frame.
std::vector<Window*>& shownWindows();

// Sends the canvas's pointer events and the page's keys to the window.
void listen(Window* window, const std::string& selector);
// Whether the text bridge holds focus; keys then belong to it.
bool textInputFocused();

}  // namespace detail
}  // namespace glim::shell
