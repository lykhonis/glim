#pragma once

// Sample percentiles shared by the drive report and the probe.
#include <algorithm>
#include <vector>

namespace glim {
namespace toolkit {
namespace host {
namespace detail {

// Nearest-rank percentile of an unsorted sample; p = 50 is the upper median.
template <class T>
T percentile(std::vector<T> v, float p) {
  if (v.empty()) return T{};
  std::sort(v.begin(), v.end());
  const auto rank = static_cast<std::size_t>(p / 100.f * static_cast<float>(v.size() - 1) + 0.5f);
  return v[std::min(rank, v.size() - 1)];
}

}  // namespace detail
}  // namespace host
}  // namespace toolkit
}  // namespace glim
