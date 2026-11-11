#pragma once

#include <string>
#include <vector>

namespace glim {
namespace toolkit {
namespace host {

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
