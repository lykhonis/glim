#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "glim/toolkit.h"

namespace glim {
namespace toolkit {
namespace host {
class TelemetryHub;
struct FrameTelemetry;
}  // namespace host

Block showcaseTree(
    Scope app,
    const host::TelemetryHub* hub = nullptr,
    const std::vector<std::string>* log = nullptr
);

// Push-log line for one painted frame: "SEV Title |detail" so the
// feed parses the prefix ("! " hitch, "~ " warn, else info).
std::string telemetryLogLine(const host::FrameTelemetry& sample);

// Normalized load 0..1 for the tick ring (33ms maps to full).
float ringValue(const host::TelemetryHub* hub);

inline constexpr std::uint32_t kShowcaseSeatId = 7;

}  // namespace toolkit
}  // namespace glim
