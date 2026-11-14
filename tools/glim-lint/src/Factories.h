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
//  A: a namespace-level Block/auto/*Tag factory with 2+ fixed params
//     must take the props struct instead (allowlist: tap, list, pad,
//     translate).
//  B: a factory taking As&&... must forward every trailing arg
//     (std::forward<As>(as)...) and wrap through finish().
FactoryReport checkFactories(std::string_view file, std::string_view src);

// Composition rule: UI code builds tags through their factory, never
// as a `FooTag{...}` literal or a default `FooTag x;` filled by hand.
std::vector<Finding> checkTagLiterals(std::string_view file, std::string_view src);

// Layout rule: a designated initializer `{.a = ...}` spanning lines
// ends in a trailing comma so clang-format lays it out as a block.
std::vector<Finding> checkTrailingCommas(std::string_view file, std::string_view src);

}  // namespace glimlint
