#pragma once

// Composite cases every renderer must agree on: a perspective tilt draws
// its projected quad, and a fade ramps the group's alpha across it.

#include <glim/paint/Context.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace warp_cases {

inline int failures = 0;

inline void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

constexpr int kW = 400;
constexpr int kH = 300;
using Pixels = std::vector<std::uint8_t>;

// A 200x100 plate centered at (200, 150), swung 45 degrees about Y.
inline void recordTilt(glim::paint::Context& ctx) {
  ctx.setSize({kW, kH});
  ctx.beginFrame();
  glim::paint::GroupParams p;
  p.transform = glim::Mat4::translate(200, 150) * glim::Mat4::perspective(400) *
                glim::Mat4::rotateY(3.14159265f / 4) * glim::Mat4::translate(-200, -150);
  ctx.pushGroup(p);
  ctx.setFillColor(0x2050e0ff);
  ctx.fill(glim::Rect{{100, 100}, {200, 100}});
  ctx.popGroup();
  ctx.finish();
}

// How blue a pixel reads over the opaque canvas: the plate is the only blue.
inline int blueness(const Pixels& px, int x, int y) {
  const std::size_t i = static_cast<std::size_t>((y * kW + x) * 4);
  return static_cast<int>(px[i + 2]) - static_cast<int>(px[i]);
}

inline bool filled(const Pixels& px, int x, int y) {
  const std::size_t i = static_cast<std::size_t>((y * kW + x) * 4);
  return px[i + 2] > 128 && px[i] < 100;
}

inline void checkTilt(const Pixels& px, const char* path) {
  std::cout << path << " tilt: center " << filled(px, 200, 150) << " near-edge top "
            << filled(px, 120, 94) << " far-edge top " << filled(px, 245, 104) << '\n';
  expect(filled(px, 200, 150), "the pivot stays covered");
  // The near (left) edge grows toward the viewer: above the flat top.
  expect(filled(px, 120, 94), "near edge reaches above the flat rect");
  // The far (right) edge shrinks: a bounding-box composite would fill here.
  expect(!filled(px, 245, 104), "far edge top is outside the trapezoid");
  expect(!filled(px, 290, 150), "far edge pulls in toward the pivot");
}

// The same plate, flat, fading from opaque at its top to clear at its bottom.
inline void recordFade(glim::paint::Context& ctx) {
  ctx.setSize({kW, kH});
  ctx.beginFrame();
  glim::paint::GroupParams p;
  p.fade = glim::Fade{.from = {200, 100}, .to = {200, 200}, .fromAlpha = 1, .toAlpha = 0};
  ctx.pushGroup(p);
  ctx.setFillColor(0x2050e0ff);
  ctx.fill(glim::Rect{{100, 100}, {200, 100}});
  ctx.popGroup();
  ctx.finish();
}

inline void checkFade(const Pixels& px, const char* path) {
  const int top = blueness(px, 200, 103);
  const int mid = blueness(px, 200, 150);
  const int bottom = blueness(px, 200, 197);
  std::cout << path << " fade: top " << top << " mid " << mid << " bottom " << bottom << '\n';
  expect(top > 150, "the fade starts opaque");
  // Blending runs in linear light, so the encoded midpoint reads above half.
  expect(mid > 60 && mid < top - 20, "it is part way through the middle");
  expect(bottom < top / 4, "it ends nearly clear");
  expect(!filled(px, 200, 90) && !filled(px, 200, 210), "outside the plate stays canvas");
}

// A reflection under a turned page, as the toolkit composes it: inside a
// slight tilt, the plate drawn again flipped about its bottom edge and
// faded. Two isolates, one inside the other.
inline void recordNested(glim::paint::Context& ctx) {
  ctx.setSize({kW, kH});
  ctx.beginFrame();
  glim::paint::GroupParams tilt;
  tilt.transform = glim::Mat4::translate(200, 150) * glim::Mat4::perspective(900) *
                   glim::Mat4::rotateY(0.03f) * glim::Mat4::translate(-200, -150);
  ctx.pushGroup(tilt);
  ctx.setFillColor(0x2050e0ff);
  ctx.fill(glim::Rect{{100, 50}, {200, 100}});
  glim::paint::GroupParams flip;
  flip.transform = glim::Mat4::translate(0, 300) * glim::Mat4::scale(1, -1);
  ctx.pushGroup(flip);
  glim::paint::GroupParams faded;
  faded.fade = glim::Fade{.from = {200, 150}, .to = {200, 100}, .fromAlpha = 1, .toAlpha = 0};
  ctx.pushGroup(faded);
  ctx.fill(glim::Rect{{100, 50}, {200, 100}});
  ctx.popGroup();
  ctx.popGroup();
  ctx.popGroup();
  ctx.finish();
}

inline void checkNested(const Pixels& px, const char* path) {
  const int page = blueness(px, 200, 100);
  const int seam = blueness(px, 200, 153);
  const int tail = blueness(px, 200, 199);
  std::cout << path << " nested: page " << page << " seam " << seam << " tail " << tail << '\n';
  expect(page > 150, "the page shows inside the tilt");
  expect(seam > 150, "its reflection starts opaque under it");
  expect(tail < seam / 4, "and fades out nearly clear");
}

struct Case {
  void (*record)(glim::paint::Context&);
  void (*check)(const Pixels&, const char*);
};
inline constexpr Case kCases[] = {
  {recordTilt, checkTilt}, {recordFade, checkFade}, {recordNested, checkNested}
};

}  // namespace warp_cases
