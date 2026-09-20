#include "run_web.h"

#include <glim/paint/Context.h>

void recordHello(glim::paint::Context& context, glim::Vec2 size, float timeSeconds,
                 glim::Rect safeArea);
void recordGlass(glim::paint::Context& context, glim::Vec2 size, float timeSeconds,
                 glim::Rect safeArea, bool reduceTransparency);
void recordGradients(glim::paint::Context& context, glim::Vec2 size, float timeSeconds,
                     glim::Rect safeArea);
void recordForeignExample(ExampleFrame& frame);

namespace {

int gExample = 0;

}  // namespace

void glimWebSelectExample(int example) {
    if (example >= 0 && example <= 3) {
        gExample = example;
    }
}

void recordExample(ExampleFrame& frame) {
    switch (gExample) {
        case 1:
            recordGlass(frame.context, frame.size, frame.time, frame.safeArea,
                        frame.reduceTransparency);
            break;
        case 2:
            recordGradients(frame.context, frame.size, frame.time, frame.safeArea);
            break;
        case 3:
            recordForeignExample(frame);
            break;
        case 0:
        default:
            recordHello(frame.context, frame.size, frame.time, frame.safeArea);
            break;
    }
}

extern "C" void glimWebSelect(int example) {
    glimWebSelectExample(example);
}
