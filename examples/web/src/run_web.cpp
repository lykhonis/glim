#include "Glass.h"
#include "WebClip.h"

#include <glim/gpu/Device.h>
#include <glim/paint/Context.h>

#include <glim/paint/Renderer.h>
#include <glim/paint/Scene.h>
#include <glim/shell/Application.h>
#include <glim/shell/RunLoop.h>
#include <glim/shell/Surface.h>
#include <glim/shell/Window.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace {

glim::shell::Window* gWindow = nullptr;
glim::paint::Context* gContext = nullptr;
std::unique_ptr<glim::gpu::Device> gDevice;
std::unique_ptr<glim::paint::Renderer> gRenderer;
bool gReduceTransparency = false;
float gTime = 0.f;
int gZeroStreak = 0;
std::vector<glim::paint::SlotPlacement> gSlots;
enum class Sample { Glass, Video };
Sample gSample = Sample::Glass;
WebClip gClip;

// The clip under a frosted label: the browser producer's frames
// composite with Glim's glass in one pass.
void recordVideo(
  glim::paint::Context& ctx, glim::Vec2 size, double timeSeconds
) {
  ctx.setFillColor(0x0c1014ffu);
  ctx.fill(glim::Rect::fromSize(size));
  if (!gClip.attach(*gDevice, ctx)) return;
  gClip.produce(timeSeconds);
  const float w = std::min(size.x - 48.f, (size.y - 48.f) * 16.f / 9.f);
  const glim::Rect plate{
    {(size.x - w) * 0.5f, (size.y - w * 9.f / 16.f) * 0.5f},
    {w, w * 9.f / 16.f}
  };
  if (const std::uint32_t id = gClip.acquire()) {
    ctx.blit(plate, glim::paint::Matter::foreign(id));
  }
  glim::paint::GroupParams label;
  label.bounds = {
    {plate.origin.x + 16.f, plate.origin.y + 16.f},
    {plate.size.x * 0.4f, 40.f}
  };
  label.clip = label.bounds;
  label.clipRadius = glim::Radius{20.f};
  label.lens = glim::paint::Lens{};
  ctx.pushGroup(label);
  ctx.popGroup();
}

void syncSlots() {
  if (gWindow == nullptr || gContext == nullptr) {
    return;
  }
  std::vector<glim::paint::SlotPlacement> places;
  glim::paint::collectSlots(gContext->scene(), &places);
  std::sort(
    places.begin(),
    places.end(),
    [](const auto& a, const auto& b) { return a.id < b.id; }
  );
  std::vector<std::uint32_t> live;
  gWindow->forEachSlot([&](std::uint32_t id, glim::Rect) {
    live.push_back(id);
  });
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
  gDevice =
    std::make_unique<glim::gpu::Device>(std::move(created.value()));
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
  if (gSample == Sample::Video) {
    recordVideo(*gContext, size, gTime);
  } else {
    recordGlass(
      *gContext, size, gTime, gWindow->safeArea(), gReduceTransparency
    );
  }
  gContext->finish();
  const glim::Vec2 drawable = gWindow->drawableSize();
  gDevice->setDrawableSize(
    static_cast<int>(drawable.x), static_cast<int>(drawable.y)
  );
  gRenderer->draw(gContext->scene());
  syncSlots();
  if (gRenderer->stats().draws == 0 &&
      gRenderer->stats().reuseHits == 0) {
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

extern "C" void glimWebSelect(int example) {
  gSample = example == 1 ? Sample::Video : Sample::Glass;
}

extern "C" void glimWebResize(int w, int h, float pixelRatio) {
  if (gWindow != nullptr) {
    gWindow->setSize(w, h);
    gWindow->setPixelRatio(pixelRatio);
  }
}

extern "C" void glimWebFrame(
  double timeSeconds, int reduceTransparency
) {
  gTime = static_cast<float>(timeSeconds);
  gReduceTransparency = reduceTransparency != 0;
  paintOnce();
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
  (void)type;
  (void)x;
  (void)y;
  (void)key;
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
  host.run();
  paintOnce();
  return 0;
}
