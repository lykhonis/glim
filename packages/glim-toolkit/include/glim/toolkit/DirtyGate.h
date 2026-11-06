#pragma once

#include <cstdint>

namespace glim {
namespace toolkit {

// Idle-skip gate over Store::revision; non-State pixel changes use invalidate().
struct DirtyGate {
  bool check(std::uint64_t revision) const { return invalidated_ || revision != painted_; }
  void acknowledge(std::uint64_t revision) {
    painted_ = revision;
    invalidated_ = false;
  }
  void invalidate() { invalidated_ = true; }

 private:
  std::uint64_t painted_ = 0;
  bool invalidated_ = true;
};

}  // namespace toolkit
}  // namespace glim
