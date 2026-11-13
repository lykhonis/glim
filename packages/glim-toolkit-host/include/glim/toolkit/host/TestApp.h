#pragma once
// Headless harness for any toolkit Build: tap/key/wheel/step + dump/a11y/ops/png.
#include <glim/image/Image.h>
#include <glim/paint/Context.h>
#include <glim/paint/FramePacket.h>
#include <glim/toolkit.h>
#include <glim/toolkit/host/Host.h>
#include <glim/toolkit/host/Paint.h>

#include <functional>
#include <string>
#include <vector>
namespace glim {
namespace toolkit {
namespace host {
struct TestApp {
  Build build;
  // Per-frame context setup before paint (e.g. registering cached images).
  std::function<void(::glim::paint::Context&, Store&)> prepare;
  // Paints the frame instead of the tree, for apps that draw the context
  // directly (the headless form of a host Record).
  std::function<void(::glim::paint::Context&, Vec2 size, float now)> paint;
  Store store;
  Screen screen;
  LayerFeed feed;
  float w = 1024, h = 640, now = 0;
  Block tree;
  Box box{};
  bool laid = false;
  Engine engine();
  void rebuild();
  bool tap(float x, float y);
  bool tapIndex(int i);
  bool key(Key k);
  bool wheel(float x, float y, float dx, float dy);
  void step(float dt);
  struct Snap {
    std::string dump, a11y;
    std::vector<Op> ops;
    ::glim::image::Image png;
    // Timed split for agents/CI: dump (tree + a11y text), scene record,
    // encode + raster.
    float buildMs = 0, recordMs = 0, encodeMs = 0;
    ::glim::paint::Stats stats{};
    std::uint64_t revision = 0;
  };
  // The one frame recording path for headless snapshots and the probe.
  void recordScene(::glim::paint::Context& ctx);
  Snap snapshotTimed(bool withRaster = true);
  std::string dumpStats(const Snap& s);
  bool writePng(const std::string& path);
  bool writeOps(const std::string& path);
};
}  // namespace host
}  // namespace toolkit
}  // namespace glim
