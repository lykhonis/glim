#pragma once

#include <string>
#include <vector>

namespace glim {
namespace toolkit {
namespace host {

// Where a --startup measurement ends: the first presented frame, or the
// first submitted one where nothing reaches a screen (hosted CI VMs).
enum class StartupUntil { Off, Presented, Encoded };

double processUptimeMs();

class StartupReport {
 public:
  void mark(const char* phase);
  void markAt(const char* phase, double atMs);
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
