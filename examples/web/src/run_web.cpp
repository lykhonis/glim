#include "run_web.h"

#include <glim/paint/Renderer.h>
#include <glim/shell/Application.h>
#include <glim/shell/RunLoop.h>
#include <glim/shell/Surface.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace {

glim::shell::Window* gWindow = nullptr;
glim::paint::Context* gContext = nullptr;
glim::paint::Renderer* gRenderer = nullptr;
glim::gpu::Device* gDevice = nullptr;
bool gReduceTransparency = false;
float gTime = 0.f;

void paintOnce() {
    if (gWindow == nullptr || gContext == nullptr || gRenderer == nullptr || gDevice == nullptr) {
        return;
    }
    const glim::Vec2 size = gWindow->size();
    gContext->setSize(size);
    gContext->beginFrame();
    ExampleFrame frame{*gContext, *gWindow, size, gWindow->safeArea(), gReduceTransparency,
                       gTime, *gDevice};
    recordExample(frame);
    gContext->finish();
    const glim::Vec2 drawable = gWindow->drawableSize();
    gDevice->setDrawableSize(static_cast<int>(drawable.x), static_cast<int>(drawable.y));
    gRenderer->draw(gContext->scene());
}

#if defined(__EMSCRIPTEN__)
void rafThunk(void*) {
    paintOnce();
}
#endif

}  // namespace

extern "C" void glimWebResize(int w, int h, float pixelRatio) {
    (void)pixelRatio;
    if (gWindow != nullptr) {
        gWindow->setSize(w, h);
    }
}

extern "C" void glimWebFrame(double timeSeconds, int reduceTransparency) {
    gTime = static_cast<float>(timeSeconds);
    gReduceTransparency = reduceTransparency != 0;
    paintOnce();
}

extern "C" void glimWebEvent(int type, float x, float y, int key) {
    (void)type;
    (void)x;
    (void)y;
    (void)key;
}

int main() {
    static glim::shell::Application host;
    static glim::shell::Window window;
    static glim::shell::Surface surface;
    window.setTitle("Glim Web");
    window.setSize(720, 480);
    window.setCanvasSelector("#glim");
    surface.attach(window);
    surface.setVSync(true);
    auto created = glim::gpu::Device::create(surface.deviceCreateInfo());
    if (!created.ok()) {
        return 1;
    }
    static glim::gpu::Device device = std::move(created.value());
    static glim::paint::Context context;
    static glim::paint::Renderer renderer(device);
    gWindow = &window;
    gContext = &context;
    gRenderer = &renderer;
    gDevice = &device;
    window.show();
#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop_arg(rafThunk, nullptr, 0, 1);
#else
    host.run();
    paintOnce();
#endif
    return 0;
}
