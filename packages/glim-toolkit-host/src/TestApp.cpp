#include <glim/image/Png.h>
#include <glim/paint/Software.h>
#include <glim/toolkit/host/Codec.h>
#include <glim/toolkit/host/TestApp.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace glim {
namespace toolkit {
namespace host {

Engine TestApp::engine() {
  Engine e = measureEngine();
  e.store = store;
  return e;
}
void TestApp::rebuild() {
  Engine e = engine();
  tree = build(e);
  box = e.place(tree, {0, w, 0, h});
  laid = true;
}
bool TestApp::tap(float x, float y) {
  if (!laid) rebuild();
  Engine e = engine();
  screen.pointerMove(tree, box, x, y, false, 1, e);
  bool hit = screen.pointerDown(tree, box, x, y, 1, e);
  screen.pointerUp(tree, box, x, y, e);
  rebuild();
  return hit;
}
bool TestApp::tapIndex(int i) {
  if (!laid) rebuild();
  Engine e = engine();
  auto fs = focusables(tree, box, e);
  if (i < 0 || i >= static_cast<int>(fs.size())) return false;
  Box f = focusBox(tree, box, i, e);
  return tap(f.x + 1, f.y + 1);
}
bool TestApp::key(Key k) {
  if (!laid) rebuild();
  Engine e = engine();
  bool r = screen.key(tree, box, k, e);
  rebuild();
  return r;
}
bool TestApp::wheel(float x, float y, float dx, float dy) {
  if (!laid) rebuild();
  Engine e = engine();
  bool r = screen.wheel(tree, box, x, y, dx, dy, e);
  rebuild();
  return r;
}
void TestApp::step(float dt) {
  now += dt;
  store.stepValues(now);
  screen.clock.now = now;
  if (laid) screen.tick(tree, box, engine());
  rebuild();
}
void TestApp::recordScene(::glim::paint::Context& ctx) {
  if (!laid) rebuild();
  ctx.setSize({w, h});
  ctx.beginFrame();
  if (prepare) prepare(ctx, store);
  if (paint)
    paint(ctx, {w, h}, now);
  else
    paintScreen(ctx, tree, box, engine(), feed, screen.focus, screen.focusVisible);
  ctx.finish();
}
TestApp::Snap TestApp::snapshotTimed(bool withRaster) {
  using clk = std::chrono::steady_clock;
  rebuild();
  Engine e = engine();
  Snap s;
  auto t0 = clk::now();
  std::ostringstream os;
  dump(os, tree, box, e);
  s.dump = os.str();
  std::ostringstream as;
  dumpA11y(as, a11yNodes(tree, box, e));
  s.a11y = as.str();
  auto t1 = clk::now();
  // Scene first: prepare registers cached images, so ops recorded after
  // it describe the same frame the scene paints, even on the first snap.
  ::glim::paint::Context ctx;
  recordScene(ctx);
  auto t2 = clk::now();
  OpsRecorder rec;
  paintTree(tree, box, e, rec, feed);
  paintFocusRing(tree, box, screen.focus, rec, e, screen.focusVisible);
  s.ops = rec.ops;
  auto t3 = clk::now();
  // Stats via encode (merge, no raster) — fast, deterministic.
  s.stats = ::glim::paint::encode(ctx.scene(), 1.f).stats;
  if (withRaster) {
    s.png = ::glim::image::Image::rgba8(static_cast<int>(w), static_cast<int>(h));
    if (!s.png.empty())
      ::glim::paint::rasterScene(ctx.scene(), s.png.width, s.png.height, s.png.rgba.data());
  }
  auto t4 = clk::now();
  s.buildMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
  s.recordMs = std::chrono::duration<float, std::milli>(t2 - t1).count();
  s.encodeMs = std::chrono::duration<float, std::milli>(t4 - t3).count();
  s.revision = store.revision();
  (void)screen.needsPaint(store);
  screen.painted(store);
  return s;
}
std::string TestApp::dumpStats(const Snap& s) {
  char b[256];
  std::snprintf(
      b,
      sizeof(b),
      "build %.2fms record %.2fms encode %.2fms draws %u instances %u "
      "tiles %u dirty %u reuse %u/%u glass %u backdrop %u rev %llu ops %zu",
      static_cast<double>(s.buildMs),
      static_cast<double>(s.recordMs),
      static_cast<double>(s.encodeMs),
      s.stats.draws,
      s.stats.instances,
      s.stats.tileCount,
      s.stats.dirtyTiles,
      s.stats.reuseHits,
      s.stats.reuseMisses,
      s.stats.glassPassCount,
      s.stats.backdropCount,
      static_cast<unsigned long long>(s.revision),
      s.ops.size()
  );
  return b;
}
bool TestApp::writePng(const std::string& path) {
  return ::glim::image::Png::write(path.c_str(), snapshotTimed(true).png);
}
bool TestApp::writeOps(const std::string& path) {
  std::vector<std::uint8_t> bytes;
  encodeOps(snapshotTimed(false).ops, bytes);
  std::ofstream out(path, std::ios::binary);
  if (!out) return false;
  out.write(
      reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())
  );
  return static_cast<bool>(out);
}

}  // namespace host
}  // namespace toolkit
}  // namespace glim
