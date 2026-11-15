#include <glim/gpu/Device.h>
#include <glim/image/Png.h>
#include <glim/paint/Renderer.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#include "Video.h"
#include "VideoSource.h"
#include "glim/toolkit/host/TestApp.h"

using namespace glim::toolkit;

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

struct Rig {
  glim::gpu::Device& device;
  glim::paint::Renderer& renderer;
  glim::paint::Context& ctx;
  host::TestApp& app;
};

glim::image::Image frame(Rig& r) {
  r.app.step(0.f);
  r.app.recordScene(r.ctx);
  const int w = static_cast<int>(r.app.w);
  const int h = static_cast<int>(r.app.h);
  glim::image::Image im = glim::image::Image::rgba8(w, h);
  auto target = r.device.createFrameTarget({w, h});
  if (!target.ok()) return im;
  r.renderer.drawToTarget(r.ctx.scene(), target.value());
  r.device.readPixels(target.value(), im.rgba.data(), im.rgba.size());
  return im;
}

// The plate's lower half, clear of the glass label at its top-left.
std::vector<std::uint8_t> plate(const glim::image::Image& im, const Box& b) {
  std::vector<std::uint8_t> out;
  for (int y = static_cast<int>(b.y + b.h * 0.5f); y < static_cast<int>(b.y + b.h - 4); ++y) {
    const std::size_t row = static_cast<std::size_t>(y * im.width) * 4;
    const std::size_t x0 = row + static_cast<std::size_t>(b.x + 4) * 4;
    const std::size_t x1 = row + static_cast<std::size_t>(b.x + b.w - 4) * 4;
    out.insert(out.end(), im.rgba.begin() + x0, im.rgba.begin() + x1);
  }
  return out;
}

bool lit(const std::vector<std::uint8_t>& px) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < px.size(); i += 4) sum += px[i] + px[i + 1] + px[i + 2];
  return sum / (px.size() / 4) > 3 * 0x30;
}

void wait(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

}  // namespace

int main(int argc, char** argv) {
  @autoreleasepool {
    auto created = glim::gpu::Device::create({});
    if (!created.ok()) {
      std::cerr << "headless Metal device failed: " << created.message() << '\n';
      return EXIT_FAILURE;
    }
    glim::gpu::Device device = std::move(created.value());
    glim::paint::Renderer renderer(device);
    glim::paint::Context ctx;
    std::unique_ptr<VideoSource> source = makeVideoSource();
    if (!source) {
      std::cerr << "no video producer\n";
      return EXIT_FAILURE;
    }

    host::TestApp app;
    app.w = 960;
    app.h = 640;
    app.build = [&](const Engine& e) {
      VideoView view{.playing = rootScope(e).state("playing", true)};
      source->setPlaying(*view.playing);
      view.image = source->acquire();
      return videoTree(std::move(view));
    };
    app.prepare = [&](glim::paint::Context& c, Store&) { source->attach(c); };
    Rig rig{device, renderer, ctx, app};

    frame(rig);
    for (int i = 0; i < 100 && source->stats().acquired == 0; ++i) {
      wait(20);
      frame(rig);
    }
    expect(source->stats().acquired > 0, "a video frame became ready");

    const Box tile = focusBox(app.tree, app.box, 0, app.engine());
    const glim::image::Image a = frame(rig);
    wait(200);
    const glim::image::Image b = frame(rig);
    expect(lit(plate(a, tile)), "video shows on the plate");
    expect(plate(a, tile) != plate(b, tile), "video moves while playing");
    if (argc > 1) glim::image::Png::write(argv[1], b);

    expect(app.tapIndex(0), "tap hits the video");
    expect(!*app.store.state("playing", true), "tap pauses");
    wait(100);
    frame(rig);
    const glim::image::Image c = frame(rig);
    wait(200);
    const glim::image::Image d = frame(rig);
    expect(plate(c, tile) == plate(d, tile), "paused video holds its frame");
    const std::uint64_t pausedAt = source->stats().published;

    expect(app.tapIndex(0), "second tap hits");
    expect(*app.store.state("playing", true), "tap resumes");
    wait(200);
    frame(rig);
    expect(source->stats().published > pausedAt, "producer resumes");

    const host::FeedStats s = source->stats();
    std::cout << "video feed published=" << s.published << " acquired=" << s.acquired
              << " fence_misses=" << s.fenceMisses << " dropped=" << s.dropped
              << " max_lag=" << s.maxLag << '\n';
  }
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
