#include "Isolates.h"

#include <algorithm>
#include <cmath>
#include <cstring>

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

float glassBlurRadius(const Glass& g) {
    if (g.flatten || g.variant == GlassVariant::Identity) {
        return 0.f;
    }
    return g.variant == GlassVariant::Clear ? 6.f : 20.f;
}

GlassUniforms makeGlassUniforms(const Glass& g, const GlassPill* pills, int n, int iw, int ih,
                                float pixelRatio, float u0, float v0, float u1, float v1) {
    GlassUniforms u{};
    u.resolution[0] = static_cast<float>(iw);
    u.resolution[1] = static_cast<float>(ih);
    u.dpr = pixelRatio > 0.f ? pixelRatio : 1.f;
    const int count = std::max(0, std::min(kMaxGlassPills, n));
    u.shapeCount = static_cast<float>(count);
    for (int i = 0; i < count; ++i) {
        const GlassPill& p = (pills && n > 0) ? pills[i] : GlassPill{};
        Rect r = p.rect;
        u.shapes[i].center[0] = r.origin.x + r.size.x * 0.5f;
        u.shapes[i].center[1] = r.origin.y + r.size.y * 0.5f;
        u.shapes[i].halfExtent[0] = r.size.x * 0.5f;
        u.shapes[i].halfExtent[1] = r.size.y * 0.5f;
        u.shapes[i].corner = p.radius;
        u.shapes[i].n = p.superellipseN > 0.f ? p.superellipseN : 4.f;
        u.shapes[i].mergeK = g.mergeKPx;
    }
    const bool ident = g.variant == GlassVariant::Identity;
    u.thickness = ident ? 0.f : g.thicknessPx;
    u.ior = g.ior;
    u.refDistance = g.refDistance;
    u.dispersion = (ident || g.flatten) ? 0.f : g.dispersion;
    u.fresnelRange = 18.f;
    u.fresnelHardness = 0.0f;
    u.fresnelIntensity = ident || g.flatten ? (g.flatten ? 0.05f : 0.f)
                                            : (g.variant == GlassVariant::Clear ? 0.16f : 0.22f);
    u.glareAngle = 0.f;
    u.glareRange = 14.f;
    u.glareHardness = 0.0f;
    u.glareConvergence = 0.7f;
    u.glareIntensity = ident || g.flatten ? 0.f : 0.9f;
    u.tint[0] = g.tint.red();
    u.tint[1] = g.tint.green();
    u.tint[2] = g.tint.blue();
    u.tint[3] = g.tint.alpha();
    u.blurEdge = g.variant == GlassVariant::Regular ? 1.f : 0.f;
    u.lumaLift = 0.55f;
    u.lumaShadow = 0.06f;
    u.lumaOn = (!ident && !g.flatten && g.variant == GlassVariant::Regular) ? 1.f : 0.f;
    u.dimmer = (!ident && !g.flatten && g.variant == GlassVariant::Clear) ? 0.12f : 0.f;
    u.interactive = g.interactive ? 1.f : 0.f;
    u.flatten = g.flatten ? 1.f : 0.f;
    u.destUv0[0] = u0;
    u.destUv0[1] = v0;
    u.destUv1[0] = u1;
    u.destUv1[1] = v1;
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

void blur1dPass(gpu::CommandEncoder& encoder, const Renderer::Pipelines& pipes, void* sampler,
                void* dst, int dw, int dh, void* src, float sigma, float dx, float dy, float su0,
                float sv0, float su1, float sv1, Stats* stats) {
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

PlateInstance makePlate(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                        float sigma, float bend, float mergeK, float press, bool flat, float lx,
                        float ly, float lz, const BackdropPill* pills, int n) {
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
    p.extra[1] = bend;
    p.extra[2] = mergeK;
    p.extra[3] = static_cast<float>(count | (flat ? 8 : 0));
    p.light[0] = lx;
    p.light[1] = ly;
    p.light[2] = lz;
    p.light[3] = press;
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

void blitTexture(gpu::CommandEncoder& encoder, const Renderer::Pipelines& pipes, void* sampler,
                 Stats& stats, void* dst, int dw, int dh, void* src, float x, float y, float w,
                 float h, float u0, float v0, float u1, float v1, float r, float g, float b,
                 float a) {
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
    p.setPipeline(pipes.blit);
    p.setBytes(0, &blit, sizeof(blit));
    p.setFragmentTexture(0, src);
    p.setFragmentSampler(0, sampler);
    p.draw(6, 1, 0, 0);
    stats.draws += 1;
    p.end();
}

void* blurGlass(gpu::CommandEncoder& encoder, const Renderer::Pipelines& pipes, void* sampler,
                gpu::Device& device, Renderer::TargetPool& targets, void* sharp, int contentW,
                int contentH, float su0, float sv0, float su1, float sv1, float sigmaFull,
                Stats* stats) {
    // Large sigmas are blurred at reduced resolution, then bilinearly
    // upsampled. Targets never go below 1/8 res so animated content
    // behind the frost can't visibly step/pulse. Remaining sigma is
    // covered by iterating H+V pairs (variances add), which matches a
    // single wide Gaussian without a 65-tap kernel.
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
    // V (+downsample), then (n-1) H+V pairs and a final H:
    // 2n directional passes of sigmaK == one wide Gaussian.
    blur1dPass(encoder, pipes, sampler, targets.glassBlurTmp.native(), bw, bh, sharp, sigma, 0.f,
               1.f, su0, sv0, su1, sv1, stats);
    void* src = targets.glassBlurTmp.native();
    void* dst = targets.glassBlur.native();
    for (int i = 1; i < n; ++i) {
        blur1dPass(encoder, pipes, sampler, dst, bw, bh, src, sigma, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f,
                   stats);
        std::swap(src, dst);
        blur1dPass(encoder, pipes, sampler, dst, bw, bh, src, sigma, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f,
                   stats);
        std::swap(src, dst);
    }
    blur1dPass(encoder, pipes, sampler, dst, bw, bh, src, sigma, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f,
               stats);
    return dst;
}

void* snapshotBackdrop(gpu::CommandEncoder& encoder, const Renderer::Pipelines& pipes,
                       void* sampler, gpu::Device& device, Renderer::TargetPool& targets,
                       int viewportW, int viewportH, void* nativeColor, Stats& stats) {
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
    dp.setPipeline(pipes.blit);
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
