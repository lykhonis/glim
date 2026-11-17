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

  // The yardstick: one plain focus walk over the same screen, which every
  // key and tap already pays. Relative budgets hold on any runner.
  std::vector<double> walkUs;
  for (int i = 0; i < 100; ++i) {
    const auto t0 = clk::now();
    expect(!focusables(tree, box, e).empty(), "focusables found");
    walkUs.push_back(std::chrono::duration<double, std::micro>(clk::now() - t0).count());
  }
  const double walk = p(walkUs, 0.5);
  // Walks are linear in the tree: twice the rows, about twice the time.
  const auto walkOf = [&](int rows) {
    Block t = column(roll(vertical(), list(rows, rowAt)));
    const Box b = e.place(t, {0, 960, 0, 640});
    std::vector<double> us;
    for (int i = 0; i < 40; ++i) {
      const auto t0 = clk::now();
      expect(!focusables(t, b, e).empty(), "rows are focusable");
      us.push_back(std::chrono::duration<double, std::micro>(clk::now() - t0).count());
    }
    return p(us, 0.5);
  };
  const double growth = walkOf(800) / walkOf(400);
  const double keyP50 = p(keyUs, 0.5), keyP95 = p(keyUs, 0.95);
  std::cout << "text perf: keystroke p50 " << keyP50 << " us p95 " << keyP95 << " us (" << kKeys
            << " keys, 200-row screen; app rebuild p50 " << p(buildUs, 0.5)
            << " us, focus walk p50 " << walk << " us, x2 rows costs x" << growth
            << "); compose+commit p50 " << p(markUs, 0.5) << " us p95 " << p(markUs, 0.95)
            << " us at " << value->text.size() << " bytes\n";
  // Route plus republish is two walks; 3 allows noise, a quadratic walk or
  // an extra pass per key does not fit. A key must also fit in a frame.
  expect(growth < 3.0, "focus walk grows linearly with the tree");
  expect(keyP50 < 3.0 * walk, "keystroke p50 within 3 focus walks");
  expect(keyP95 < 16700.0, "keystroke p95 within one 60 Hz frame");
  expect(p(markUs, 0.95) < 200.0, "compose+commit p95 under 200 us");
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
