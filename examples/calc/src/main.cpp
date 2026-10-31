#include <iostream>
#include <sstream>
#include <string>

#include "elements/Button.h"
#include "elements/Card.h"
#include "glim/toolkit/Toolkit.h"

using namespace glim::toolkit;

Block calcTree(State<std::string> out) {
  auto o = out;
  Block display =
      roll(horizontal(), scrollOffset(State<float>{}), lines(*o));
  auto digit = [&](const char* label, char d) -> Block {
    return button(label, [o, d] { o = *o + d; }, expand());
  };
  auto op = [&](const char* label) -> Block {
    return button(label, [o, label] { o = *o + label; }, expand());
  };
  return column(
      gap(8),
      display,
      row(gap(8),
          digit("7", '7'),
          digit("8", '8'),
          digit("9", '9'),
          op("/")),
      row(gap(8),
          digit("4", '4'),
          digit("5", '5'),
          digit("6", '6'),
          op("*")),
      row(gap(8),
          digit("1", '1'),
          digit("2", '2'),
          digit("3", '3'),
          button("=", [o] { o = "="; }, primary(), expand()))
  );
}

std::string dumpTree(Block tree, Constraints c) {
  std::ostringstream os;
  Box box = place(tree, c);
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree);
  Rec rec;
  record(tree, box, rec);
  for (const auto& e : rec.entries) {
    os << e.kind << " [" << e.box.x << "," << e.box.y << " " << e.box.w
       << "x" << e.box.h << "] " << e.text << "\n";
  }
  return os.str();
}

std::string calcDump() {
  Store store;
  return dumpTree(
      calcTree(store.make<std::string>("12+3")), {0, 720, 0, 480}
  );
}

Block modalTree(Action onNo, Action onYes) {
  Block dialog = card(
      radius(6),
      gap(8),
      lines("Delete?"),
      row(gap(8),
          button("No", std::move(onNo)),
          button("Yes", std::move(onYes)))
  );
  return stack(
      empty(expand()),
      column(
          gap(8),
          empty(expand()),
          row(gap(0),
              empty(expand()),
              std::move(dialog),
              empty(expand())),
          empty(expand())
      )
  );
}

std::string modalDump() {
  return dumpTree(modalTree([] {}, [] {}), {0, 720, 0, 480});
}

std::string listDump() {
  Block tree = roll(vertical(), list(8, [](int i) {
                      return lines("item" + std::to_string(i));
                    }));
  return dumpTree(std::move(tree), {0, 720, 0, 100});
}

std::string focusDump() {
  Store store;
  Block tree = calcTree(store.make<std::string>("12+3"));
  Constraints c{0, 720, 0, 480};
  Box box = place(tree, c);
  std::ostringstream os;
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree);
  Rec rec;
  record(tree, box, rec);
  recordFocusRing(rec, tree, box, 0);
  for (const auto& e : rec.entries) {
    os << e.kind << " [" << e.box.x << "," << e.box.y << " " << e.box.w
       << "x" << e.box.h << "] " << e.text << "\n";
  }
  return os.str();
}

#ifndef GLIM_GOLDEN
int main() {
  std::cout << calcDump();
  return 0;
}
#endif
