// LensMath.h scalar kernels pinned against tabulated values.
// Anchors are closed-form (hand-computed below), not copied from the code.

#include "LensMath.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

bool near(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) <= eps; }

}  // namespace

int main() {
  using namespace glim::paint::lens;

  // smoothstep: midpoints, ends, clamping, degenerate guard.
  expect(smoothstep(0.f, 1.f, 0.5f) == 0.5f, "smoothstep midpoint");
  expect(smoothstep(0.f, 1.f, 0.f) == 0.f, "smoothstep lo edge");
  expect(smoothstep(0.f, 1.f, 1.f) == 1.f, "smoothstep hi edge");
  expect(smoothstep(0.f, 1.f, -2.f) == 0.f, "smoothstep clamps low");
  expect(smoothstep(0.f, 1.f, 2.f) == 1.f, "smoothstep clamps high");
  expect(near(smoothstep(2.f, 6.f, 4.f), 0.5f), "smoothstep shifted midpoint");
  expect(smoothstep(1.f, 1.f, 0.5f) == 0.f, "smoothstep degenerate below");
  expect(smoothstep(1.f, 1.f, 2.f) == 1.f, "smoothstep degenerate above");
  // Env-soft band shape used by the metal stripes: smoothstep(-0.6, 0.6, 0).
  expect(near(smoothstep(-0.6f, 0.6f, 0.f), 0.5f), "smoothstep symmetric band");

  // sminCirc: no-blend, disjoint (h = 0), and hand-computed blends.
  // sminCirc(0, 0, 4): scaled k falls out, result is exactly -k.
  // sminCirc(0, 10, 2): |a-b| exceeds scaled k, h = 0, result is min.
  expect(sminCirc(3.f, 7.f, 0.f) == 3.f, "sminCirc k=0 takes min");
  expect(sminCirc(7.f, 3.f, 0.0005f) == 3.f, "sminCirc tiny k takes min");
  expect(sminCirc(0.f, 10.f, 2.f) == 0.f, "sminCirc disjoint untouched");
  expect(near(sminCirc(0.f, 0.f, 4.f), -4.f, 1e-4f), "sminCirc tabulated neck");
  expect(sminCirc(0.f, 0.f, 4.f) < 0.f, "sminCirc carves the neck");
  // Symmetry in the pair (identical ops both orders).
  expect(sminCirc(2.f, 5.f, 3.f) == sminCirc(5.f, 2.f, 3.f), "sminCirc symmetric");
  expect(sminCirc(-3.f, 4.f, 2.f) <= -3.f, "sminCirc never above min");

  // fresnelSchlick: F0 at normal incidence, 1 at grazing, hand value at 0.5.
  // F(0.04, 0.5) = 0.04 + 0.96 * 0.5^5 = 0.04 + 0.03 = 0.07.
  expect(fresnelSchlick(0.04f, 1.f) == 0.04f, "fresnel normal incidence");
  expect(fresnelSchlick(0.9f, 1.f) == 0.9f, "fresnel metallic F0");
  expect(fresnelSchlick(0.04f, 0.f) == 1.f, "fresnel grazing");
  expect(near(fresnelSchlick(0.04f, 0.5f), 0.07f), "fresnel tabulated mid");
  // Grazing clamp: cosT slightly negative still yields 1, never above.
  expect(fresnelSchlick(0.04f, -0.5f) == 1.f, "fresnel clamps past grazing");

  // hairWeight: 1 at center, 0 at/ past the band edge, quintic mid.
  // hairWeight(0.3, 1.2, 1.8): 1 - 0.9/1.8 = 0.5 -> 0.5^5 = 0.03125.
  expect(hairWeight(1.2f, 1.2f, 1.8f) == 1.f, "hair band center");
  expect(hairWeight(3.0f, 1.2f, 1.8f) == 0.f, "hair band edge");
  expect(hairWeight(99.f, 1.2f, 1.8f) == 0.f, "hair far outside");
  expect(hairWeight(-5.f, 1.2f, 1.8f) == 0.f, "hair far inside");
  expect(near(hairWeight(0.3f, 1.2f, 1.8f), 0.03125f), "hair tabulated mid");
  expect(
    near(hairWeight(1.2f + 0.45f, 1.2f, 1.8f), hairWeight(1.2f - 0.45f, 1.2f, 1.8f), 1e-6f),
    "hair symmetric"
  );

  // domeTilt: 1 at the rim, linear to 0 at the radius, clamped beyond.
  expect(domeTilt(0.f, 10.f) == 1.f, "dome rim full tilt");
  expect(near(domeTilt(5.f, 10.f), 0.5f), "dome halfway");
  expect(domeTilt(10.f, 10.f) == 0.f, "dome center flat");
  expect(domeTilt(50.f, 10.f) == 0.f, "dome clamps deep");
  expect(near(domeTilt(0.5f, 0.f), 0.5f), "dome radius guard");

  // rimRefract: none past the band, full weight at the rim, monotone fade.
  {
    const RimRefract out = rimRefract(20.f, 12.f, 1.45f, 0.35f, 1.f, 0.f, 0.f);
    expect(out.wrapX == 0.f && out.magPx == 0.f, "rimRefract outside band");
    const RimRefract rim = rimRefract(0.f, 12.f, 1.45f, 0.35f, 1.f, 0.f, 0.f);
    expect(rim.wrapX == 1.f, "rimRefract rim weight");
    expect(rim.magPx != 0.f, "rimRefract bends at rim");
    const RimRefract mid = rimRefract(6.f, 12.f, 1.45f, 0.35f, 1.f, 0.f, 0.f);
    expect(near(mid.wrapX, 0.5f), "rimRefract smoothstep midpoint");
    const RimRefract edge = rimRefract(11.99f, 12.f, 1.45f, 0.35f, 1.f, 0.f, 0.f);
    expect(edge.wrapX < 1e-3f, "rimRefract zero slope at band edge");
    // Higher ior bends less; diagonal damps by half at diag=1; dpr scales.
    const RimRefract hi = rimRefract(2.f, 12.f, 2.0f, 0.35f, 1.f, 0.f, 0.f);
    const RimRefract lo = rimRefract(2.f, 12.f, 1.2f, 0.35f, 1.f, 0.f, 0.f);
    expect(!near(lo.magPx, hi.magPx, 1e-4f), "rimRefract ior responds");
    const RimRefract dia = rimRefract(2.f, 12.f, 1.45f, 0.35f, 1.f, 0.f, 1.f);
    const RimRefract str = rimRefract(2.f, 12.f, 1.45f, 0.35f, 1.f, 0.f, 0.f);
    expect(near(dia.magPx, str.magPx * 0.5f, 1e-4f), "rimRefract diagonal damp");
    const RimRefract d2 = rimRefract(2.f, 12.f, 1.45f, 0.35f, 2.f, 0.f, 0.f);
    expect(near(d2.magPx, str.magPx * 2.f, 1e-4f), "rimRefract dpr scale");
  }

  // smoothstep needs e0 < e1; falling ramps are written 1 - smoothstep.
  expect(near(smoothstep(3.f, 5.f, 4.f), 0.5f), "smoothstep midpoint");

  // streak: 1 at its center, 0 at its width, symmetric.
  expect(streak(0.3f, 0.3f, 0.2f) == 1.f && streak(0.55f, 0.3f, 0.2f) == 0.f, "streak ends");
  expect(near(streak(0.2f, 0.3f, 0.2f), streak(0.4f, 0.3f, 0.2f)), "streak symmetric");

  // chromeStudio: dark seams (horizon, wall, floor) cut soft white walls;
  // the softbox is the brightest band.
  {
    const float horizon = chromeStudio(0.15f);
    const float wallSeam = chromeStudio(-0.62f);
    const float softbox = chromeStudio(-0.25f);
    const float wall = chromeStudio(-0.9f);
    expect(horizon < 0.2f && wallSeam < 0.35f, "chromeStudio dark seams");
    expect(softbox > wall && wall > wallSeam, "chromeStudio softbox > wall > seam");
    for (float ry = -1.f; ry <= 1.f; ry += 0.05f) {
      const float l = chromeStudio(ry);
      expect(l >= 0.f && l <= 1.25f, "chromeStudio range");
    }
  }

  // chromeRim: the rolled edge is bright on the upper lip, dark on the
  // lower one, monotone in between and within [0, 1].
  {
    expect(chromeRim(-1.f) > 0.95f, "chromeRim upper lip bright");
    expect(chromeRim(1.f) < 0.2f, "chromeRim lower lip dark");
    for (float ny = -1.f; ny < 1.f; ny += 0.1f) {
      expect(chromeRim(ny) >= chromeRim(ny + 0.1f), "chromeRim monotone");
      expect(chromeRim(ny) >= 0.f && chromeRim(ny) <= 1.f, "chromeRim range");
    }
  }

  // swellAt: slope is the gradient of the height field (central
  // differences) at any phase, bounded, and the field loops every 2*pi.
  {
    constexpr float kTau = 6.28318530718f;
    for (const float ph : {0.f, 1.3f, 4.f}) {
      const auto height = [ph](double u, double v) {
        return 0.5 * std::sin(1.9 * u + 1.1 * v + 0.4 + ph) +
               0.3 * std::sin(-1.3 * u + 2.6 * v + 2.1 - ph) +
               0.2 * std::sin(3.4 * u - 2.1 * v + 4.7 + 2.0 * ph);
      };
      for (float u = -3.f; u <= 3.f; u += 0.7f) {
        for (float v = -1.f; v <= 1.f; v += 0.4f) {
          const Swell sw = swellAt(u, v, ph);
          const double h = 1e-4;
          const double gx = (height(u + h, v) - height(u - h, v)) / (2.0 * h) * 0.5;
          const double gy = (height(u, v + h) - height(u, v - h)) / (2.0 * h) * 0.5;
          expect(near(sw.sx, static_cast<float>(gx), 1e-3f), "swellAt sx is the gradient");
          expect(near(sw.sy, static_cast<float>(gy), 1e-3f), "swellAt sy is the gradient");
          expect(std::fabs(sw.sx) <= 1.f && std::fabs(sw.sy) <= 1.f, "swellAt slope bound");
          expect(std::fabs(sw.curv) <= 1.02f, "swellAt curvature bound");
          const Swell loop = swellAt(u, v, ph + kTau);
          expect(
            near(sw.sx, loop.sx, 1e-4f) && near(sw.sy, loop.sy, 1e-4f) &&
              near(sw.curv, loop.curv, 1e-4f),
            "swellAt loops every 2*pi"
          );
        }
      }
    }
    expect(
      !near(swellAt(0.3f, 0.2f, 0.f).sx, swellAt(0.3f, 0.2f, 1.f).sx, 1e-2f),
      "swellAt drifts with phase"
    );
  }

  // Cross-check against double-precision recomputation (independent path).
  {
    const double t = 0.37;
    const double tt = t * t;
    const float want = static_cast<float>(0.04 + 0.96 * tt * tt * t);
    expect(
      near(fresnelSchlick(0.04f, static_cast<float>(1.0 - t)), want, 1e-6f),
      "fresnel matches double recomputation"
    );
  }

  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return EXIT_FAILURE;
  }
  std::cout << "lensmath_test ok\n";
  return EXIT_SUCCESS;
}
