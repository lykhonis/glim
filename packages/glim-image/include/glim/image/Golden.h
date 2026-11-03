#pragma once

// Shared golden-test helpers: one home for the strict-vs-local rule
// and the failure-artifact path, so the per-example golden mains stay
// three lines. Strict mode (GLIM_GOLDEN_STRICT=1, set in CI) turns a
// missing golden into a failure; local runs keep write-and-pass.
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

// Strict CPU-golden check: got vs want with the standard per-channel
// tolerance of 2. Mismatch (or unreadable golden) writes the GOT
// artifact when GLIM_GOT_DIR is set and returns false.
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
