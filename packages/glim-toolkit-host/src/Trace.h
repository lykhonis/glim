#pragma once

// Profiling output shared by the live drive report and the headless
// trace: Chrome Trace Event slices and sample percentiles.
#include <algorithm>
#include <ostream>
#include <string>
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

// Chrome Trace Events (ph:X complete slices); open in Perfetto or speedscope.
class TraceWriter {
 public:
  explicit TraceWriter(std::ostream& os) : os_(os) { os_ << "{\"traceEvents\":[\n"; }

  void slice(
      const char* name, int tid, long long tsUs, long long durUs, const std::string& args = {}
  ) {
    os_ << (first_ ? "" : ",\n") << "{\"name\":\"" << name
        << "\",\"ph\":\"X\",\"pid\":1,\"tid\":" << tid << ",\"ts\":" << tsUs << ",\"dur\":" << durUs
        << ",\"args\":{" << args << "}}";
    first_ = false;
  }
  void close() { os_ << "\n],\"displayTimeUnit\":\"ms\"}\n"; }

 private:
  std::ostream& os_;
  bool first_ = true;
};

}  // namespace detail
}  // namespace host
}  // namespace toolkit
}  // namespace glim
