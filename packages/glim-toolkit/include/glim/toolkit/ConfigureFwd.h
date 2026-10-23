#pragma once

#include <vector>

namespace glim {
namespace toolkit {

struct Block;

template <class H, class... As>
void configure(H& h, std::vector<Block>& children, As&&... as);

template <class H, class... As>
void configureTags(H& h, As&&... as);

}  // namespace toolkit
}  // namespace glim
