#include <glim/trace.h>

#include <ostream>

namespace glim::trace {

JsonWriter::JsonWriter(std::ostream& os) : os_(os) { os_ << "{\"traceEvents\":[\n"; }

void JsonWriter::next() {
  os_ << (first_ ? "" : ",\n");
  first_ = false;
}

void JsonWriter::slice(
  const char* name, int tid, long long tsUs, long long durUs, const std::string& args
) {
  next();
  os_ << "{\"name\":\"" << name << "\",\"ph\":\"X\",\"pid\":1,\"tid\":" << tid << ",\"ts\":" << tsUs
      << ",\"dur\":" << durUs << ",\"args\":{" << args << "}}";
}

void JsonWriter::counter(const char* name, long long tsUs, double value) {
  next();
  os_ << "{\"name\":\"" << name << "\",\"ph\":\"C\",\"pid\":1,\"ts\":" << tsUs
      << ",\"args\":{\"value\":" << value << "}}";
}

void JsonWriter::close() { os_ << "\n],\"displayTimeUnit\":\"ms\"}\n"; }

}  // namespace glim::trace

#if defined(__wasi__) && !defined(__wasm_atomics__)

namespace glim::trace {

bool enabled() { return false; }
void setEnabled(bool) {}
bool startFile(const char*) { return false; }
void stopFile() {}
Token begin(const char*) { return {}; }
void end(const char*, Token) {}
void counter(const char*, double) {}

}  // namespace glim::trace

#else

#include <atomic>
#include <chrono>
#include <fstream>
#include <optional>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <thread>

#if defined(__APPLE__)
#include <os/signpost.h>
#elif defined(__ANDROID__)
#include <android/trace.h>
#include <dlfcn.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>

EM_JS(void, glimMarkBegin, (const char* name), { performance.mark(UTF8ToString(name) + ':b'); });
EM_JS(void, glimMarkEnd, (const char* name), {
  const n = UTF8ToString(name);
  performance.measure(n, n + ':b');
});
#endif

namespace glim::trace {
namespace {

std::atomic<bool> forced{false};
std::atomic<bool> fileOn{false};

struct File {
  std::mutex mutex;
  std::ofstream out;
  std::optional<JsonWriter> json;
};

File traceFile;

File& file() { return traceFile; }

std::int64_t nowUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
           std::chrono::steady_clock::now().time_since_epoch()
  )
    .count();
}

int threadId() {
  return static_cast<int>(std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xffff);
}

template <class Fn>
void withJson(Fn&& fn) {
  File& f = file();
  std::lock_guard lock(f.mutex);
  if (f.json) fn(*f.json);
}

#if defined(__APPLE__)
os_log_t appleLog() {
  static os_log_t log = os_log_create("dev.glim", OS_LOG_CATEGORY_POINTS_OF_INTEREST);
  return log;
}
#endif

bool platformOn() {
#if defined(__APPLE__)
  return os_signpost_enabled(appleLog());
#elif defined(__ANDROID__)
  return ATrace_isEnabled();
#else
  return false;
#endif
}

void startFromEnv() {
  static const bool once = [] {
    if (const char* path = std::getenv("GLIM_TRACE"); path && *path) startFile(path);
    return true;
  }();
  (void)once;
}

}  // namespace

bool enabled() {
  startFromEnv();
  return fileOn.load(std::memory_order_relaxed) || forced.load(std::memory_order_relaxed) ||
         platformOn();
}

void setEnabled(bool on) { forced.store(on, std::memory_order_relaxed); }

bool startFile(const char* path) {
  File& f = file();
  std::lock_guard lock(f.mutex);
  if (f.json) return false;
  f.out.open(path);
  if (!f.out) return false;
  f.json.emplace(f.out);
  fileOn.store(true, std::memory_order_relaxed);
  static const bool registered = [] { return std::atexit(stopFile) == 0; }();
  (void)registered;
  return true;
}

void stopFile() {
  File& f = file();
  std::lock_guard lock(f.mutex);
  fileOn.store(false, std::memory_order_relaxed);
  if (!f.json) return;
  f.json->close();
  f.json.reset();
  f.out.close();
}

Token begin([[maybe_unused]] const char* name) {
  Token t{0, nowUs()};
#if defined(__APPLE__)
  if (os_signpost_enabled(appleLog()) || forced.load(std::memory_order_relaxed)) {
    t.id = os_signpost_id_generate(appleLog());
    os_signpost_interval_begin(appleLog(), t.id, "glim", "%{public}s", name);
  }
#elif defined(__ANDROID__)
  ATrace_beginSection(name);
#elif defined(__EMSCRIPTEN__)
  if (forced.load(std::memory_order_relaxed)) {
    t.id = 1;
    glimMarkBegin(name);
  }
#endif
  return t;
}

void end(const char* name, Token token) {
#if defined(__APPLE__)
  if (token.id != 0) os_signpost_interval_end(appleLog(), token.id, "glim", "%{public}s", name);
#elif defined(__ANDROID__)
  ATrace_endSection();
#elif defined(__EMSCRIPTEN__)
  if (token.id != 0) {
    glimMarkEnd(name);
  }
#endif
  if (fileOn.load(std::memory_order_relaxed)) {
    const std::int64_t dur = nowUs() - token.startUs;
    withJson([&](JsonWriter& j) { j.slice(name, threadId(), token.startUs, dur); });
  }
}

void counter(const char* name, double value) {
#if defined(__APPLE__)
  if (os_signpost_enabled(appleLog()) || forced.load(std::memory_order_relaxed)) {
    os_signpost_event_emit(
      appleLog(), OS_SIGNPOST_ID_EXCLUSIVE, "counter", "%{public}s=%f", name, value
    );
  }
#elif defined(__ANDROID__)
  using SetCounter = void (*)(const char*, std::int64_t);
  static const auto setCounter =
    reinterpret_cast<SetCounter>(dlsym(RTLD_DEFAULT, "ATrace_setCounter"));
  if (setCounter) setCounter(name, static_cast<std::int64_t>(value));
#endif
  if (fileOn.load(std::memory_order_relaxed)) {
    withJson([&](JsonWriter& j) { j.counter(name, nowUs(), value); });
  }
}

}  // namespace glim::trace

#endif
