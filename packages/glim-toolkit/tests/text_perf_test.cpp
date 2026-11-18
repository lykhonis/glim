// N8 keystroke cost: structural budgets asserted, timings reported, with a
// loose wall-clock ceiling that only a regression of kind (not noise) trips.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "glim/toolkit.h"

using namespace glim::toolkit;

namespace {

struct Feed {
  std::uint32_t id = 0;
  bool foreign = false;
};

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

double p(std::vector<double> v, double q) {
  std::sort(v.begin(), v.end());
  return v[static_cast<std::size_t>(q * static_cast<double>(v.size() - 1))];
}

}  // namespace

int main() {
  using clk = std::chrono::steady_clock;
  Store store;
  Engine e;
  e.store = store;
  // A busy screen: 200 rows of labels and buttons around one field.
  State<TextEdit> value = rootScope(e).state("note", TextEdit{});
  const auto rowAt = [](int i) {
    return row(gap(8), lines("Row " + std::to_string(i)), tap(lines("Go"), [] {}));
  };
  const auto build = [&] {
    return column(textField({.value = value, .width = 600}), roll(vertical(), list(200, rowAt)));
  };
  Block tree = build();
  const Box box = e.place(tree, {0, 960, 0, 640});
  Screen screen;
  const Box field = focusBox(tree, box, 0, e);
  screen.pointerDown(tree, box, field.x + 4, field.y + 4, 1, e);

  // One keystroke as host::run handles it: rebuild, route, republish.
  constexpr int kKeys = 400;
  // Routing and republishing are text input's own cost; the rebuilds are
  // the app's and are reported apart.
  std::vector<double> keyUs, buildUs;
  for (int i = 0; i < kKeys; ++i) {
    const auto b0 = clk::now();
    tree = build();
    buildUs.push_back(std::chrono::duration<double, std::micro>(clk::now() - b0).count());
    auto t0 = clk::now();
    const bool routed = screen.text(tree, box, {.op = TextOp::Insert, .text = "a"}, e);
    double us = std::chrono::duration<double, std::micro>(clk::now() - t0).count();
    tree = build();
    t0 = clk::now();
    const auto focus = screen.textFocus(tree, box, e);
    keyUs.push_back(us + std::chrono::duration<double, std::micro>(clk::now() - t0).count());
    if (!routed || !focus) {
      expect(false, "every key reaches the focused field");
      break;
    }
  }
  expect(value->text.size() == kKeys, "no key lost or doubled");

  // Composition churn on long text stays linear in the edit, not the history.
  std::vector<double> markUs;
  for (int i = 0; i < kKeys; ++i) {
    const auto t0 = clk::now();
    TextEdit edit = value.get();
    edit = markText(edit, "ni", {2, 0});
    edit = insertText(edit, "你");
    markUs.push_back(std::chrono::duration<double, std::micro>(clk::now() - t0).count());
    value = edit;
  }

  // Paint: the field adds one text op, and a fill only while composing.
  {
    tree = build();
    OpsRecorder rec;
    Feed feed;
    paintTree(tree, box, e, rec, feed);
    OpsRecorder caret;
    paintCaret(tree, box, screen.focus, caret, e);
    int fieldTexts = 0;
    for (const auto& o : rec.ops) fieldTexts += o.kind == Op::Kind::Text && o.text == value->text;
    expect(fieldTexts == 1, "one text op for the field");
    expect(caret.ops.size() == 1, "one caret op");
  }

  // The yardstick is one plain focus walk over the same screen, which every
  // key and tap already pays. CI runs tests in parallel on shared runners,
  // so bounds compare minimums of interleaved samples: the uncontended cost
  // of each, measured under the same conditions. Percentiles are reported.
  const auto timed = [&](auto&& fn) {
    const auto t0 = clk::now();
    fn();
    return std::chrono::duration<double, std::micro>(clk::now() - t0).count();
  };
  std::vector<double> walkUs;
  for (int i = 0; i < 100; ++i) {
    walkUs.push_back(timed([&] { expect(!focusables(tree, box, e).empty(), "focusables"); }));
  }
  // Walks are linear in the tree: twice the rows, about twice the time.
  const auto screenOf = [&](int rows) {
    Block t = column(roll(vertical(), list(rows, rowAt)));
    const Box b = e.place(t, {0, 960, 0, 640});
    return std::pair{std::move(t), b};
  };
  const auto [small, smallBox] = screenOf(400);
  const auto [large, largeBox] = screenOf(800);
  // Each pair runs back to back under the same load, so its ratio cancels
  // contention; the median ignores pairs split by a preemption either way,
  // where a ratio of two minimums hinges on one lucky sample.
  std::vector<double> ratios;
  for (int i = 0; i < 80; ++i) {
    const double s = timed([&] { focusables(small, smallBox, e); });
    ratios.push_back(timed([&] { focusables(large, largeBox, e); }) / s);
  }
  const double walk = p(walkUs, 0.0);
  const double key = p(keyUs, 0.0);
  const double growth = p(ratios, 0.5);
  std::cout << "text perf: keystroke min " << key << " us p50 " << p(keyUs, 0.5) << " us p95 "
            << p(keyUs, 0.95) << " us (" << kKeys << " keys, 200-row screen; app rebuild p50 "
            << p(buildUs, 0.5) << " us, focus walk min " << walk << " us, x2 rows costs x" << growth
            << "); compose+commit min " << p(markUs, 0.0) << " us p95 " << p(markUs, 0.95)
            << " us at " << value->text.size() << " bytes\n";
  // Route plus republish is two walks; 3 leaves room for noise, while a
  // quadratic walk (x4) or an extra pass per key does not fit.
  expect(growth < 3.0, "focus walk grows linearly with the tree");
  expect(key < 3.0 * walk, "a keystroke costs at most 3 focus walks");
  expect(p(markUs, 0.0) < 0.01 * walk, "compose+commit is negligible beside a walk");
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
