#pragma once

#include <cstdint>

namespace glim {
namespace toolkit {

// Idle-skip gate over Store::revision (see State.h). The harness
// Frame-tests check(); the paint site calls acknowledge(). State writes
// are the only automatic source: anything else that changes pixels
// without touching a cell (today only focus/capture flips, which pair
// with an immediate paint on their event) uses invalidate().
// T7 tweens will feed invalidate() when motion lands.
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
