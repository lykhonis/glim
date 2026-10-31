#pragma once

#include <limits>
#include <string>
#include <vector>

namespace glim {
namespace toolkit {

constexpr float kUnbounded = std::numeric_limits<float>::infinity();

inline bool isUnbounded(float v) { return v >= kUnbounded / 2; }

struct Constraints {
  float minW = 0, maxW = 0, minH = 0, maxH = 0;
};

struct Box {
  float x = 0, y = 0, w = 0, h = 0;
};

struct Rec {
  struct Entry {
    std::string kind;
    Box box;
    std::string text;
    std::string mark;
    int radius = 0;
  };
  std::vector<Entry> entries;
  void push(
      std::string kind, Box box, std::string text = {}, std::string mark = {}, int radius = 0
  ) {
    entries.push_back({std::move(kind), box, std::move(text), std::move(mark), radius});
  }
};

}  // namespace toolkit
}  // namespace glim
