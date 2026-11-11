#include <glim/paint/Context.h>

#include "run_web.h"

void recordGlass(
    glim::paint::Context& context,
    glim::Vec2 size,
    float timeSeconds,
    glim::Rect safeArea,
    bool reduceTransparency
);

extern "C" void glimWebSelect(int) {}

void recordExample(ExampleFrame& frame) {
  recordGlass(
      frame.context,
      frame.size,
      frame.time,
      frame.safeArea,
      frame.reduceTransparency
  );
}
