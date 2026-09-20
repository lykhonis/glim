#include "run_web.h"

#include <memory>

#include <glim/paint/Overlay.h>
#include <glim/paint/Renderer.h>
#include <glim/shell/Application.h>
#include <glim/shell/RunLoop.h>
#include <glim/shell/Surface.h>

namespace {

glim::shell::Window* gWindow = nullptr;
glim::paint::Context* gContext = nullptr;
std::unique_ptr<glim::gpu::Device> gDevice;
std::unique_ptr<glim::paint::Renderer> gRenderer;
bool gReduceTransparency = false;
float gTime = 0.f;
float gLastTime = 0.f;
int gZeroStreak = 0;
glim::paint::Overlay gOverlay;

bool ensureDevice() {
    if (gDevice && gRenderer) {
        return true;
    }
    if (gWindow == nullptr || gContext == nullptr) {
        return false;
    }
    glim::shell::Surface surface;
    surface.attach(*gWindow);
    surface.setVSync(true);
    auto created = glim::gpu::Device::create(surface.deviceCreateInfo());
    if (!created.ok()) {
        return false;
    }
    gDevice = std::make_unique<glim::gpu::Device>(std::move(created.value()));
    gRenderer = std::make_unique<glim::paint::Renderer>(*gDevice);
    return true;
}

void paintOnce() {
    if (gWindow == nullptr || gContext == nullptr || !ensureDevice()) {
        return;
    }
    const glim::Vec2 size = gWindow->size();
    gContext->setSize(size);
    gContext->beginFrame();
    ExampleFrame frame{*gContext, *gWindow, size, gWindow->safeArea(), gReduceTransparency,
                       gTime, *gDevice};
    recordExample(frame);
    if (gOverlay.enabled()) {
        gOverlay.tick(gTime - gLastTime);
        gOverlay.setStats(gRenderer->stats());
        gOverlay.record(*gContext, gWindow->safeArea());
    }
    gLastTime = gTime;
    gContext->finish();
    const glim::Vec2 drawable = gWindow->drawableSize();
    gDevice->setDrawableSize(static_cast<int>(drawable.x), static_cast<int>(drawable.y));
    gRenderer->draw(gContext->scene());
    if (gRenderer->stats().draws == 0) {
        if (++gZeroStreak > 10) {
            gZeroStreak = 0;
            gRenderer.reset();
            gDevice.reset();
        }
    } else {
        gZeroStreak = 0;
    }
}

}  // namespace

extern "C" void glimWebResize(int w, int h, float pixelRatio) {
    if (gWindow != nullptr) {
        gWindow->setSize(w, h);
        gWindow->setPixelRatio(pixelRatio);
    }
}

extern "C" void glimWebFrame(double timeSeconds, int reduceTransparency) {
    gTime = static_cast<float>(timeSeconds);
    gReduceTransparency = reduceTransparency != 0;
    paintOnce();
}

extern "C" void glimWebEvent(int type, float x, float y, int key) {
    (void)x;
    (void)y;
    // Matches GlimCanvas event codes: 1 = pointerdown, 4 = keydown (13 = Enter).
    if (type == 1 || (type == 4 && key == 13)) {
        gOverlay.toggleExpanded();
    }
}

int main() {
    static glim::shell::Application host;
    static glim::shell::Window window;
    window.setTitle("Glim Web");
    window.setSize(720, 480);
    window.setCanvasSelector("#glim");
    window.show();
    static glim::paint::Context context;
    gWindow = &window;
    gContext = &context;
    gOverlay.setEnabled(true);
    host.run();
    paintOnce();
    return 0;
}
