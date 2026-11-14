#pragma once

// Golden helpers: strict mode (GLIM_GOLDEN_STRICT=1, CI) fails on missing
// golden; local runs write-and-pass; update mode (GLIM_GOLDEN_UPDATE=1,
// regen-goldens) overwrites. GOT artifact goes to GLIM_GOT_DIR.
#include <glim/image/Image.h>
#include <glim/image/Png.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace glim::image {

inline bool envFlag(const char* name) {
  const char* v = std::getenv(name);
  return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

inline bool goldenStrict() { return envFlag("GLIM_GOLDEN_STRICT"); }

inline bool goldenUpdate() { return envFlag("GLIM_GOLDEN_UPDATE"); }

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

// Compares `got` against the PNG at goldenPath. A missing golden fails in
// strict mode and is written (passing) otherwise; update mode rewrites it.
inline bool matchCpuGolden(
  const Image& got, const char* goldenPath, const char* testName, const char* what
) {
  std::FILE* f = goldenUpdate() ? nullptr : std::fopen(goldenPath, "rb");
  if (f) {
    std::fclose(f);
  } else {
    if (goldenStrict() && !goldenUpdate()) {
      std::cerr << "missing golden " << goldenPath << " (strict mode)\n";
      return false;
    }
    if (!Png::write(goldenPath, got)) {
      std::cerr << "could not write golden " << goldenPath << '\n';
      return false;
    }
    std::cout << "wrote golden " << goldenPath << '\n';
    return true;
  }
  Image want;
  if (!Png::read(goldenPath, &want)) {
    std::cerr << "could not read golden " << goldenPath << '\n';
    return false;
  }
  return checkCpuGolden(got, want, testName, what);
}

}  // namespace glim::image
