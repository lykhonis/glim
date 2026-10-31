#pragma once

#include <functional>

namespace glim {
namespace toolkit {

using Action = std::function<void()>;

namespace detail {

inline bool invokeAction(const Action& a) {
  if (a) {
    a();
    return true;
  }
  return false;
}

}  // namespace detail

}  // namespace toolkit
}  // namespace glim
