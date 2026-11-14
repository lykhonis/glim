#pragma once

// Single owner for the scalar lens kernels shared by the CPU reference
// and the three hand-ported lens shaders (Metal/WGSL/Vulkan). Pure,
// deterministic, constexpr-friendly: std::pow is spelled as an explicit
// multiply chain so results do not depend on libm rounding.
//
// The GPU ports mirror these formulas by hand (see glass.metal/glass.wgsl/
// glass.frag); the uniform-liveness gate plus the golden keep them honest
// until D2 lands the single-source shader.

#include <algorithm>
#include <cmath>

namespace glim::paint::lens {

inline float saturate(float x) { return std::clamp(x, 0.f, 1.f); }

// Key light azimuth (-60 deg), shared by the specular and glare paths.
inline constexpr float kKeyLightAngle = -1.047f;

struct Vec3 {
  float x, y, z;
};

// Unit half vector between the key light (tilted 0.8 out of the plane)
// and a viewer straight above.
inline Vec3 keyHalfVector() {
  const float lx = std::cos(kKeyLightAngle);
  const float ly = std::sin(kKeyLightAngle);
  const float llen = std::sqrt(lx * lx + ly * ly + 0.64f);
  const float hx = lx / llen;
  const float hy = ly / llen;
  const float hz = 0.8f / llen + 1.f;
  const float hlen = std::sqrt(hx * hx + hy * hy + hz * hz);
  return {hx / hlen, hy / hlen, hz / hlen};
}

// Hermite smoothstep with the degenerate-interval guard the shaders use.
// Requires e0 < e1 (reversed edges are undefined in WGSL/GLSL): write a
// falling ramp as 1 - smoothstep(lo, hi, x).
inline float smoothstep(float e0, float e1, float x) {
  const float t = std::clamp((x - e0) / std::max(e1 - e0, 1e-4f), 0.f, 1.f);
  return t * t * (3.f - 2.f * t);
}

// Circular smooth-min: the single field blend in all four ports
// (matches sminCirc in glass.metal / glass.wgsl / glass.frag).
inline float sminCirc(float a, float b, float k) {
  if (k <= 0.001f) {
    return std::min(a, b);
  }
  const float ks = k / (1.f - std::sqrt(0.5f));
  const float h = std::max(ks - std::fabs(a - b), 0.f) / ks;
  const float t = std::max(1.f - h * (h - 2.f), 0.f);
  return std::min(a, b) - ks * 0.5f * (1.f + h - std::sqrt(t));
}

// Schlick Fresnel: F0 at normal incidence, 1.0 at grazing.
inline float fresnelSchlick(float f0, float cosT) {
  const float t = std::clamp(1.f - cosT, 0.f, 1.f);
  const float t2 = t * t;
  return f0 + (1.f - f0) * t2 * t2 * t;
}

// Inner hairline band: 1 at the band center, 0 outside, quintic falloff
// (matches the per-side edge-light weight in all four ports).
inline float hairWeight(float inside, float center, float halfWidth) {
  const float h = saturate(1.f - std::fabs(inside - center) / halfWidth);
  const float h2 = h * h;
  return h2 * h2 * h;
}

// Rim refraction (mirrors the wrap block in all three lens shaders).
// delta: px inside the edge; wrapT: band width px. Returns the smooth
// wrap weight (1 at the rim, 0 with zero slope at the band edge) and the
// outward sample displacement in px for a given ior.
struct RimRefract {
  float wrapX = 0.f;
  float magPx = 0.f;
};

inline RimRefract rimRefract(
  float delta, float wrapT, float ior, float refDistance, float dprN, float fillet, float diag
) {
  RimRefract o;
  if (delta >= wrapT) {
    return o;
  }
  const float t = saturate(delta / std::max(wrapT, 1.f));
  o.wrapX = 1.f - t * t * (3.f - 2.f * t);
  const float thetaI = std::asin(std::clamp(std::pow(o.wrapX, 1.25f), -1.f, 1.f));
  const float eta = std::max(ior, 1.01f);
  const float thetaT = std::asin(std::clamp(std::sin(thetaI) / eta, -1.f, 1.f));
  const float wrapE = -std::tan(thetaT - thetaI);
  const float distK = refDistance / 0.35f;
  const float filletK = 0.7f + 0.5f * fillet;
  const float diagK = 1.f - 0.5f * diag;
  o.magPx = wrapE * 32.f * distK * dprN * filletK * diagK;
  return o;
}

// Dome normal tilt for an opaque reflective plate: |n.xy| = 1 at the rim,
// falling linearly to 0 at depth R (sphere/cylinder cross-section), so the
// cap centers face the viewer instead of spinning the env into rings.
inline float domeTilt(float inside, float radius) {
  return saturate(1.f - inside / std::max(radius, 1.f));
}

// Soft streak profile: 1 at c, smoothly 0 at |e - c| >= w.
inline float streak(float e, float c, float w) {
  const float t = saturate(1.f - std::fabs(e - c) / std::max(w, 1e-4f));
  return t * t * (3.f - 2.f * t);
}

// Chrome studio: luminance of the room a mirror sees along reflection R
// (screen y down). Soft white walls, brighter overhead, crossed by dark
// seams (horizon, wall, floor) and one bright softbox above the horizon.
// Folded liquid metal sweeps R across these into flowing streaks.
inline float chromeStudio(float ry) {
  const float e = -ry;
  const float wall = 0.55f + 0.3f * smoothstep(-1.f, 0.9f, e);
  const float seams = 0.9f * streak(e, -0.15f, 0.32f) + 0.75f * streak(e, 0.62f, 0.22f) +
                      0.6f * streak(e, -0.85f, 0.25f);
  return wall * (1.f - seams) + 0.4f * streak(e, 0.25f, 0.16f);
}

// Chrome rim: the rolled edge mirrors the softbox above on its upper lip and
// the dark floor below on its lower lip. ny: outward edge normal y (screen
// down is +), so the top edge (ny = -1) is bright and the bottom dark.
inline float chromeRim(float ny) { return 0.18f + 0.82f * smoothstep(-0.5f, 0.6f, -ny); }

// Surface swell (water body, liquid-metal folds): a three-wave height field
// in dome-radius units, plate-local. phase (radians) drifts the waves at
// rates 1, -1, 2, so the field repeats exactly when phase wraps 2*pi.
// (sx, sy): slope, |s| <= 1. curv: -laplacian in [-1, 1]; crests (> 0)
// focus light.
struct Swell {
  float sx, sy, curv;
};

inline Swell swellAt(float u, float v, float phase) {
  const float a1 = 1.9f * u + 1.1f * v + 0.4f + phase;
  const float a2 = -1.3f * u + 2.6f * v + 2.1f - phase;
  const float a3 = 3.4f * u - 2.1f * v + 4.7f + 2.f * phase;
  const float c1 = std::cos(a1), c2 = std::cos(a2), c3 = std::cos(a3);
  Swell s;
  s.sx = (0.95f * c1 - 0.39f * c2 + 0.68f * c3) * 0.5f;
  s.sy = (0.55f * c1 + 0.78f * c2 - 0.42f * c3) * 0.5f;
  s.curv = (2.41f * std::sin(a1) + 2.535f * std::sin(a2) + 3.194f * std::sin(a3)) * 0.125f;
  return s;
}

}  // namespace glim::paint::lens
