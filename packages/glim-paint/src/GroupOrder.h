#pragma once

#include <glim/assert.h>

namespace glim::paint {

struct GroupOrder {
  bool seenBackdrop = false;
  bool afterBackdrop = false;
  bool seenGlass = false;
  bool afterGlass = false;

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
