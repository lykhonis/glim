#pragma once

// Probe: one command surface over every layer of a headless Glim app —
// tree, layout, a11y, ops, paint stats, CPU and GPU frames, timing,
// input, and pixel comparison against a reference design. Drive it from
// a script file, an interactive stdin session (agents or people), or
// `--e2e` tokens. Every command answers with its output lines followed
// by `ok` or `err <reason>`. `help` lists the commands.

#include <glim/image/Analyze.h>
#include <glim/image/Image.h>
#include <glim/toolkit/host/Startup.h>
#include <glim/toolkit/host/TestApp.h>

#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace glim {
namespace toolkit {
namespace host {

class Probe {
 public:
  Probe(TestApp& app, std::ostream& out);
  ~Probe();
  Probe(const Probe&) = delete;
  Probe& operator=(const Probe&) = delete;

  // Runs one command line; false when it answered `err`.
  bool exec(std::string_view line);
  // Runs lines until EOF (or the first error when stopOnError).
  bool run(std::istream& in, bool stopOnError);

 private:
  struct Gpu;
  struct Frame {
    image::Image pixels;
    bool gpu = false;
    bool valid = false;
  };

  bool dispatch(const std::vector<std::string>& args, const std::string& rest);
  bool render(bool gpu);
  const Frame& current();
  const image::Image* pick(const std::string& which);
  bool compare(const std::string& label);
  bool bench(int frames);
  std::string artifact(const std::string& name) const;

  TestApp& app_;
  std::ostream& out_;
  std::string outDir_ = "/tmp/glim-probe";
  image::Image ref_;
  std::vector<std::pair<std::string, image::Region>> regions_;
  struct Item {
    std::string name;
    image::Region search;
    image::Mask mask;
  };
  std::vector<Item> items_;
  Frame frame_;
  std::unique_ptr<Gpu> gpu_;
};

// `--e2e` token (`tap:i:1`, `snap:out.png`, `~text`) as a probe command line.
std::string probeLineFromE2e(std::string_view token);

// Headless command-line driver shared by every app binary.
// Flags every app accepts; apps append their own after it.
inline constexpr const char* kE2eUsage =
  "[--snapshot out.png] [--dump-a11y] [--dump-ops out.bin] [--stats]\n"
  "  [--e2e \"tap:i:1;step:0.5;~TEXT\"] [--probe script|-]\n"
  "  [--trace-frames N [--trace-out out.csv] [--trace-json out.json]] [--soak N]\n"
  "  [--drive script [--drive-trace out.json]] [--startup | --startup-encoded]";

struct E2eFlags {
  bool help = false;
  std::string script, snapshot;
  std::string probe;  // script path, or "-" for an interactive stdin session
  bool dumpA11y = false;
  std::string dumpOps;
  bool stats = false;
  int traceFrames = 0;
  std::string traceOut;
  std::string traceJson;
  int soak = 0;
  // Windowed, not headless: hand these to Config::drive/driveTrace.
  std::string drive, driveTrace;
  StartupUntil startup = StartupUntil::Off;
  bool any() const {
    return !script.empty() || !snapshot.empty() || !probe.empty() || dumpA11y || !dumpOps.empty() ||
           stats || traceFrames > 0 || !traceOut.empty() || soak > 0;
  }
};
E2eFlags parseE2e(int argc, char** argv);
int runE2e(TestApp& app, const E2eFlags& f);

}  // namespace host
}  // namespace toolkit
}  // namespace glim
