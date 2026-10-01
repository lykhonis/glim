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
    if (example >= 0 && example <= 4) {
        gExample = example;
    }
}

void recordSlot(ExampleFrame& frame) {
    recordHello(frame.context, frame.size, frame.time, frame.safeArea);
    const float w = 280.f;
    const float h = 40.f;
    const float x = (frame.size.x - w) * 0.5f;
    frame.context.slot(glim::Rect{{x, 20.f}, {w, h}}, 1);
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
        case 4:
            recordSlot(frame);
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
