#pragma once

#include <glim/math.h>
#include <glim/paint/Context.h>

#include <cstddef>
#include <functional>

namespace glim::examples {

// GPU-vs-CPU parity band for one scene: channels allowed over delta 2,
// and the largest single-channel delta. Measured, then pinned with a
// date.
struct GpuBand {
  std::size_t maxOverDelta = 0;
  int maxChannelDelta = 0;
};

using RecordScene = std::function<void(paint::Context&, Vec2)>;

// Headless device for the build's GPU backend → drawToTarget →
// readPixels, compared to the CPU golden (argv[1], else defaultGolden).
// Returns exit code.
int runGpuGolden(
  int argc,
  char** argv,
  const char* defaultGolden,
  GpuBand band,
  const RecordScene& record
);

}  // namespace glim::examples
