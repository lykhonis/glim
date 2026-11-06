#pragma once

// Golden helpers: strict mode (GLIM_GOLDEN_STRICT=1, CI) fails on missing
// golden; local runs write-and-pass. GOT artifact goes to GLIM_GOT_DIR.
#include <glim/image/Image.h>
#include <glim/image/Png.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace glim::image {

inline bool goldenStrict() {
  if (const char* v = std::getenv("GLIM_GOLDEN_STRICT")) {
    return v[0] != '\0' && std::strcmp(v, "0") != 0;
  }
  return false;
}

inline const char* gotDir() { return std::getenv("GLIM_GOT_DIR"); }

inline void writeGot(const char* testName, const Image& img) {
  const char* dir = gotDir();
  if (!dir || !dir[0]) return;
  std::string name = testName ? testName : "golden";
  const std::size_t slash = name.find_last_of('/');
  if (slash != std::string::npos) name = name.substr(slash + 1);
  Png::write((std::string(dir) + "/" + name + "-got.png").c_str(), img);
}

// CPU-golden check with per-channel tolerance 2; writes GOT artifact on mismatch.
inline bool checkCpuGolden(
    const Image& got, const Image& want, const char* testName, const char* what
) {
  const Diff diff = compare(got, want);
  if (diff.match()) return true;
  if (!diff.sameSize) {
    std::cerr << what << " golden size mismatch\n";
  } else {
    std::cerr << what << " golden mismatch: " << diff.overDelta << " channels differ by >2\n";
  }
  writeGot(testName, got);
  return false;
}

}  // namespace glim::image
