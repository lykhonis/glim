#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "glim/toolkit/Toolkit.h"

using namespace glim::toolkit;

std::string calcDump();
std::string modalDump();
Block calcTree(State<std::string> out);
Block modalTree(Action onNo, Action onYes);

namespace {

bool fileExists(const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) {
    return false;
  }
  std::fclose(f);
  return true;
}

std::string readFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream os;
  os << in.rdbuf();
  return os.str();
}

int checkGolden(const std::string& dir, const std::string& name, const std::string& got) {
  std::string path = dir + "/" + name;
  if (!fileExists(path)) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
      std::cerr << "could not write golden " << path << '\n';
      return EXIT_FAILURE;
    }
    out << got;
    std::cout << "wrote golden " << path << '\n';
    return EXIT_SUCCESS;
  }
  if (got != readFile(path)) {
    std::cerr << "golden mismatch: dump differs from " << path << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : "examples/calc/golden";
  if (int rc = checkGolden(dir, "dump.txt", calcDump())) return rc;
  if (int rc = checkGolden(dir, "modal.txt", modalDump())) return rc;

  // Calc dispatch + focus end to end on real buttons (flat rows, direct leaves).
  Store store;
  State<std::string> out = store.make<std::string>("12+3");
  Block tree = calcTree(out);
  Box box = place(tree, {0, 720, 0, 480});
  auto focusableList = focusables(tree, box);
  assert(focusableList.size() == 12);
  // First focusable is "7"; firing it appends through the cell.
  Box first = focusBox(tree, box, 0);
  const Block* hitFirst = hit(tree, box, first.x + 1, first.y + 1);
  assert(hitFirst && fire(*hitFirst));
  assert(*out == "12+37");
  // Spatial moves walk the digit grid.
  assert(moveFocus(tree, box, 0, FocusDir::Right) == 1);
  assert(moveFocus(tree, box, 1, FocusDir::Down) == 5);

  // Modal overlay: dim fills the box, card centers via fills, paint under→over.
  // Buttons composed inside the Card stay reachable through eachChild.
  State<int> answer = store.make<int>(0);
  Block modal = modalTree([answer] { answer = 1; }, [answer] { answer = 2; });
  Box mbox = place(modal, {0, 720, 0, 480});
  const Stack* st2 = modal.get<Stack>();
  assert(st2 && st2->children.size() == 2);
  auto modalFocus = focusables(modal, mbox);
  assert(modalFocus.size() == 2);
  // Hit the Yes button through the Card and fire it.
  Box yesBox = focusBox(modal, mbox, 1);
  const Block* yesHit = hit(modal, mbox, yesBox.x + 1, yesBox.y + 1);
  assert(yesHit && yesHit->get<Custom>());
  assert(fire(*yesHit));
  assert(*answer == 2);
  // Focus walks from No to Yes.
  assert(moveFocus(modal, mbox, 0, FocusDir::Right) == 1);
  // Dim corner hits the dim, which does not fire.
  const Block* dimHit = hit(modal, mbox, 10, 10);
  assert(dimHit && !fire(*dimHit));
  // Card padding itself resolves to the Card, which does not fire.
  {
    Rec recr;
    record(modal, mbox, recr);
    Box cardBox{0, 0, 0, 0};
    bool foundCard = false;
    for (const auto& e : recr.entries)
      if (e.kind == "Card") {
        cardBox = e.box;
        foundCard = true;
      }
    assert(foundCard);
    const Block* padHit = hit(modal, mbox, cardBox.x + 1, cardBox.y + 1);
    assert(padHit && padHit->get<Custom>() && !fire(*padHit));
  }

  std::cout << "dump_test ok\n";
  return EXIT_SUCCESS;
}
