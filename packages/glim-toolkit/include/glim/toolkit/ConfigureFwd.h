#pragma once

#include <vector>

namespace glim {
namespace toolkit {

struct Block;

template <class H, class... As>
void configure(H& h, std::vector<Block>& children, As&&... as);

template <class H, class... As>
void configureTags(H& h, As&&... as);

template <class H, class... As>
Block finish(H&& h, As&&... as);

namespace detail {

template <class C, class F>
void forEachBlock(const C& c, F&& f) {
  for (const auto& b : c) f(b);
}

}  // namespace detail

}  // namespace toolkit
}  // namespace glim
