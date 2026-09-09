#include "WindowRegistry.h"

#include <algorithm>

namespace glim::shell::detail {
namespace {

thread_local std::vector<Window*> gShown;

}  // namespace

void registerShownWindow(Window* w) {
    if (!w) {
        return;
    }
    auto& list = gShown;
    if (std::find(list.begin(), list.end(), w) == list.end()) {
        list.push_back(w);
    }
}

void unregisterShownWindow(Window* w) {
    auto& list = gShown;
    list.erase(std::remove(list.begin(), list.end(), w), list.end());
}

const std::vector<Window*>& shownWindows() {
    return gShown;
}

}  // namespace glim::shell::detail
