#include "run_web.h"

#include <memory>
#include <algorithm>
#include <vector>

#include <glim/paint/Overlay.h>
#include <glim/paint/Renderer.h>
#include <glim/paint/Scene.h>
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
std::vector<glim::paint::SlotPlacement> gSlots;

void syncSlots() {
    if (gWindow == nullptr || gContext == nullptr) {
        return;
    }
    std::vector<glim::paint::SlotPlacement> places;
    glim::paint::collectSlots(gContext->scene(), &places);
    std::sort(places.begin(), places.end(),
              [](const auto& a, const auto& b) { return a.id < b.id; });
    std::vector<std::uint32_t> live;
    gWindow->forEachSlot([&](std::uint32_t id, glim::Rect) { live.push_back(id); });
    for (std::uint32_t id : live) {
        bool kept = false;
        for (const auto& p : places) {
            if (p.id == id) {
                kept = true;
                break;
            }
        }
        if (!kept) {
            gWindow->detachSlot(id);
        }
    }
    for (const auto& p : places) {
        gWindow->positionSlot(p.id, p.windowLogical);
    }
    gSlots = std::move(places);
}

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
    syncSlots();
    if (gRenderer->stats().draws == 0 && gRenderer->stats().reuseHits == 0) {
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

extern "C" void glimWebOverlay(int enabled) {
    gOverlay.setEnabled(enabled != 0);
}

extern "C" int glimWebStats() {
    if (!gRenderer) {
        return -1;
    }
    return gRenderer->stats().draws;
}

extern "C" int glimWebSlotCount() {
    return static_cast<int>(gSlots.size());
}

extern "C" unsigned glimWebSlotId(int i) {
    if (i < 0 || static_cast<std::size_t>(i) >= gSlots.size()) {
        return 0;
    }
    return gSlots[static_cast<std::size_t>(i)].id;
}

extern "C" float glimWebSlotX(int i) {
    if (i < 0 || static_cast<std::size_t>(i) >= gSlots.size()) {
        return 0.f;
    }
    return gSlots[static_cast<std::size_t>(i)].windowLogical.origin.x;
}

extern "C" float glimWebSlotY(int i) {
    if (i < 0 || static_cast<std::size_t>(i) >= gSlots.size()) {
        return 0.f;
    }
    return gSlots[static_cast<std::size_t>(i)].windowLogical.origin.y;
}

extern "C" float glimWebSlotW(int i) {
    if (i < 0 || static_cast<std::size_t>(i) >= gSlots.size()) {
        return 0.f;
    }
    return gSlots[static_cast<std::size_t>(i)].windowLogical.size.x;
}

extern "C" float glimWebSlotH(int i) {
    if (i < 0 || static_cast<std::size_t>(i) >= gSlots.size()) {
        return 0.f;
    }
    return gSlots[static_cast<std::size_t>(i)].windowLogical.size.y;
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
