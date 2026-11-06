#pragma once

namespace glim {
namespace toolkit {

// Text measurement seam: embedder publishes a measurer into Engine env; fallback is fixed advance.
struct Measurer {
  float (*measure)(const char* text, float sizePx) = nullptr;
};

}  // namespace toolkit
}  // namespace glim
