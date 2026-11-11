#pragma once

#include <glim/assert.h>

namespace glim::paint {

// Sibling Groups composite as [under*][backdrop*][glass*][overlay*], except
// that glass may follow overlays again: each such glass run samples what is
// drawn beneath it, overlays included. Every renderer walks children through
// one of these.
struct GroupOrder {
  bool seenBackdrop = false;
  bool afterBackdrop = false;
  bool seenGlass = false;
  bool afterGlass = false;

  // True when `glass` starts a new glass run after overlays; renderers that
  // freeze the glass source once must take it again.
  bool next(bool backdrop, bool glass) {
    if (backdrop) {
      GLIM_ASSERT(
          !afterBackdrop && !seenGlass,
          "backdrop Groups must be consecutive ([under*][backdrop*][glass*][overlay*])"
      );
      seenBackdrop = true;
      return false;
    }
    if (glass) {
      const bool newRun = afterGlass;
      afterGlass = false;
      if (seenBackdrop) {
        afterBackdrop = true;
      }
      seenGlass = true;
      return newRun;
    }
    if (seenGlass) {
      afterGlass = true;
    } else if (seenBackdrop) {
      afterBackdrop = true;
    }
    return false;
  }
};

}  // namespace glim::paint
