#include <glim/toolkit.h>
#include <glim/toolkit/host/Host.h>
#include <glim/toolkit/host/Probe.h>

#include <iostream>

namespace host = glim::toolkit::host;

#if defined(__ANDROID__)
extern "C" int glim_app_main() {
  int argc = 0;
  char** argv = nullptr;
#else
int main(int argc, char** argv) {
#endif
  const host::E2eFlags flags = host::parseE2e(argc, argv);
  if (flags.help) {
    std::cout << "glim package host ok\n";
    return 0;
  }
  return host::run({.title = "Glim Package"}, [](const glim::toolkit::Engine&) {
    return glim::toolkit::lines("Hello from an installed Glim");
  });
}
