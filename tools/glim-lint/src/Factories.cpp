#include "Factories.h"

#include <algorithm>
#include <array>

#include "Tokenizer.h"

namespace glimlint {
namespace {

using Tokens = std::vector<Token>;

constexpr std::array<std::string_view, 2> kMultiFixedAllowed{"tap", "list"};

bool is(const Token& t, std::string_view text) { return t.text == text; }

// Index of the token closing the bracket opened at `open`, or size.
std::size_t closeOf(const Tokens& t, std::size_t open, std::string_view o, std::string_view c) {
  int depth = 0;
  for (std::size_t i = open; i < t.size(); ++i) {
    if (is(t[i], o)) ++depth;
    if (is(t[i], c) && --depth == 0) return i;
  }
  return t.size();
}

// Comma-separated parameters at the top bracket level.
std::vector<Tokens> splitParams(const Tokens& t, std::size_t b, std::size_t e) {
  std::vector<Tokens> params(1);
  int depth = 0;
  for (std::size_t i = b; i < e; ++i) {
    const std::string& x = t[i].text;
    if (x == "(" || x == "<" || x == "[" || x == "{") ++depth;
    if (x == ")" || x == ">" || x == "]" || x == "}") --depth;
    if (x == "," && depth == 0) {
      params.emplace_back();
      continue;
    }
    params.back().push_back(t[i]);
  }
  if (params.back().empty()) params.pop_back();
  return params;
}

bool hasPack(const Tokens& p) {
  return std::any_of(p.begin(), p.end(), [](const Token& x) { return is(x, "."); });
}

bool hasForwardAll(const Tokens& t, std::size_t b, std::size_t e) {
  constexpr std::array<std::string_view, 12> seq{
      "std", "::", "forward", "<", "As", ">", "(", "as", ")", ".", ".", "."
  };
  for (std::size_t i = b; i + seq.size() <= e; ++i) {
    bool hit = true;
    for (std::size_t k = 0; k < seq.size() && hit; ++k) hit = is(t[i + k], seq[k]);
    if (hit) return true;
  }
  return false;
}

bool callsFinish(const Tokens& t, std::size_t b, std::size_t e) {
  for (std::size_t i = b; i + 1 < e; ++i)
    if (is(t[i], "finish") && is(t[i + 1], "(")) return true;
  return false;
}

// A declaration can only start after one of these at namespace level.
bool startsDeclaration(const Tokens& t, std::size_t i) {
  if (i == 0) return true;
  const std::string& p = t[i - 1].text;
  return p == ";" || p == "}" || p == "{" || p == "inline" || p == ">";
}

}  // namespace

FactoryReport checkFactories(std::string_view file, std::string_view src) {
  FactoryReport report;
  const Tokens t = tokenize(src);
  const std::string path(file);

  // Brace stack: true marks a namespace body. Factories only live in
  // namespace scope, never inside a struct or function body.
  std::vector<bool> braces;
  bool sawNamespace = false;
  int nonNamespaceDepth = 0;

  for (std::size_t i = 0; i < t.size(); ++i) {
    if (is(t[i], "namespace")) sawNamespace = true;
    if (is(t[i], ";")) sawNamespace = false;
    if (is(t[i], "{")) {
      braces.push_back(sawNamespace);
      if (!sawNamespace) ++nonNamespaceDepth;
      sawNamespace = false;
      continue;
    }
    if (is(t[i], "}")) {
      if (!braces.empty()) {
        if (!braces.back()) --nonNamespaceDepth;
        braces.pop_back();
      }
      continue;
    }
    if (nonNamespaceDepth > 0) continue;
    if (!(is(t[i], "Block") || is(t[i], "auto"))) continue;
    if (i + 2 >= t.size() || t[i + 1].kind != Kind::Ident || !is(t[i + 2], "(")) continue;
    if (!startsDeclaration(t, i)) continue;

    const Token& name = t[i + 1];
    ++report.factories;
    const std::size_t close = closeOf(t, i + 2, "(", ")");
    const auto params = splitParams(t, i + 3, close);
    std::size_t fixed = 0;
    bool forwards = false;
    for (const Tokens& p : params) {
      if (hasPack(p))
        forwards = true;
      else
        ++fixed;
    }

    const bool allowed =
        std::find(kMultiFixedAllowed.begin(), kMultiFixedAllowed.end(), name.text) !=
        kMultiFixedAllowed.end();
    if (fixed >= 2 && !allowed)
      report.findings.push_back(
          {path,
           name.line,
           name.text + " takes " + std::to_string(fixed) +
               " fixed params - pass the element struct instead"}
      );

    if (!forwards) continue;
    std::size_t j = close + 1;
    while (j < t.size() && !is(t[j], "{") && !is(t[j], ";")) ++j;
    if (j >= t.size() || is(t[j], ";")) continue;  // declaration only
    const std::size_t end = closeOf(t, j, "{", "}");
    if (!hasForwardAll(t, j, end))
      report.findings.push_back({path, name.line, name.text + " takes As&&... without forwarding"});
    if (!callsFinish(t, j, end))
      report.findings.push_back({path, name.line, name.text + " takes As&&... without finish()"});
  }
  return report;
}

}  // namespace glimlint
