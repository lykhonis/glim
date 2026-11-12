#pragma once

#include <functional>
#include <optional>
#include <string>

namespace glim {
namespace toolkit {
namespace host {

// Field crash capture. A fatal signal appends a plain-text report to
// `path`: the signal, then one line per frame as
// "#n 0x<pc> <module> +0x<offset>", which llvm-symbolizer or atos turn
// into source lines with the app's own symbols. Handlers installed
// before this one still run afterwards, so crash SDKs keep working.
// Returns false where signals are unavailable (web).
bool installCrashHandler(const std::string& path);

// The report a previous run left at `path`, removed once read.
std::optional<std::string> takeCrashReport(const std::string& path);

// Delivered once, at the next launch, with the previous run's report.
using CrashSink = std::function<void(const std::string& report)>;

}  // namespace host
}  // namespace toolkit
}  // namespace glim
