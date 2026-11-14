#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

// One trace seam for every layer. Spans and counters go to the platform
// profiler when it is recording (os_signpost on Apple, ATrace on Android,
// performance.measure on the web once enabled) and to a Chrome trace JSON
// file when GLIM_TRACE=<path> is set or startFile() is called; open it in
// ui.perfetto.dev. Off, a span costs one relaxed atomic load.
namespace glim::trace {

bool enabled();
// Forces platform markers on; the web has no recorder to detect.
void setEnabled(bool on);

bool startFile(const char* path);
void stopFile();

struct Token {
  std::uint64_t id = 0;
  std::int64_t startUs = 0;
};

Token begin(const char* name);
void end(const char* name, Token token);
void counter(const char* name, double value);

// Chrome Trace Event JSON: the one home for the format.
class JsonWriter {
 public:
  explicit JsonWriter(std::ostream& os);
  void slice(
    const char* name, int tid, long long tsUs, long long durUs, const std::string& args = {}
  );
  void counter(const char* name, long long tsUs, double value);
  void close();

 private:
  void next();
  std::ostream& os_;
  bool first_ = true;
};

class Span {
 public:
  explicit Span(const char* name) : name_(name), on_(enabled()) {
    if (on_) token_ = begin(name_);
  }
  ~Span() {
    if (on_) end(name_, token_);
  }
  Span(const Span&) = delete;
  Span& operator=(const Span&) = delete;

 private:
  const char* name_;
  bool on_;
  Token token_{};
};

}  // namespace glim::trace
