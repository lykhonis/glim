#include "Isolates.h"

#include <glim/paint/ColorSpace.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "../LensMath.h"

namespace glim::paint {

void setProjection(gpu::Pass& pass, const Mat4& m) {
  Uniforms u{};
  std::memcpy(u.projection, m.m, sizeof(u.projection));
  pass.setBytes(1, &u, sizeof(u));
}

Mat4 presentProjection(const Mat4& ortho, int degrees) {
  if (degrees == 0) {
    return ortho;
  }
  Mat4 r = Mat4::identity();
  switch (degrees) {
    case 90:
      r.m[0] = 0.f;
      r.m[1] = 1.f;
      r.m[4] = -1.f;
      r.m[5] = 0.f;
      break;
    case 180:
      r.m[0] = -1.f;
      r.m[5] = -1.f;
      break;
    case 270:
      r.m[0] = 0.f;
      r.m[1] = -1.f;
      r.m[4] = 1.f;
      r.m[5] = 0.f;
      break;
    default:
      return ortho;
  }
  return r * ortho;
}

GlassUniforms makeGlassUniforms(
    const Lens& g,
    const LensPill* pills,
    int n,
    int iw,
    int ih,
    float pixelRatio,
    float u0,
    float v0,
    float u1,
    float v1
) {
  GlassUniforms u{};
  u.resolution[0] = static_cast<float>(iw);
  u.resolution[1] = static_cast<float>(ih);
  u.dpr = pixelRatio > 0.f ? pixelRatio : 1.f;
  const int count = std::max(0, std::min(kMaxLensPills, n));
  u.shapeCount = static_cast<float>(count);
  for (int i = 0; i < count; ++i) {
    const LensPill& p = (pills && n > 0) ? pills[i] : LensPill{};
    Rect r = p.rect;
    u.shapes[i].center[0] = r.origin.x + r.size.x * 0.5f;
    u.shapes[i].center[1] = r.origin.y + r.size.y * 0.5f;
    u.shapes[i].halfExtent[0] = r.size.x * 0.5f;
    u.shapes[i].halfExtent[1] = r.size.y * 0.5f;
    u.shapes[i].corner = p.radius;
    u.shapes[i].n = p.superellipseN > 0.f ? p.superellipseN : 4.f;
    u.shapes[i].mergeK = g.mergeKPx;
  }
  const bool ident = g.variant == LensVariant::Identity;
  const bool isGlass = g.material == LensMaterial::Glass;
  const bool isMetal = g.material == LensMaterial::LiquidMetal;
  // Same size scale as the glassSurface gutter, from the pills' bbox.
  float bx0 = 0.f;
  float by0 = 0.f;
  float bx1 = 0.f;
  float by1 = 0.f;
  for (int i = 0; i < count; ++i) {
    const LensPill& q = (pills && n > 0) ? pills[i] : LensPill{};
    const float x0 = q.rect.origin.x;
    const float y0 = q.rect.origin.y;
    const float x1 = x0 + q.rect.size.x;
    const float y1 = y0 + q.rect.size.y;
    if (i == 0 || x0 < bx0) {
      bx0 = x0;
    }
    if (i == 0 || y0 < by0) {
      by0 = y0;
    }
    if (i == 0 || x1 > bx1) {
      bx1 = x1;
    }
    if (i == 0 || y1 > by1) {
      by1 = y1;
    }
  }
  const Rect plateBox{{bx0, by0}, {std::max(0.f, bx1 - bx0), std::max(0.f, by1 - by0)}};
  u.thickness = ident ? 0.f : lensThicknessFor(g, plateBox);
  u.ior = g.ior;
  u.refDistance = g.refDistance;
  u.dispersion = (ident || g.flatten) ? 0.f : g.dispersion;
  u.fresnelRange = 18.f;
  u.fresnelHardness = 0.0f;
  u.fresnelIntensity = ident ? 0.f : (g.flatten ? 0.02f : (isMetal ? 0.9f : 0.04f));  // Schlick F0
  u.glareAngle = lens::kKeyLightAngle;
  u.glareRange = 14.f;
  u.glareHardness = 0.0f;
  u.glareConvergence = 0.7f;
  u.glareIntensity = ident || g.flatten ? 0.f : 0.9f;
  u.tint[0] = srgbToLinear(g.tint.red());
  u.tint[1] = srgbToLinear(g.tint.green());
  u.tint[2] = srgbToLinear(g.tint.blue());
  u.tint[3] = g.tint.alpha();
  u.blurEdge = (!isGlass || g.variant != LensVariant::Regular) ? 0.f : 1.f;
  u.lumaLift = 0.55f;
  u.lumaShadow = 0.06f;
  // A set tint is the designed wash: no luma lift on top (CPU parity).
  u.lumaOn = (!ident && !g.flatten && isGlass && g.variant == LensVariant::Regular &&
              g.tint.alpha() == 0.f)
                 ? 1.f
                 : 0.f;
  u.dimmer = (!ident && !g.flatten && isGlass && g.variant == LensVariant::Clear) ? 0.35f : 0.f;
  u.interactive = g.interactive ? 1.f : 0.f;
  u.flatten = g.flatten ? 1.f : 0.f;
  u.edge = static_cast<float>(static_cast<std::uint8_t>(g.edge));
  u.destUv0[0] = u0;
  u.destUv0[1] = v0;
  u.destUv1[0] = u1;
  u.destUv1[1] = v1;
  u.material = static_cast<float>(static_cast<int>(g.material));
  u.transmit = g.transmit;
  u.rippleAmp = g.rippleAmp;
  u.timeSec = lensRipples(g) ? g.timeSec : 0.f;
  u.swellAmp = g.swellAmp;
  u.envSoft = g.envSoft;
  u.absorbK = g.absorbK;
  u.envLight = g.envLight;
  u.swellScale[0] = g.swellScale.x;
  u.swellScale[1] = g.swellScale.y;
  u.swellPhase = lensSwellPhase(g);
  u._padS = 0.f;
  return u;
}

namespace {

void packPill(float* dst, const BackdropPill& pill) {
  dst[0] = pill.rect.origin.x;
  dst[1] = pill.rect.origin.y;
  dst[2] = pill.rect.size.x;
  dst[3] = pill.rect.size.y;
}

int bucketSize(int n) {
  constexpr int k = 32;
  return std::max(k, (n + k - 1) / k * k);
}

void ensureRt(gpu::Device& device, gpu::FrameTarget& ft, int w, int h) {
  w = std::max(1, w);
  h = std::max(1, h);
  if (!ft.native() || ft.width() != w || ft.height() != h) {
    auto created = device.createFrameTarget({w, h});
    if (created.ok()) {
      ft = std::move(created.value());
    }
  }
}

void blur1dPass(
    gpu::CommandEncoder& encoder,
    const Renderer::Pipelines& pipes,
    void* sampler,
    void* dst,
    int dw,
    int dh,
    void* src,
    float sigma,
    float dx,
    float dy,
    float su0,
    float sv0,
    float su1,
    float sv1,
    Stats* stats
) {
  gpu::PassDesc d;
  d.nativeColor = dst;
  d.load = gpu::LoadOp::Clear;
  d.clear = {0, 0, 0, 0};
  d.viewportW = dw;
  d.viewportH = dh;
  gpu::Pass p = encoder.beginPass(d);
  setProjection(p, Mat4::orthoYDown(0, 0, static_cast<float>(dw), static_cast<float>(dh)));
  Renderer::BlitInstance inst{};
  inst.rect[2] = static_cast<float>(dw);
  inst.rect[3] = static_cast<float>(dh);
  inst.uv[0] = su0;
  inst.uv[1] = sv0;
  inst.uv[2] = su1;
  inst.uv[3] = sv1;
  inst.extra[0] = sigma;
  inst.extra[1] = dx;
  inst.extra[2] = dy;
  p.setPipeline(pipes.blur1d);
  p.setBytes(0, &inst, sizeof(inst));
  p.setFragmentBytes(0, &inst, sizeof(inst));
  p.setFragmentTexture(0, src);
  p.setFragmentSampler(0, sampler);
  p.draw(6, 1, 0, 0);
  if (stats) {
    stats->draws += 1;
  }
  p.end();
}

}  // namespace

PlateInstance makePlate(
    float x,
    float y,
    float w,
    float h,
    float u0,
    float v0,
    float u1,
    float v1,
    float sigma,
    bool flat,
    const BackdropPill* pills,
    int n
) {
  PlateInstance p{};
  p.rect[0] = x;
  p.rect[1] = y;
  p.rect[2] = w;
  p.rect[3] = h;
  p.uv[0] = u0;
  p.uv[1] = v0;
  p.uv[2] = u1;
  p.uv[3] = v1;
  const int count = n > 0 ? n : 1;
  p.extra[0] = sigma * 0.5f;
  p.extra[3] = static_cast<float>(count | (flat ? 8 : 0));
  if (pills && n > 0) {
    packPill(p.pill0, pills[0]);
    p.radii[0] = pills[0].radius;
    if (n > 1) {
      packPill(p.pill1, pills[1]);
      p.radii[1] = pills[1].radius;
    }
    if (n > 2) {
      packPill(p.pill2, pills[2]);
      p.radii[2] = pills[2].radius;
    }
    if (n > 3) {
      packPill(p.pill3, pills[3]);
      p.radii[3] = pills[3].radius;
    }
  } else {
    p.pill0[2] = w;
    p.pill0[3] = h;
    p.radii[0] = 0.f;
  }
  return p;
}

gpu::FrameTarget* Renderer::TargetPool::acquire(gpu::Device& device, int w, int h) {
  w = bucketSize(std::max(1, w));
  h = bucketSize(std::max(1, h));
  for (int i = used_; i < static_cast<int>(slots_.size()); ++i) {
    if (slots_[i].w == w && slots_[i].h == h && slots_[i].ft.native()) {
      if (i != used_) {
        std::swap(slots_[i], slots_[used_]);
      }
      return &slots_[used_++].ft;
    }
  }
  if (used_ < static_cast<int>(slots_.size())) {
    ensureRt(device, slots_[used_].ft, w, h);
    slots_[used_].w = w;
    slots_[used_].h = h;
    return &slots_[used_++].ft;
  }
  Slot s;
  ensureRt(device, s.ft, w, h);
  s.w = w;
  s.h = h;
  slots_.push_back(std::move(s));
  return &slots_[used_++].ft;
}

void blitTexture(
    gpu::CommandEncoder& encoder,
    const Renderer::Pipelines& pipes,
    void* sampler,
    Stats& stats,
    void* dst,
    int dw,
    int dh,
    void* src,
    float x,
    float y,
    float w,
    float h,
    float u0,
    float v0,
    float u1,
    float v1,
    float r,
    float g,
    float b,
    float a
) {
  if (!dst || !src || dw <= 0 || dh <= 0) {
    return;
  }
  gpu::PassDesc d;
  d.nativeColor = dst;
  d.load = gpu::LoadOp::Clear;
  d.clear = {0, 0, 0, 0};
  d.viewportW = dw;
  d.viewportH = dh;
  gpu::Pass p = encoder.beginPass(d);
  setProjection(p, Mat4::orthoYDown(0, 0, static_cast<float>(dw), static_cast<float>(dh)));
  Renderer::BlitInstance blit{};
  blit.rect[0] = x;
  blit.rect[1] = y;
  blit.rect[2] = w;
  blit.rect[3] = h;
  blit.uv[0] = u0;
  blit.uv[1] = v0;
  blit.uv[2] = u1;
  blit.uv[3] = v1;
  blit.extra[0] = r;
  blit.extra[1] = g;
  blit.extra[2] = b;
  blit.extra[3] = a;
  p.setPipeline(pipes.blitLinear);
  p.setBytes(0, &blit, sizeof(blit));
  p.setFragmentTexture(0, src);
  p.setFragmentSampler(0, sampler);
  p.draw(6, 1, 0, 0);
  stats.draws += 1;
  p.end();
}

void* blurGlass(
    gpu::CommandEncoder& encoder,
    const Renderer::Pipelines& pipes,
    void* sampler,
    gpu::Device& device,
    Renderer::TargetPool& targets,
    void* sharp,
    int contentW,
    int contentH,
    float su0,
    float sv0,
    float su1,
    float sv1,
    float sigmaFull,
    Stats* stats
) {
  // Large sigmas blur at reduced res (never below 1/8); iterated H+V pairs sum variances.
  int div = 2;
  float sigmaEff = sigmaFull;
  while (sigmaEff > 4.f && div < 8) {
    div *= 2;
    sigmaEff = sigmaFull * 2.f / static_cast<float>(div);
  }
  const float ratio = sigmaEff / 4.f;
  const int n = std::max(1, static_cast<int>(std::ceil(ratio * ratio)));
  const float sigma = sigmaEff / std::sqrt(static_cast<float>(n));
  const int bw = std::max(1, contentW / div);
  const int bh = std::max(1, contentH / div);
  ensureRt(device, targets.glassBlurTmp, bw, bh);
  ensureRt(device, targets.glassBlur, bw, bh);
  if (!targets.glassBlurTmp.native() || !targets.glassBlur.native()) {
    return sharp;
  }
  // 2n directional passes of sigmaK == one wide Gaussian.
  blur1dPass(
      encoder,
      pipes,
      sampler,
      targets.glassBlurTmp.native(),
      bw,
      bh,
      sharp,
      sigma,
      0.f,
      1.f,
      su0,
      sv0,
      su1,
      sv1,
      stats
  );
  void* src = targets.glassBlurTmp.native();
  void* dst = targets.glassBlur.native();
  for (int i = 1; i < n; ++i) {
    blur1dPass(
        encoder, pipes, sampler, dst, bw, bh, src, sigma, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, stats
    );
    std::swap(src, dst);
    blur1dPass(
        encoder, pipes, sampler, dst, bw, bh, src, sigma, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, stats
    );
    std::swap(src, dst);
  }
  blur1dPass(encoder, pipes, sampler, dst, bw, bh, src, sigma, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, stats);
  return dst;
}

void encodeGlassPlate(
    gpu::CommandEncoder& encoder,
    const Renderer::Pipelines& pipes,
    void* sampler,
    gpu::Device& device,
    Renderer::TargetPool& targets,
    Stats& stats,
    Stats* drawStats,
    const Lens& mat,
    const GlassUniforms& gu,
    int pillCount,
    void* sharp,
    void* dst,
    int iw,
    int ih,
    float pixelRatio,
    const Mat4& proj,
    float u0,
    float v0,
    float u1,
    float v1
) {
  if (!sharp) {
    return;
  }
  void* blur = sharp;
  const float blurR = lensBlurRadius(mat);
  if (blurR > 0.f && pipes.blur1d.native() && !mat.flatten) {
    const float sigmaFull = blurR * pixelRatio * 0.5f;
    blur = blurGlass(
        encoder,
        pipes,
        sampler,
        device,
        targets,
        sharp,
        iw,
        ih,
        u0,
        v0,
        u1,
        v1,
        sigmaFull,
        drawStats
    );
  }
  if (pipes.glass.native() && pillCount > 0) {
    gpu::PassDesc plate;
    plate.nativeColor = dst;
    plate.load = gpu::LoadOp::Clear;
    plate.clear = {0, 0, 0, 0};
    plate.viewportW = iw;
    plate.viewportH = ih;
    gpu::Pass gp = encoder.beginPass(plate);
    setProjection(gp, proj);
    gp.setPipeline(pipes.glass);
    gp.setBytes(0, &gu, sizeof(gu));
    gp.setFragmentBytes(0, &gu, sizeof(gu));
    gp.setFragmentTexture(0, sharp);
    gp.setFragmentTexture(1, blur ? blur : sharp);
    gp.setFragmentSampler(0, sampler);
    gp.setFragmentSampler(1, sampler);
    gp.draw(6, 1, 0, 0);
    if (drawStats) {
      drawStats->draws += 1;
    }
    gp.end();
  } else {
    blitTexture(
        encoder,
        pipes,
        sampler,
        stats,
        dst,
        iw,
        ih,
        sharp,
        0.f,
        0.f,
        static_cast<float>(iw),
        static_cast<float>(ih),
        u0,
        v0,
        u1,
        v1,
        1.f,
        1.f,
        1.f,
        1.f
    );
  }
}

void* snapshotBackdrop(
    gpu::CommandEncoder& encoder,
    const Renderer::Pipelines& pipes,
    void* sampler,
    gpu::Device& device,
    Renderer::TargetPool& targets,
    int viewportW,
    int viewportH,
    void* nativeColor,
    Stats& stats
) {
  const int hw = std::max(1, viewportW / 2);
  const int hh = std::max(1, viewportH / 2);
  if (!targets.backdrop.native() || targets.backdrop.width() != hw ||
      targets.backdrop.height() != hh) {
    gpu::FrameTargetDesc d;
    d.width = hw;
    d.height = hh;
    d.mipmaps = true;
    auto ft = device.createFrameTarget(d);
    if (!ft.ok()) {
      return nullptr;
    }
    targets.backdrop = std::move(ft.value());
  }
  if (encoder.copyColorTo(targets.backdrop)) {
    encoder.generateMips(targets.backdrop);
    return targets.backdrop.native();
  }
  void* src = nativeColor ? nativeColor : device.colorNative();
  if (!src) {
    return nullptr;
  }
  gpu::PassDesc down;
  down.nativeColor = targets.backdrop.native();
  down.load = gpu::LoadOp::DontCare;
  down.viewportW = hw;
  down.viewportH = hh;
  gpu::Pass dp = encoder.beginPass(down);
  setProjection(dp, Mat4::orthoYDown(0, 0, static_cast<float>(hw), static_cast<float>(hh)));
  Renderer::BlitInstance blit{};
  blit.rect[0] = 0.f;
  blit.rect[1] = 0.f;
  blit.rect[2] = static_cast<float>(hw);
  blit.rect[3] = static_cast<float>(hh);
  blit.uv[2] = 1.f;
  blit.uv[3] = 1.f;
  blit.extra[0] = 1.f;
  blit.extra[1] = 1.f;
  blit.extra[2] = 1.f;
  blit.extra[3] = 1.f;
  dp.setPipeline(pipes.blitLinear);
  dp.setBytes(0, &blit, sizeof(blit));
  dp.setFragmentTexture(0, src);
  dp.setFragmentSampler(0, sampler);
  dp.draw(6, 1, 0, 0);
  stats.draws += 1;
  dp.end();
  encoder.generateMips(targets.backdrop);
  return targets.backdrop.native();
}

}  // namespace glim::paint
