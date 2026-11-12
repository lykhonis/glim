#include <glim/toolkit/host/Crash.h>

#include <cstdio>
#include <fstream>
#include <sstream>

#if !defined(__EMSCRIPTEN__)
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <unwind.h>

#include <cstdint>
#include <cstring>
#endif

namespace glim {
namespace toolkit {
namespace host {

#if !defined(__EMSCRIPTEN__)
namespace {

// Everything below runs inside a signal handler: write(2) on a
// pre-opened fd and no allocation.
constexpr int kSignals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP};
constexpr int kMaxFrames = 64;
int reportFd = -1;
struct sigaction previous[NSIG];
alignas(16) char altStack[64 * 1024];

void put(const char* text) {
  const ssize_t written = write(reportFd, text, std::strlen(text));
  (void)written;
}

void putNumber(std::uintptr_t value, unsigned base) {
  char digits[2 * sizeof(value) + 1];
  int at = sizeof(digits) - 1;
  digits[at] = '\0';
  do {
    digits[--at] = "0123456789abcdef"[value % base];
    value /= base;
  } while (value != 0);
  put(base == 16 ? "0x" : "");
  put(digits + at);
}

_Unwind_Reason_Code putFrame(_Unwind_Context* context, void* depth) {
  int& n = *static_cast<int*>(depth);
  const std::uintptr_t pc = _Unwind_GetIP(context);
  if (pc == 0) return _URC_END_OF_STACK;
  put("#");
  putNumber(static_cast<std::uintptr_t>(n), 10);
  put(" ");
  putNumber(pc, 16);
  Dl_info info{};
  if (dladdr(reinterpret_cast<void*>(pc), &info) != 0 && info.dli_fname != nullptr) {
    put(" ");
    put(info.dli_fname);
    put(" +");
    putNumber(pc - reinterpret_cast<std::uintptr_t>(info.dli_fbase), 16);
  }
  put("\n");
  return ++n < kMaxFrames ? _URC_NO_REASON : _URC_END_OF_STACK;
}

void onFatalSignal(int signal) {
  put("glim crash\nsignal ");
  putNumber(static_cast<std::uintptr_t>(signal), 10);
  put("\n");
  int depth = 0;
  _Unwind_Backtrace(putFrame, &depth);
  // Blocked until this handler returns, then delivered to whoever
  // handled it before us (or the default action).
  sigaction(signal, &previous[signal], nullptr);
  raise(signal);
}

}  // namespace

bool installCrashHandler(const std::string& path) {
  const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  if (fd < 0) return false;
  if (reportFd >= 0) close(reportFd);
  reportFd = fd;
  stack_t current{};
  if (sigaltstack(nullptr, &current) == 0 && (current.ss_flags & SS_DISABLE) != 0) {
    stack_t stack{};
    stack.ss_sp = altStack;
    stack.ss_size = sizeof(altStack);
    sigaltstack(&stack, nullptr);
  }
  struct sigaction action {};
  action.sa_handler = onFatalSignal;
  action.sa_flags = SA_ONSTACK;
  sigemptyset(&action.sa_mask);
  for (const int signal : kSignals) {
    struct sigaction old {};
    sigaction(signal, &action, &old);
    if (old.sa_handler != onFatalSignal) previous[signal] = old;
  }
  return true;
}
#else
bool installCrashHandler(const std::string&) { return false; }
#endif

std::optional<std::string> takeCrashReport(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  std::ostringstream text;
  text << in.rdbuf();
  in.close();
  std::remove(path.c_str());
  if (text.str().empty()) return std::nullopt;
  return text.str();
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
