#include <glim/paint/Context.h>
#include <glim/paint/Scene.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

bool same(const glim::Rect& a, const glim::Rect& b) {
  return a.origin.x == b.origin.x && a.origin.y == b.origin.y && a.size.x == b.size.x &&
         a.size.y == b.size.y;
}

}  // namespace

int main() {
  using glim::Rect;
  glim::paint::Context ctx;

  // Outside a frame nothing records.
  ctx.fill(Rect{{0, 0}, {4, 4}});
  ctx.fillRounded(Rect{{0, 0}, {4, 4}}, glim::Radius{});
  ctx.fillChamfer(Rect{{0, 0}, {4, 4}}, 1.f);
  ctx.strokeRect(Rect{{0, 0}, {4, 4}}, 1.f);
  ctx.strokeChamfer(Rect{{0, 0}, {4, 4}}, 1.f, 1.f);
  ctx.arc({.center = {2, 2}, .radius = 2.f, .sweepDeg = 90.f});
  ctx.polyline({.points = {{0, 0}, {4, 4}}});
  ctx.clipRect(Rect{{0, 0}, {4, 4}});
  expect(ctx.scene().root.shapes.empty(), "no recording outside a frame");

  ctx.setSize({64, 64});
  ctx.beginFrame();
  ctx.fillChamfer(Rect{{0, 0}, {8, 8}}, 2.f);
  ctx.strokeChamfer(Rect{{0, 0}, {8, 8}}, 2.f, 1.f);
  ctx.strokeRect(Rect{{0, 0}, {8, 8}}, 0.f);
  ctx.strokeChamfer(Rect{{0, 0}, {8, 8}}, 2.f, 0.f);
  ctx.arc({.center = {4, 4}, .radius = 4.f, .sweepDeg = 180.f});
  ctx.polyline({.points = {{0, 0}, {8, 8}}, .width = 2.f});

  ctx.clipRect(Rect{{0, 0}, {32, 32}}, glim::Radius{4, 4, 4, 4});
  ctx.clipRect(Rect{{16, 16}, {32, 32}});
  const glim::paint::GroupParams clipped = ctx.scene().root.params;
  ctx.clipRect(Rect{{40, 40}, {8, 8}});
  const glim::paint::GroupParams disjoint = ctx.scene().root.params;
  ctx.finish();

  const auto& shapes = ctx.scene().root.shapes;
  expect(shapes.size() == 4, "chamfer, chamfer stroke, arc, polyline; zero widths skipped");
  expect(std::holds_alternative<glim::paint::StrokeChamfer>(shapes[1]), "chamfer stroke kept");
  expect(same(clipped.clip, Rect{{16, 16}, {16, 16}}), "second clip intersects the first");
  expect(clipped.clipRadius.lt == 0.f, "intersected clip drops its radius");
  expect(same(disjoint.clip, Rect{}), "disjoint clip collapses to empty");

  // Image store rejects what it cannot hold.
  const std::vector<std::uint8_t> px(16, 255);
  expect(ctx.addImage(2, 2, nullptr) == 0, "null pixels");
  expect(ctx.addImage(0, 2, px.data()) == 0, "empty image");
  expect(ctx.addImage(4096, 4096, px.data()) == 0, "over the byte budget");
  int surface = 0;
  const std::uint32_t id = ctx.wrapSurface(&surface, 2, 2);
  const auto img = ctx.images().get(id);
  expect(
    img && img->get().foreign && img->get().nativeKind == glim::paint::NativeKind::Surface &&
      img->get().format == glim::paint::SampleFormat::Bgra8Unorm,
    "wrapSurface: foreign BGRA surface"
  );
  ctx.releaseImage(0);
  ctx.releaseImage(9999);
  expect(ctx.images().get(id).has_value(), "bad release ids are ignored");

  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
