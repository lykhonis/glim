#pragma once

#include <glim/math.h>
#include <glim/paint/Context.h>

// `size` is the full window. `safeArea` is the unobscured rect (notch, home
// indicator). Empty safeArea means the full window (golden fixture).
void recordHello(glim::paint::Context& context, glim::Vec2 size, float timeSeconds,
                 glim::Rect safeArea = {});
