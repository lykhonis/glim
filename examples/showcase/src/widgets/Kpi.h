#pragma once

// KPI card: caption + big value + delta line. Value uses the theme
// value size (~28); caption/delta stay on body type. Equal thirds come
// from expand() in the parent row. Read-only, never focusable.

#include <string>
#include <utility>

#include "Card.h"
#include "Type.h"
#include "glim/toolkit.h"

namespace glim {
namespace toolkit {

struct Kpi {
  std::string title;
  std::string value;
  std::string delta;
};

template <class... As>
Block kpiCard(Kpi k, As&&... as) {
  Column c;
  c.gap = 4;
  c.children.push_back(lines(std::move(k.title)));
  c.children.push_back(valueLines(std::move(k.value)));
  c.children.push_back(lines(std::move(k.delta)));
  configure(c, c.children, std::forward<As>(as)...);
  return finish(std::move(c), std::forward<As>(as)...);
}

}  // namespace toolkit
}  // namespace glim
