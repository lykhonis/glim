#include <glim/shell/RunLoop.h>
#include <glim/toolkit/host/Drive.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#include <glim/trace.h>

#include "Percentile.h"

namespace glim {
namespace toolkit {
namespace host {

namespace {

using shell::EventType;

constexpr float kClickHoldSec = 0.05f;
constexpr float kDrainSec = 0.5f;
constexpr float kStallMs = 100.0f;

shell::Event pointer(EventType type, float x, float y) {
  shell::Event e(type);
  e.setPoint(x, y);
  e.setButton(shell::PointerButton::Left);
  e.setPointerId(1);
  return e;
}

bool keyNamed(const std::string& name, shell::Key& out) {
  static const std::pair<const char*, shell::Key> kKeys[] = {
    {"enter", shell::Key::Enter},
    {"escape", shell::Key::Escape},
    {"back", shell::Key::Back},
    {"up", shell::Key::DpadUp},
    {"down", shell::Key::DpadDown},
    {"left", shell::Key::DpadLeft},
    {"right", shell::Key::DpadRight},
    {"center", shell::Key::DpadCenter},
  };
  for (const auto& [n, k] : kKeys) {
    if (name == n) {
      out = k;
      return true;
    }
  }
  return false;
}

const char* eventName(EventType t) {
  switch (t) {
    case EventType::PointerDown:
      return "down";
    case EventType::PointerMove:
      return "move";
    case EventType::PointerUp:
      return "up";
    case EventType::KeyDown:
      return "key";
    default:
      return "other";
  }
}

void distribution(
  std::ostringstream& os, const char* name, const std::vector<float>& v, const char* note
) {
  char line[160];
  std::snprintf(
    line,
    sizeof(line),
    "  %-8s p50 %7.2f  p95 %7.2f  p99 %7.2f  max %7.2f ms   %s\n",
    name,
    static_cast<double>(detail::percentile(v, 50)),
    static_cast<double>(detail::percentile(v, 95)),
    static_cast<double>(detail::percentile(v, 99)),
    static_cast<double>(detail::percentile(v, 100)),
    note
  );
  os << line;
}

}  // namespace

DriveScript parseDriveScript(std::string_view text) {
  DriveScript out;
  float t = 0.f;
  std::istringstream lines{std::string(text)};
  std::string line;
  int number = 0;
  while (std::getline(lines, line)) {
    ++number;
    line = line.substr(0, line.find('#'));
    std::istringstream in(line);
    std::string cmd;
    if (!(in >> cmd)) {
      continue;
    }
    auto fail = [&] {
      out.error = "line " + std::to_string(number) + ": " + line;
      out.steps.clear();
      return out;
    };
    if (cmd == "wait") {
      float s = 0.f;
      if (!(in >> s) || s < 0.f) return fail();
      t += s;
    } else if (cmd == "click") {
      float x = 0.f, y = 0.f, every = 0.15f;
      int count = 1;
      if (!(in >> x >> y)) return fail();
      if (in >> count) in >> every;
      if (count < 1 || every < kClickHoldSec) return fail();
      for (int i = 0; i < count; ++i) {
        out.steps.push_back({t, pointer(EventType::PointerDown, x, y)});
        out.steps.push_back({t + kClickHoldSec, pointer(EventType::PointerUp, x, y)});
        t += every;
      }
    } else if (cmd == "drag") {
      float x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f, s = 0.5f, hz = 120.f;
      if (!(in >> x0 >> y0 >> x1 >> y1)) return fail();
      if (in >> s) in >> hz;
      if (s <= 0.f || hz <= 0.f) return fail();
      const int n = std::max(1, static_cast<int>(s * hz));
      out.steps.push_back({t, pointer(EventType::PointerDown, x0, y0)});
      for (int i = 1; i <= n; ++i) {
        const float f = static_cast<float>(i) / static_cast<float>(n);
        out.steps.push_back(
          {t + s * f, pointer(EventType::PointerMove, x0 + (x1 - x0) * f, y0 + (y1 - y0) * f)}
        );
      }
      t += s;
      out.steps.push_back({t, pointer(EventType::PointerUp, x1, y1)});
    } else if (cmd == "key") {
      std::string name;
      shell::Key key{};
      if (!(in >> name) || !keyNamed(name, key)) return fail();
      shell::Event e(EventType::KeyDown);
      e.setKey(key);
      out.steps.push_back({t, e});
    } else {
      return fail();
    }
  }
  return out;
}

void DriveReport::event(float atMs, EventType type, float lagMs, float handleMs) {
  events_.push_back({atMs, lagMs, handleMs, type});
}

void DriveReport::paint(float atMs, const FrameTelemetry& t) {
  paints_.push_back(
    {atMs, t.frameMs, t.buildMs + t.recordMs + t.encodeMs, t.stats.acquireMs, t.stats.gpuMs}
  );
}

std::string DriveReport::summary() const {
  std::vector<float> lag, handle, frame, paint, acquire, gpu;
  std::size_t lagStalls = 0, frameStalls = 0;
  for (const Event& e : events_) {
    lag.push_back(e.lagMs);
    handle.push_back(e.handleMs);
    lagStalls += e.lagMs > kStallMs ? 1 : 0;
  }
  for (const Paint& p : paints_) {
    frame.push_back(p.frameMs);
    paint.push_back(p.paintMs);
    acquire.push_back(p.acquireMs);
    if (p.gpuMs > 0.f) {
      gpu.push_back(p.gpuMs);
    }
    frameStalls += p.frameMs > kStallMs ? 1 : 0;
  }
  const float spanMs = std::max(
    events_.empty() ? 0.f : events_.back().atMs, paints_.empty() ? 0.f : paints_.back().atMs
  );
  std::ostringstream os;
  char head[160];
  std::snprintf(
    head,
    sizeof(head),
    "drive: %zu events, %zu paints over %.1f s\n",
    events_.size(),
    paints_.size(),
    static_cast<double>(spanMs / 1000.f)
  );
  os << head;
  distribution(os, "lag", lag, "due -> dispatched (main-thread backlog)");
  distribution(os, "handle", handle, "event dispatch, incl. synchronous paint");
  distribution(os, "frame", frame, "interval between paints");
  distribution(os, "paint", paint, "build + record + encode");
  distribution(os, "acquire", acquire, "waiting for a drawable (GPU/display behind)");
  if (!gpu.empty()) {
    distribution(os, "gpu", gpu, "GPU execution per frame (budget: one vsync)");
  }
  os << "  stalls   " << lagStalls << " events lagged, " << frameStalls << " frame gaps over "
     << kStallMs << " ms\n";
  return os.str();
}

bool DriveReport::stalled() const {
  return std::any_of(
           events_.begin(), events_.end(), [](const Event& e) { return e.lagMs > kStallMs; }
         ) ||
         std::any_of(paints_.begin(), paints_.end(), [](const Paint& p) {
           return p.frameMs > kStallMs;
         });
}

std::string DriveReport::traceJson() const {
  std::ostringstream os;
  trace::JsonWriter trace(os);
  auto slice = [&](const char* name, int tid, float startMs, float durMs, const std::string& args) {
    trace.slice(
      name,
      tid,
      static_cast<long long>(startMs * 1000.f),
      static_cast<long long>(std::max(durMs, 0.001f) * 1000.f),
      args
    );
  };
  for (const Event& e : events_) {
    slice(eventName(e.type), 1, e.atMs, e.handleMs, "\"lagMs\":" + std::to_string(e.lagMs));
  }
  for (const Paint& p : paints_) {
    slice(
      "paint",
      2,
      p.atMs - p.paintMs,
      p.paintMs,
      "\"acquireMs\":" + std::to_string(p.acquireMs) + ",\"gpuMs\":" + std::to_string(p.gpuMs) +
        ",\"frameMs\":" + std::to_string(p.frameMs)
    );
  }
  trace.close();
  return os.str();
}

std::unique_ptr<DriveRun> DriveRun::load(const char* path) {
  std::ifstream file(path);
  if (!file) {
    std::cerr << "drive: cannot read " << path << "\n";
    return nullptr;
  }
  std::stringstream text;
  text << file.rdbuf();
  DriveScript script = parseDriveScript(text.str());
  if (!script.error.empty()) {
    std::cerr << "drive: " << path << " " << script.error << "\n";
    return nullptr;
  }
  return std::make_unique<DriveRun>(std::move(script.steps));
}

float DriveRun::sinceStartMs() const {
  return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start_)
    .count();
}

void DriveRun::start(shell::Window& window) {
  shell::RunLoop().scheduleRepeatedTask([this, &window](shell::RunLoop::TaskContext& ctx) {
    const float next = poll(window);
    if (next < 0.f) {
      ctx.cancel();
      return;
    }
    shell::RunLoop().scheduleTimer(std::chrono::duration<float>(next));
  });
}

float DriveRun::poll(shell::Window& window) {
  if (closed_) {
    return -1.f;
  }
  if (!started_) {
    started_ = true;
    start_ = std::chrono::steady_clock::now();
  }
  // Everything due goes out now, back to back, the way a backed-up
  // OS event queue drains.
  while (next_ < steps_.size() && sinceStartMs() >= steps_[next_].at * 1000.f) {
    const DriveStep& s = steps_[next_++];
    const float dispatchMs = sinceStartMs();
    window.dispatch(s.event);
    report_.event(
      dispatchMs, s.event.type(), dispatchMs - s.at * 1000.f, sinceStartMs() - dispatchMs
    );
  }
  const float endSec = (steps_.empty() ? 0.f : steps_.back().at) + kDrainSec;
  const float nowSec = sinceStartMs() / 1000.f;
  if (next_ < steps_.size()) {
    return std::max(0.f, steps_[next_].at - nowSec);
  }
  if (nowSec < endSec) {
    return endSec - nowSec;
  }
  closed_ = true;
  window.dispatch(shell::Event(EventType::WindowClosed));
  return -1.f;
}

void DriveRun::paint(const FrameTelemetry& t) {
  if (started_ && !closed_) {
    report_.paint(sinceStartMs(), t);
  }
}

bool DriveRun::finish(const char* tracePath) const {
  std::cout << report_.summary();
  if (tracePath && *tracePath) {
    std::ofstream out(tracePath);
    out << report_.traceJson();
    std::cout << "  trace    " << tracePath << "\n";
  }
  return !report_.stalled();
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
