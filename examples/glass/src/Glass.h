#pragma once

#include <glim/math.h>
#include <glim/paint/Context.h>

void recordGlass(
    glim::paint::Context& context,
    glim::Vec2 size,
    float timeSeconds,
    glim::Rect safeArea = {},
    bool reduceTransparency = false
);
