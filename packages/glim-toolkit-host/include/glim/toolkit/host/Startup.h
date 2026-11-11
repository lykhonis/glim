#pragma once

// Startup: time to first frame, split into phases. Every mark is ms since
// the OS started the process, so work done before `main` (dyld, static
// initializers) shows up in the first phase instead of disappearing.
// The host records the marks under --startup, presents one frame, exits
// and prints the report; tools/startup.mjs repeats that for a median.

#include <string>
#include <vector>

namespace glim {
namespace toolkit {
namespace host {

// Ms since this process was started; falls back to ms since the first
// call when the platform does not expose a start time.
double processUptimeMs();

class StartupReport {
 public:
  // Ends the phase that began at the previous mark (or process start).
  void mark(const char* phase);
  void markAt(const char* phase, double atMs);
  // `startup <phase> <ms>` per phase, then `startup total <ms>`.
  std::string summary() const;
  double totalMs() const;

 private:
  struct Mark {
    std::string phase;
    double atMs;
  };
  std::vector<Mark> marks_;
};

}  // namespace host
}  // namespace toolkit
}  // namespace glim
