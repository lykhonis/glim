#include <glim/toolkit/host/Startup.h>

#include <chrono>
#include <cstdio>
#include <sstream>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/time.h>
#include <unistd.h>
#elif defined(__linux__)
#include <time.h>
#include <unistd.h>

#include <fstream>
#endif

namespace glim {
namespace toolkit {
namespace host {

namespace {

double sinceFirstCallMs() {
  static const auto first = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - first)
      .count();
}

}  // namespace

double processUptimeMs() {
#if defined(__APPLE__)
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  kinfo_proc info{};
  size_t size = sizeof(info);
  if (sysctl(mib, 4, &info, &size, nullptr, 0) == 0) {
    timeval now{};
    gettimeofday(&now, nullptr);
    const timeval& start = info.kp_proc.p_starttime;
    return static_cast<double>(now.tv_sec - start.tv_sec) * 1000.0 +
           static_cast<double>(now.tv_usec - start.tv_usec) / 1000.0;
  }
#elif defined(__linux__)
  // Field 22 of /proc/self/stat: start time in clock ticks after boot.
  std::ifstream stat("/proc/self/stat");
  std::string text((std::istreambuf_iterator<char>(stat)), std::istreambuf_iterator<char>());
  const auto paren = text.rfind(')');
  if (paren != std::string::npos) {
    std::istringstream fields(text.substr(paren + 2));
    std::string field;
    for (int i = 3; i < 22 && fields >> field; ++i) {
    }
    unsigned long long ticks = 0;
    timespec boot{};
    if (fields >> ticks && clock_gettime(CLOCK_BOOTTIME, &boot) == 0) {
      const double nowMs = static_cast<double>(boot.tv_sec) * 1000.0 + boot.tv_nsec / 1e6;
      return nowMs - static_cast<double>(ticks) * 1000.0 / sysconf(_SC_CLK_TCK);
    }
  }
#endif
  return sinceFirstCallMs();
}

void StartupReport::mark(const char* phase) { markAt(phase, processUptimeMs()); }

void StartupReport::markAt(const char* phase, double atMs) { marks_.push_back({phase, atMs}); }

double StartupReport::totalMs() const { return marks_.empty() ? 0.0 : marks_.back().atMs; }

std::string StartupReport::summary() const {
  std::ostringstream os;
  char line[96];
  double prev = 0.0;
  for (const Mark& m : marks_) {
    std::snprintf(line, sizeof(line), "startup %-12s %8.1f\n", m.phase.c_str(), m.atMs - prev);
    os << line;
    prev = m.atMs;
  }
  std::snprintf(line, sizeof(line), "startup %-12s %8.1f\n", "total", totalMs());
  os << line;
  return os.str();
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
