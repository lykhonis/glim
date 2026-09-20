#pragma once

#include <glim/math.h>
#include <glim/paint/Context.h>
#include <glim/shell/Window.h>
#if !GLIM_SOFTWARE
#include <glim/gpu/Device.h>
#endif

struct ExampleFrame {
    glim::paint::Context& context;
    glim::shell::Window& window;
    glim::Vec2 size;
    glim::Rect safeArea;
    bool reduceTransparency = false;
    float time = 0.f;
#if !GLIM_SOFTWARE
    glim::gpu::Device& device;
#endif
};

void recordExample(ExampleFrame&);
