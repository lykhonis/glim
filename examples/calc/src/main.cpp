#include <iostream>
#include <sstream>
#include <string>

#include "glim/toolkit/Toolkit.h"

#include "elements/Button.h"
#include "elements/Card.h"

using namespace glim::toolkit;

Block calcTree(State<std::string> out) {
  auto o = out;
  Block display = roll(horizontal(), scrollOffset(State<float>{}), lines(*o));
  auto digit = [&](const char* label, char d) -> Block {
    return button(label, [o, d] { o = *o + d; });
  };
  auto op = [&](const char* label) -> Block {
    return button(label, [o, label] { o = *o + label; });
  };
  return column(gap(8), display, row(gap(8), fill(digit("7", '7')), fill(digit("8", '8')),
                                      fill(digit("9", '9')), fill(op("/"))),
                row(gap(8), fill(digit("4", '4')), fill(digit("5", '5')), fill(digit("6", '6')),
                    fill(op("*"))),
                row(gap(8), fill(digit("1", '1')), fill(digit("2", '2')), fill(digit("3", '3')),
                    fill(button("=", [o] { o = "="; }, primary()))));
}

std::string dumpTree(Block tree, Constraints c) {
  std::ostringstream os;
  Box box = place(tree, c);
  os << "root box: " << box.w << "x" << box.h << "\n";
  dump(os, tree);
  Rec rec;
  record(tree, box, rec);
  for (const auto& e : rec.entries) {
    os << e.kind << " [" << e.box.x << "," << e.box.y << " " << e.box.w << "x" << e.box.h
       << "] " << e.text << "\n";
  }
  return os.str();
}

std::string calcDump() {
  Store store;
  return dumpTree(calcTree(store.make<std::string>("12+3")), {0, 720, 0, 480});
}

Block modalTree(Action onNo, Action onYes) {
  Block dialog = card(radius(6), gap(8), lines("Delete?"),
                      row(gap(8), button("No", std::move(onNo)), button("Yes", std::move(onYes))));
  return stack(fill(Block{}),
               column(gap(8), fill(Block{}),
                      row(gap(0), fill(Block{}), std::move(dialog), fill(Block{})), fill(Block{})));
}

std::string modalDump() { return dumpTree(modalTree([] {}, [] {}), {0, 720, 0, 480}); }

std::string listDump() {
  Block tree = roll(vertical(), list(8, [](int i) { return lines("item" + std::to_string(i)); }));
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
    os << e.kind << " [" << e.box.x << "," << e.box.y << " " << e.box.w << "x" << e.box.h
       << "] " << e.text << "\n";
  }
  return os.str();
}

#ifndef GLIM_GOLDEN
int main() {
  std::cout << calcDump();
  return 0;
}
#endif
