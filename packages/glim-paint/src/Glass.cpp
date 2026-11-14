#include <glim/paint/Scene.h>

#include <algorithm>
#include <cmath>
#include <span>

#include "LensMath.h"

namespace glim::paint {
namespace {

constexpr float kGlassRefArea = 200.f * 80.f;

float sizeScaleFor(const Rect& aabb) {
  const float area = std::max(0.f, aabb.size.x * aabb.size.y);
  const float t = std::clamp(std::sqrt(area / std::max(kGlassRefArea, 1e-6f)), 0.f, 1.f);
  return 1.f + 0.6f * t;
}

void emitPill(std::span<LensPill> out, int& n, Rect rect, float radius) {
  if (n < 0 || static_cast<std::size_t>(n) >= out.size()) {
    return;
  }
  const float minSide = std::min(rect.size.x, rect.size.y);
  LensPill p;
  p.rect = rect;
  p.radius = radius;
  p.superellipseN = 4.f;
  p.kind = radius >= minSide * 0.5f - 1e-3f ? LensPillKind::Capsule : LensPillKind::Rounded;
  out[static_cast<std::size_t>(n)] = p;
  ++n;
}

void collectFills(const Group& g, std::span<LensPill> out, int& n) {
  for (const Shape& s : g.shapes) {
    if (const auto* r = std::get_if<FillRounded>(&s)) {
      emitPill(out, n, r->rect, plateRadius(r->radius));
    }
  }
}

// Logical backdrop blur radius implied by the variant alone.
float glassVariantBlurRadius(const Lens& g) {
  switch (g.variant) {
    case LensVariant::Clear:
      return 6.f;
    case LensVariant::Identity:
      return 0.f;
    case LensVariant::Regular:
    default:
      return 20.f;
  }
}

}  // namespace

bool lensOpaque(const Lens& g) {
  return g.material == LensMaterial::LiquidMetal && g.transmit <= 0.f;
}

float lensBlurRadius(const Lens& g) {
  if (g.flatten || g.variant == LensVariant::Identity || lensOpaque(g)) {
    return 0.f;
  }
  if (g.material == LensMaterial::Water) {
    return 2.f;  // water stays sharp, ripple does the work
  }
  return glassVariantBlurRadius(g);
}

float lensThicknessFor(const Lens& mat, Rect plateBounds) {
  return mat.thicknessPx * sizeScaleFor(plateBounds);
}

bool lensRipples(const Lens& mat) {
  return !mat.flatten && mat.material == LensMaterial::Water && mat.rippleAmp > 0.f;
}

namespace {

bool swellMoves(const Lens& mat) {
  const LensMotion& m = mat.motion;
  return !mat.flatten && mat.material != LensMaterial::Glass && mat.swellAmp > 0.f &&
         m.period > 0.f && m.travel != 0.f;
}

// Unit step response of a damped spring over u in [0, 1]: starts and ends at
// rest, overshoots 1 by `bounce`, and is pinned to exactly 1 at u = 1.
float springSettle(float u, float bounce) {
  constexpr float kPi = 3.14159265359f;
  const float lb = std::log(std::clamp(bounce, 0.01f, 0.9f));
  const float zeta = -lb / std::sqrt(kPi * kPi + lb * lb);
  const float decay = std::log(100.f);  // envelope down to 1% by u = 1
  const float wd = decay / zeta * std::sqrt(1.f - zeta * zeta);
  const auto raw = [&](float x) {
    return 1.f - std::exp(-decay * x) * (std::cos(wd * x) + decay / wd * std::sin(wd * x));
  };
  return raw(u) + (1.f - raw(1.f)) * u;
}

float surge(LensMotionCurve curve, float u, float bounce) {
  return curve == LensMotionCurve::Spring ? springSettle(u, bounce) : lens::smoothstep(0.f, 1.f, u);
}

}  // namespace

bool lensAnimates(const Lens& mat) { return lensRipples(mat) || swellMoves(mat); }

float lensSwellPhase(const Lens& mat) {
  if (!swellMoves(mat)) {
    return 0.f;
  }
  const LensMotion& m = mat.motion;
  const double period = m.period;
  if (m.curve == LensMotionCurve::Flow) {
    // The swell repeats every 2*pi of phase, so wrapping keeps it seamless
    // and precise for any uptime.
    constexpr double kTau = 6.283185307179586;
    const double phase = std::fmod(m.travel / period * mat.timeSec, kTau);
    return static_cast<float>(phase < 0.0 ? phase + kTau : phase);
  }
  const double loop = std::fmod(static_cast<double>(mat.timeSec), 2.0 * period);
  const double t = loop < 0.0 ? loop + 2.0 * period : loop;
  const bool back = t >= period;
  const double local = back ? t - period : t;
  const float share = std::clamp(m.moveShare, 1e-3f, 1.f);
  const float u = std::min(static_cast<float>(local / (period * share)), 1.f);
  const float s = surge(m.curve, u, m.bounce);
  return m.travel * (back ? 1.f - s : s);
}

const Lens& lensOf(const Group& g) {
  static const Lens kDefault{};
  if (g.params.lens.has_value()) {
    return *g.params.lens;
  }
  for (const auto& c : g.children) {
    if (c && c->params.lens.has_value()) {
      return *c->params.lens;
    }
  }
  return kDefault;
}

Rect glassSurface(const Group& g) {
  const bool explicitBounds = g.params.bounds.size.x > 0.f && g.params.bounds.size.y > 0.f;
  Rect aabb = explicitBounds ? g.params.bounds : contentBounds(g);
  const Lens& mat = lensOf(g);
  const float T = lensThicknessFor(mat, aabb);
  const float blur = glassVariantBlurRadius(mat);
  const float slack = mat.dispersion > 0.f ? 16.f : 12.f;
  const float gutter = std::ceil(T + blur + slack);
  aabb.origin.x -= gutter;
  aabb.origin.y -= gutter;
  aabb.size.x += gutter * 2.f;
  aabb.size.y += gutter * 2.f;
  return aabb;
}

int collectLensPills(const Group& g, std::span<LensPill> out) {
  int n = 0;
  if (hasGlass(g)) {
    collectFills(g, out, n);
  }
  if (isGlassContainer(g)) {
    for (const auto& child : g.children) {
      if (!child || !hasGlass(*child)) {
        continue;
      }
      const int before = n;
      collectFills(*child, out, n);
      if (n == before) {
        const bool childBounds =
          child->params.bounds.size.x > 0.f && child->params.bounds.size.y > 0.f;
        Rect b = childBounds ? child->params.bounds : contentBounds(*child);
        emitPill(out, n, b, plateRadius(child->params.clipRadius));
      }
    }
  }
  if (hasGlass(g) && n == 0) {
    const bool ownBounds = g.params.bounds.size.x > 0.f && g.params.bounds.size.y > 0.f;
    Rect b = ownBounds ? g.params.bounds : contentBounds(g);
    emitPill(out, n, b, plateRadius(g.params.clipRadius));
  }
  return n;
}

void dropLensPills(Group& g) { dropBackdropPills(g); }

void clearLensParams(GroupParams& p) {
  p.lens.reset();
  p.glassContainer = false;
}

void stripGlassForIsolate(Group& g) {
  // Nested glass flattens: outer plate wins, inner dropped silently.
  const bool hadGlass = hasGlass(g);
  clearLensParams(g.params);
  if (hadGlass) {
    dropLensPills(g);
  }
  for (auto& child : g.children) {
    if (child) {
      stripGlassForIsolate(*child);
    }
  }
}

}  // namespace glim::paint
