#pragma once

// Drive: scripted input replayed into the live window, with a latency
// report. Each step goes through the window's own event path once it
// is due, so a busy main thread delays it exactly as it delays real
// input; that delay is the reported lag.
//
// Script, one command per line; `#` starts a comment. Times are in
// seconds, points in logical px from the window's top-left:
//   wait <s>
//   click <x> <y> [count=1] [every=0.15]
//   drag <x0> <y0> <x1> <y1> [s=0.5] [hz=120]
//   key <enter|escape|back|up|down|left|right|center>
// After the last step the run drains for half a second, closes the
// window and prints the report. The app exits 1 when the run stalled,
// so a script doubles as a responsiveness gate.

#include <glim/shell/Event.h>
#include <glim/shell/Window.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "glim/toolkit/host/Telemetry.h"

namespace glim {
namespace toolkit {
namespace host {

struct DriveStep {
  float at = 0.f;  // seconds after the run starts
  shell::Event event;
};

struct DriveScript {
  std::vector<DriveStep> steps;
  std::string error;  // first bad line; empty when the script parsed
};

DriveScript parseDriveScript(std::string_view text);

// One drive run's measurements. Times are ms since the run started.
class DriveReport {
 public:
  // lagMs: due to dispatched. handleMs: the dispatch itself, including
  // any paint the host does synchronously for the event.
  void event(float atMs, shell::EventType type, float lagMs, float handleMs);
  void paint(float atMs, const FrameTelemetry& t);
  std::string summary() const;
  // An event waited, or the screen froze, longer than a person notices.
  bool stalled() const;
  // Chrome trace (chrome://tracing, Perfetto): events and paints as
  // slices on one track each.
  std::string traceJson() const;

 private:
  struct Event {
    float atMs, lagMs, handleMs;
    shell::EventType type;
  };
  struct Paint {
    float atMs, frameMs, paintMs, acquireMs, gpuMs;
  };
  std::vector<Event> events_;
  std::vector<Paint> paints_;
};

// A script bound to a window: dispatches due steps from this thread's
// run loop and feeds the report from the host's telemetry.
class DriveRun {
 public:
  // Null (with the reason on stderr) when the file is missing or bad.
  static std::unique_ptr<DriveRun> load(const char* path);
  explicit DriveRun(std::vector<DriveStep> steps) : steps_(std::move(steps)) {}

  void start(shell::Window& window);
  void paint(const FrameTelemetry& t);
  // Prints the summary and writes the trace when `tracePath` is set;
  // false when the run stalled.
  bool finish(const char* tracePath) const;

 private:
  // Seconds until the next step, or negative once the window closed.
  float poll(shell::Window& window);
  float sinceStartMs() const;

  std::vector<DriveStep> steps_;
  std::size_t next_ = 0;
  bool started_ = false;
  bool closed_ = false;
  std::chrono::steady_clock::time_point start_{};
  DriveReport report_;
};

}  // namespace host
}  // namespace toolkit
}  // namespace glim
