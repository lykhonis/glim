#pragma once

#include <algorithm>
#include <chrono>

namespace glim::examples {

// Best of many runs, so a preempted run on a loaded CI machine cannot
// fail the budget.
template <class Record>
double bestRecordMs(Record record) {
  using Clock = std::chrono::steady_clock;
  auto once = [&] {
    const auto t0 = Clock::now();
    record();
    return std::chrono::duration<double, std::milli>(Clock::now() - t0)
      .count();
  };
  for (int i = 0; i < 3; ++i) once();
  double best = once();
  for (int i = 1; i < 31; ++i) best = std::min(best, once());
  return best;
}

}  // namespace glim::examples
