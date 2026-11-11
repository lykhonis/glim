#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace glimlint {

struct Finding {
  std::string file;
  int line;
  std::string message;
};

struct FactoryReport {
  int factories = 0;
  std::vector<Finding> findings;
};

// Props-form factory rule over one file's source:
//  A: a namespace-level Block/auto factory with 2+ fixed params must
//     take the element struct instead (allowlist: tap, list).
//  B: a factory taking As&&... must forward every trailing arg
//     (std::forward<As>(as)...) and wrap through finish().
FactoryReport checkFactories(std::string_view file, std::string_view src);

}  // namespace glimlint
