#include <glim/paint/Renderer.h>

#if GLIM_SOFTWARE
#include <glim/paint/Software.h>
#else
#include <glim/assert.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>
#if !GLIM_GPU_VULKAN
#include <fstream>
#include <sstream>
#endif

#if GLIM_GPU_VULKAN
#include "glim_vulkan_shaders.h"
#else
#include "glim_metal_shaders.h"
#endif
#include "Strip.h"
#endif

namespace glim::paint {
#if !GLIM_SOFTWARE
namespace {

struct Uniforms {
    float projection[16];
};

struct PlateInstance {
    float rect[4];
    float uv[4];
    float extra[4];
    float light[4];
    float pill0[4];
    float pill1[4];
    float pill2[4];
    float pill3[4];
    float radii[4];
};

void packPill(float* dst, const BackdropPill& pill) {
    dst[0] = pill.rect.origin.x;
    dst[1] = pill.rect.origin.y;
    dst[2] = pill.rect.size.x;
    dst[3] = pill.rect.size.y;
}

struct GlassShapeU {
    float center[2];
    float halfExtent[2];
    float corner;
    float n;
    float mergeK;
    float _pad;
};

struct GlassUniforms {
    float resolution[2];
    float dpr;
    float shapeCount;
    GlassShapeU shapes[8];
    float thickness;
    float ior;
    float refDistance;
    float dispersion;
    // Reserved for future fresnel/glare model; currently unused by shaders.
    float fresnelRange;
    float fresnelHardness;
    float fresnelIntensity;
    float glareAngle;
    float glareRange;
    float glareHardness;
    float glareConvergence;
    float glareIntensity;
    float tint[4];
    float blurEdge;
    float lumaLift;
    float lumaShadow;
    float lumaOn;
    float dimmer;
    float interactive;
    float flatten;
    float _pad1;
    float destUv0[2];
    float destUv1[2];
};

static_assert(sizeof(GlassUniforms) == 384, "GlassUniforms packed std430");

float glassBlurRadius(const Glass& g) {
    if (g.flatten || g.variant == GlassVariant::Identity) {
        return 0.f;
    }
    return g.variant == GlassVariant::Clear ? 6.f : 20.f;
}

GlassUniforms makeGlassUniforms(const Glass& g, const GlassPill* pills, int n, int iw, int ih,
                                float pixelRatio, float u0 = 0.f, float v0 = 0.f, float u1 = 1.f,
                                float v1 = 1.f) {
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

PlateInstance makePlate(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                        float sigma, float bend, float mergeK, float press, bool flat, float lx, float ly,
                        float lz, const BackdropPill* pills, int n) {
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

#if !GLIM_GPU_VULKAN
std::string loadShader(const char* name) {
#ifdef GLIM_METAL_SHADER_DIR
    std::string path = std::string(GLIM_METAL_SHADER_DIR) + "/" + name;
    std::ifstream in(path);
    if (in) {
        std::ostringstream ss;
        ss << in.rdbuf();
        const std::string fromFile = ss.str();
        if (!fromFile.empty()) {
            return fromFile;
        }
    }
#endif
    if (std::strcmp(name, "solid.metal") == 0) {
        return glim::metal_shaders::solid;
    }
    if (std::strcmp(name, "blit.metal") == 0) {
        return glim::metal_shaders::blit;
    }
    if (std::strcmp(name, "rounded.metal") == 0) {
        return glim::metal_shaders::rounded;
    }
    if (std::strcmp(name, "glyph.metal") == 0) {
        return glim::metal_shaders::glyph;
    }
    if (std::strcmp(name, "blur.metal") == 0) {
        return glim::metal_shaders::blur;
    }
    if (std::strcmp(name, "blur1d.metal") == 0) {
        return glim::metal_shaders::blur1d;
    }
    if (std::strcmp(name, "glass.metal") == 0) {
        return glim::metal_shaders::glass;
    }
    if (std::strcmp(name, "gradient.metal") == 0) {
        return glim::metal_shaders::gradient;
    }
    return {};
}
#endif

}  // namespace

Renderer::Renderer(gpu::Device& device) : device_(device) {}

gpu::FrameTarget* Renderer::acquireScratch(int w, int h) {
    w = bucketSize(std::max(1, w));
    h = bucketSize(std::max(1, h));
    for (int i = scratchUsed_; i < static_cast<int>(scratch_.size()); ++i) {
        if (scratch_[i].w == w && scratch_[i].h == h && scratch_[i].ft.native()) {
            if (i != scratchUsed_) {
                std::swap(scratch_[i], scratch_[scratchUsed_]);
            }
            return &scratch_[scratchUsed_++].ft;
        }
    }
    if (scratchUsed_ < static_cast<int>(scratch_.size())) {
        ensureRt(device_, scratch_[scratchUsed_].ft, w, h);
        scratch_[scratchUsed_].w = w;
        scratch_[scratchUsed_].h = h;
        return &scratch_[scratchUsed_++].ft;
    }
    ScratchRt s;
    ensureRt(device_, s.ft, w, h);
    s.w = w;
    s.h = h;
    scratch_.push_back(std::move(s));
    return &scratch_[scratchUsed_++].ft;
}

Renderer::~Renderer() = default;

bool Renderer::ensurePipelines() {
    if (ready_) {
        return true;
    }
#if GLIM_GPU_VULKAN
    auto vs = device_.createShader(
        gpu::ShaderStage::Vertex,
        reinterpret_cast<const char*>(glim::vulkan_shaders::solid_vert),
        sizeof(glim::vulkan_shaders::solid_vert));
    auto fs = device_.createShader(
        gpu::ShaderStage::Fragment,
        reinterpret_cast<const char*>(glim::vulkan_shaders::solid_frag),
        sizeof(glim::vulkan_shaders::solid_frag));
#else
    const std::string solidSrc = loadShader("solid.metal");
    const std::string blitSrc = loadShader("blit.metal");
    const std::string roundedSrc = loadShader("rounded.metal");
    const std::string glyphSrc = loadShader("glyph.metal");
    const std::string blurSrc = loadShader("blur.metal");
    const std::string blur1dSrc = loadShader("blur1d.metal");
    const std::string glassSrc = loadShader("glass.metal");
    const std::string gradientSrc = loadShader("gradient.metal");
    if (solidSrc.empty() || blitSrc.empty() || roundedSrc.empty() || glyphSrc.empty() || blurSrc.empty() ||
        blur1dSrc.empty() || glassSrc.empty() || gradientSrc.empty()) {
        return false;
    }
    auto vs = device_.createShader(gpu::ShaderStage::Vertex, solidSrc.data(), solidSrc.size());
    auto fs = device_.createShader(gpu::ShaderStage::Fragment, solidSrc.data(), solidSrc.size());
#endif
    if (!vs.ok() || !fs.ok()) {
        return false;
    }
    gpu::PipelineDesc pd;
    pd.vertexShader = vs->handle();
    pd.fragmentShader = fs->handle();
    // Follow-up: per-blend pipelines. merge() now groups both-Plus, but every
    // content pipeline below is created SrcOver, so Plus still draws as
    // SrcOver. A Plus pipeline family (solid/rounded/blit/glyph) plus a blend
    // switch on flush is needed to make Plus visually correct.
    pd.blend = gpu::Blend::SrcOver;
    auto pipe = device_.createPipeline(pd);
    if (!pipe.ok()) {
        return false;
    }
    solid_ = std::move(pipe.value());

#if GLIM_GPU_VULKAN
    auto bvs = device_.createShader(
        gpu::ShaderStage::Vertex,
        reinterpret_cast<const char*>(glim::vulkan_shaders::blit_vert),
        sizeof(glim::vulkan_shaders::blit_vert));
    auto bfs = device_.createShader(
        gpu::ShaderStage::Fragment,
        reinterpret_cast<const char*>(glim::vulkan_shaders::blit_frag),
        sizeof(glim::vulkan_shaders::blit_frag));
#else
    auto bvs = device_.createShader(gpu::ShaderStage::Vertex, blitSrc.data(), blitSrc.size());
    auto bfs = device_.createShader(gpu::ShaderStage::Fragment, blitSrc.data(), blitSrc.size());
#endif
    if (!bvs.ok() || !bfs.ok()) {
        return false;
    }
    pd.vertexShader = bvs->handle();
    pd.fragmentShader = bfs->handle();
    auto blit = device_.createPipeline(pd);
    if (!blit.ok()) {
        return false;
    }
    blit_ = std::move(blit.value());

#if GLIM_GPU_VULKAN
    auto rvs = device_.createShader(
        gpu::ShaderStage::Vertex,
        reinterpret_cast<const char*>(glim::vulkan_shaders::rounded_vert),
        sizeof(glim::vulkan_shaders::rounded_vert));
    auto rfs = device_.createShader(
        gpu::ShaderStage::Fragment,
        reinterpret_cast<const char*>(glim::vulkan_shaders::rounded_frag),
        sizeof(glim::vulkan_shaders::rounded_frag));
#else
    auto rvs = device_.createShader(gpu::ShaderStage::Vertex, roundedSrc.data(), roundedSrc.size());
    auto rfs = device_.createShader(gpu::ShaderStage::Fragment, roundedSrc.data(), roundedSrc.size());
#endif
    if (rvs.ok() && rfs.ok()) {
        pd.vertexShader = rvs->handle();
        pd.fragmentShader = rfs->handle();
        auto rounded = device_.createPipeline(pd);
        if (rounded.ok()) {
            rounded_ = std::move(rounded.value());
        }
    }

#if GLIM_GPU_VULKAN
    auto gvs = device_.createShader(
        gpu::ShaderStage::Vertex,
        reinterpret_cast<const char*>(glim::vulkan_shaders::glyph_vert),
        sizeof(glim::vulkan_shaders::glyph_vert));
    auto gfs = device_.createShader(
        gpu::ShaderStage::Fragment,
        reinterpret_cast<const char*>(glim::vulkan_shaders::glyph_frag),
        sizeof(glim::vulkan_shaders::glyph_frag));
#else
    auto gvs = device_.createShader(gpu::ShaderStage::Vertex, glyphSrc.data(), glyphSrc.size());
    auto gfs = device_.createShader(gpu::ShaderStage::Fragment, glyphSrc.data(), glyphSrc.size());
#endif
    if (!gvs.ok() || !gfs.ok()) {
        return false;
    }
    pd.vertexShader = gvs->handle();
    pd.fragmentShader = gfs->handle();
    auto glyph = device_.createPipeline(pd);
    if (!glyph.ok()) {
        return false;
    }
    glyph_ = std::move(glyph.value());

#if GLIM_GPU_VULKAN
    auto blvs = device_.createShader(
        gpu::ShaderStage::Vertex,
        reinterpret_cast<const char*>(glim::vulkan_shaders::blur_vert),
        sizeof(glim::vulkan_shaders::blur_vert));
    auto blfs = device_.createShader(
        gpu::ShaderStage::Fragment,
        reinterpret_cast<const char*>(glim::vulkan_shaders::blur_frag),
        sizeof(glim::vulkan_shaders::blur_frag));
#else
    auto blvs = device_.createShader(gpu::ShaderStage::Vertex, blurSrc.data(), blurSrc.size());
    auto blfs = device_.createShader(gpu::ShaderStage::Fragment, blurSrc.data(), blurSrc.size());
#endif
    if (!blvs.ok() || !blfs.ok()) {
        return false;
    }
    pd.vertexShader = blvs->handle();
    pd.fragmentShader = blfs->handle();
    auto blur = device_.createPipeline(pd);
    if (!blur.ok()) {
        return false;
    }
    blur_ = std::move(blur.value());

#if GLIM_GPU_VULKAN
    auto b1vs = device_.createShader(
        gpu::ShaderStage::Vertex,
        reinterpret_cast<const char*>(glim::vulkan_shaders::blur1d_vert),
        sizeof(glim::vulkan_shaders::blur1d_vert));
    auto b1fs = device_.createShader(
        gpu::ShaderStage::Fragment,
        reinterpret_cast<const char*>(glim::vulkan_shaders::blur1d_frag),
        sizeof(glim::vulkan_shaders::blur1d_frag));
#else
    auto b1vs = device_.createShader(gpu::ShaderStage::Vertex, blur1dSrc.data(), blur1dSrc.size());
    auto b1fs = device_.createShader(gpu::ShaderStage::Fragment, blur1dSrc.data(), blur1dSrc.size());
#endif
    if (!b1vs.ok() || !b1fs.ok()) {
        return false;
    }
    pd.vertexShader = b1vs->handle();
    pd.fragmentShader = b1fs->handle();
    auto blur1d = device_.createPipeline(pd);
    if (!blur1d.ok()) {
        return false;
    }
    blur1d_ = std::move(blur1d.value());

#if GLIM_GPU_VULKAN
    auto glvs = device_.createShader(
        gpu::ShaderStage::Vertex,
        reinterpret_cast<const char*>(glim::vulkan_shaders::glass_vert),
        sizeof(glim::vulkan_shaders::glass_vert));
    auto glfs = device_.createShader(
        gpu::ShaderStage::Fragment,
        reinterpret_cast<const char*>(glim::vulkan_shaders::glass_frag),
        sizeof(glim::vulkan_shaders::glass_frag));
#else
    auto glvs = device_.createShader(gpu::ShaderStage::Vertex, glassSrc.data(), glassSrc.size());
    auto glfs = device_.createShader(gpu::ShaderStage::Fragment, glassSrc.data(), glassSrc.size());
#endif
    if (!glvs.ok() || !glfs.ok()) {
        return false;
    }
    pd.vertexShader = glvs->handle();
    pd.fragmentShader = glfs->handle();
    auto glass = device_.createPipeline(pd);
    if (!glass.ok()) {
        return false;
    }
    glass_ = std::move(glass.value());

#if GLIM_GPU_VULKAN
    auto grvs = device_.createShader(
        gpu::ShaderStage::Vertex,
        reinterpret_cast<const char*>(glim::vulkan_shaders::gradient_vert),
        sizeof(glim::vulkan_shaders::gradient_vert));
    auto grfs = device_.createShader(
        gpu::ShaderStage::Fragment,
        reinterpret_cast<const char*>(glim::vulkan_shaders::gradient_frag),
        sizeof(glim::vulkan_shaders::gradient_frag));
#else
    auto grvs = device_.createShader(gpu::ShaderStage::Vertex, gradientSrc.data(), gradientSrc.size());
    auto grfs = device_.createShader(gpu::ShaderStage::Fragment, gradientSrc.data(), gradientSrc.size());
#endif
    if (grvs.ok() && grfs.ok()) {
        pd.vertexShader = grvs->handle();
        pd.fragmentShader = grfs->handle();
        auto gradient = device_.createPipeline(pd);
        if (gradient.ok()) {
            gradient_ = std::move(gradient.value());
        }
    }
    ready_ = true;
    return true;
}

void Renderer::flushSolid(gpu::Pass& pass) {
    if (pending_.empty()) {
        return;
    }
    pass.setPipeline(solid_);
    // 4 KB chunks: setBytes-style uploads stay within the fast path on both
    // backends (larger chunks risk heap-alloc fallback on Metal and dynamic
    // UBO alignment trouble on Vulkan). Raise only with device validation.
    constexpr std::size_t kMax = 4096 / sizeof(SolidInstance);
    std::size_t i = 0;
    while (i < pending_.size()) {
        const std::size_t n = std::min(kMax, pending_.size() - i);
        pass.setBytes(0, pending_.data() + i, sizeof(SolidInstance) * n);
        pass.draw(6, static_cast<std::uint32_t>(n), 0, 0);
        stats_.draws += 1;
        stats_.instances += static_cast<unsigned>(n);
        i += n;
    }
    pending_.clear();
}

void Renderer::flushRounded(gpu::Pass& pass) {
    if (pendingRounded_.empty()) {
        return;
    }
    if (!rounded_.native()) {
        pendingRounded_.clear();
        return;
    }
    pass.setPipeline(rounded_);
    constexpr std::size_t kMax = 4096 / sizeof(RoundedInstance);
    std::size_t i = 0;
    while (i < pendingRounded_.size()) {
        const std::size_t n = std::min(kMax, pendingRounded_.size() - i);
        const std::uint64_t bytes = sizeof(RoundedInstance) * n;
        pass.setBytes(0, pendingRounded_.data() + i, bytes);
        pass.setFragmentBytes(0, pendingRounded_.data() + i, bytes);
        pass.draw(6, static_cast<std::uint32_t>(n), 0, 0);
        stats_.draws += 1;
        stats_.instances += static_cast<unsigned>(n);
        i += n;
    }
    pendingRounded_.clear();
}

void Renderer::flushGradient(gpu::Pass& pass) {
    if (pendingGradient_.empty()) {
        return;
    }
    if (!gradient_.native()) {
        pendingGradient_.clear();
        return;
    }
    pass.setPipeline(gradient_);
    constexpr std::size_t kMax = 4096 / sizeof(GradientInstance);
    std::size_t i = 0;
    while (i < pendingGradient_.size()) {
        const std::size_t n = std::min(kMax, pendingGradient_.size() - i);
        const std::uint64_t bytes = sizeof(GradientInstance) * n;
        pass.setBytes(0, pendingGradient_.data() + i, bytes);
        pass.setFragmentBytes(0, pendingGradient_.data() + i, bytes);
        pass.draw(6, static_cast<std::uint32_t>(n), 0, 0);
        stats_.draws += 1;
        stats_.instances += static_cast<unsigned>(n);
        i += n;
    }
    pendingGradient_.clear();
}

void Renderer::flushBlit(gpu::Pass& pass) {
    if (pendingBlit_.empty() || !pendingBlitTex_) {
        pendingBlit_.clear();
        pendingBlitTex_ = nullptr;
        return;
    }
    pass.setPipeline(blit_);
    pass.setFragmentTexture(0, pendingBlitTex_);
    pass.setFragmentSampler(0, device_.nativeSampler());
    constexpr std::size_t kMax = 4096 / sizeof(BlitInstance);
    std::size_t i = 0;
    while (i < pendingBlit_.size()) {
        const std::size_t n = std::min(kMax, pendingBlit_.size() - i);
        pass.setBytes(0, pendingBlit_.data() + i, sizeof(BlitInstance) * n);
        pass.draw(6, static_cast<std::uint32_t>(n), 0, 0);
        stats_.draws += 1;
        stats_.instances += static_cast<unsigned>(n);
        i += n;
    }
    pendingBlit_.clear();
    pendingBlitTex_ = nullptr;
}

void Renderer::flushGlyph(gpu::Pass& pass) {
    if (pendingGlyph_.empty() || !pendingGlyphTex_) {
        pendingGlyph_.clear();
        pendingGlyphTex_ = nullptr;
        return;
    }
    pass.setPipeline(glyph_);
    pass.setFragmentTexture(0, pendingGlyphTex_);
    pass.setFragmentSampler(0, device_.nativeSampler());
    constexpr std::size_t kMax = 4096 / sizeof(BlitInstance);
    std::size_t i = 0;
    while (i < pendingGlyph_.size()) {
        const std::size_t n = std::min(kMax, pendingGlyph_.size() - i);
        pass.setBytes(0, pendingGlyph_.data() + i, sizeof(BlitInstance) * n);
        pass.draw(6, static_cast<std::uint32_t>(n), 0, 0);
        stats_.draws += 1;
        stats_.instances += static_cast<unsigned>(n);
        i += n;
    }
    pendingGlyph_.clear();
    pendingGlyphTex_ = nullptr;
}

void* Renderer::gpuTexture(std::uint32_t imageId) {
    if (!images_ || imageId == 0) {
        return nullptr;
    }
    if (imageId >= gpuImages_.size()) {
        gpuImages_.resize(imageId + 1);
    }
    gpu::Texture& tex = gpuImages_[imageId];
    if (tex.native()) {
        return tex.native();
    }
    const StoredImage* img = images_->get(imageId);
    if (!img) {
        return nullptr;
    }
    if (img->native) {
        return img->native;
    }
    if (img->rgba.empty()) {
        return nullptr;
    }
    auto created = device_.createTexture({img->width, img->height});
    if (!created.ok()) {
        return nullptr;
    }
    tex = std::move(created.value());
    device_.writeTexture(tex, img->rgba.data(), img->rgba.size());
    return tex.native();
}

#if GLIM_EMBED

void Renderer::submitLayer(gpu::CommandEncoder& encoder, const std::vector<Quad>& quads,
                           const std::vector<BlitQuad>& blits, const std::vector<GradientQuad>& gradients,
                           const std::vector<Isolate>& isolates, const Mat4& projection, int viewportW,
                           int viewportH, void* nativeColor, gpu::LoadOp load) {
    gpu::PassDesc passDesc;
    passDesc.nativeColor = nativeColor;
    passDesc.load = load;
    passDesc.clear = {0, 0, 0, nativeColor ? 0.f : 1.f};
    passDesc.viewportW = viewportW;
    passDesc.viewportH = viewportH;
    gpu::Pass pass = encoder.beginPass(passDesc);
    Uniforms u{};
    std::memcpy(u.projection, projection.m, sizeof(u.projection));
    pass.setBytes(1, &u, sizeof(u));

    pending_.clear();
    for (const Quad& q : quads) {
        SolidInstance inst{};
        inst.rect[0] = q.x;
        inst.rect[1] = q.y;
        inst.rect[2] = q.w;
        inst.rect[3] = q.h;
        inst.color[0] = q.r;
        inst.color[1] = q.g;
        inst.color[2] = q.b;
        inst.color[3] = q.a;
        pending_.push_back(inst);
    }
    flushSolid(pass);

    pendingGradient_.clear();
    for (const GradientQuad& q : gradients) {
        if (q.w <= 0.f || q.h <= 0.f || q.coverage <= 1e-4f || q.stopCount == 0) {
            continue;
        }
        GradientInstance inst{};
        inst.rect[0] = q.x;
        inst.rect[1] = q.y;
        inst.rect[2] = q.w;
        inst.rect[3] = q.h;
        inst.grad[0] = q.p0x;
        inst.grad[1] = q.p0y;
        inst.grad[2] = q.p1x;
        inst.grad[3] = q.p1y;
        inst.misc[0] = static_cast<float>(q.kind);
        inst.misc[1] = q.radius;
        inst.misc[2] = q.coverage;
        inst.misc[3] = static_cast<float>(q.stopCount);
        for (int i = 0; i < q.stopCount && i < kMaxGradientStops; ++i) {
            inst.colors[i * 4 + 0] = q.r[i];
            inst.colors[i * 4 + 1] = q.g[i];
            inst.colors[i * 4 + 2] = q.b[i];
            inst.colors[i * 4 + 3] = q.a[i];
            inst.offsets[i] = q.offsets[i];
        }
        pendingGradient_.push_back(inst);
    }
    flushGradient(pass);

    pendingBlit_.clear();
    pendingBlitTex_ = nullptr;
    pendingGlyph_.clear();
    pendingGlyphTex_ = nullptr;
    auto pushSampled = [this, &pass](const BlitQuad& q) {
        void* tex = gpuTexture(q.imageId);
        if (!tex) {
            return;
        }
        BlitInstance inst{};
        inst.rect[0] = q.x;
        inst.rect[1] = q.y;
        inst.rect[2] = q.w;
        inst.rect[3] = q.h;
        inst.uv[0] = q.u0;
        inst.uv[1] = q.v0;
        inst.uv[2] = q.u1;
        inst.uv[3] = q.v1;
        inst.extra[0] = q.r;
        inst.extra[1] = q.g;
        inst.extra[2] = q.b;
        inst.extra[3] = q.a;
        if (q.sdf) {
            flushBlit(pass);
            if (pendingGlyphTex_ && pendingGlyphTex_ != tex) {
                flushGlyph(pass);
            }
            pendingGlyphTex_ = tex;
            pendingGlyph_.push_back(inst);
            return;
        }
        flushGlyph(pass);
        if (pendingBlitTex_ && pendingBlitTex_ != tex) {
            flushBlit(pass);
        }
        pendingBlitTex_ = tex;
        pendingBlit_.push_back(inst);
    };
    for (const BlitQuad& q : blits) {
        pushSampled(q);
    }
    flushBlit(pass);
    flushGlyph(pass);

    bool glassFrozen = false;
    for (const Isolate& iso : isolates) {
        gpu::FrameTarget* ft = acquireScratch(iso.contentW, iso.contentH);
        if (!ft || !ft->native()) {
            continue;
        }
        const float isoU1 =
            static_cast<float>(iso.contentW) / static_cast<float>(std::max(1, ft->width()));
        const float isoV1 =
            static_cast<float>(iso.contentH) / static_cast<float>(std::max(1, ft->height()));
        pass.end();
        gpu::LoadOp plateLoad = gpu::LoadOp::Clear;
        const auto blitTex = [&](void* dst, int dw, int dh, void* src, float u0, float v0, float u1,
                                 float v1, float r, float g, float b, float a) {
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
            Uniforms uu{};
            const Mat4 proj = Mat4::orthoYDown(0, 0, static_cast<float>(dw), static_cast<float>(dh));
            std::memcpy(uu.projection, proj.m, sizeof(uu.projection));
            p.setBytes(1, &uu, sizeof(uu));
            BlitInstance blit{};
            blit.rect[2] = static_cast<float>(dw);
            blit.rect[3] = static_cast<float>(dh);
            blit.uv[0] = u0;
            blit.uv[1] = v0;
            blit.uv[2] = u1;
            blit.uv[3] = v1;
            blit.extra[0] = r;
            blit.extra[1] = g;
            blit.extra[2] = b;
            blit.extra[3] = a;
            p.setPipeline(blit_);
            p.setBytes(0, &blit, sizeof(blit));
            p.setFragmentTexture(0, src);
            p.setFragmentSampler(0, device_.nativeSampler());
            p.draw(6, 1, 0, 0);
            stats_.draws += 1;
            p.end();
        };
        if (iso.hasGlass) {
            const int hw = std::max(1, viewportW / 2);
            const int hh = std::max(1, viewportH / 2);
            if (!glassFrozen) {
                if (!glassSrc_.native() || glassSrc_.width() != hw || glassSrc_.height() != hh) {
                    auto bg = device_.createFrameTarget({hw, hh});
                    if (bg.ok()) {
                        glassSrc_ = std::move(bg.value());
                    }
                }
                void* src = nativeColor ? nativeColor : device_.colorNative();
                if (glassSrc_.native() && src) {
                    blitTex(glassSrc_.native(), hw, hh, src, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f);
                    glassFrozen = true;
                }
            }
            if (glassSrc_.native()) {
                    const GlassUniforms gu = makeGlassUniforms(
                        iso.glass, iso.glassPills.empty() ? nullptr : iso.glassPills.data(),
                        static_cast<int>(iso.glassPills.size()), iso.contentW, iso.contentH,
                        iso.destW > 0.f ? static_cast<float>(iso.contentW) / iso.destW : 1.f,
                        iso.glassU0, iso.glassV0, iso.glassU1, iso.glassV1);
                    void* sharp = glassSrc_.native();
                    void* blur = sharp;
                    const float blurR = glassBlurRadius(iso.glass);
                    if (blurR > 0.f && blur1d_.native() && !iso.glass.flatten) {
                        // Large sigmas are blurred at reduced resolution, then bilinearly
                        // upsampled. Targets never go below 1/8 res so animated content
                        // behind the frost can't visibly step/pulse. Remaining sigma is
                        // covered by iterating H+V pairs (variances add), which matches a
                        // single wide Gaussian without a 65-tap kernel.
                        const float pr =
                            iso.destW > 0.f ? static_cast<float>(iso.contentW) / iso.destW : 1.f;
                        const float sigmaFull = blurR * pr * 0.5f;
                        int div = 2;
                        float sigmaEff = sigmaFull;
                        while (sigmaEff > 4.f && div < 8) {
                            div *= 2;
                            sigmaEff = sigmaFull * 2.f / static_cast<float>(div);
                        }
                        const float ratio = sigmaEff / 4.f;
                        const int n = std::max(1, static_cast<int>(std::ceil(ratio * ratio)));
                        const float sigma = sigmaEff / std::sqrt(static_cast<float>(n));
                        const int bw = std::max(1, iso.contentW / div);
                        const int bh = std::max(1, iso.contentH / div);
                        ensureRt(device_, glassBlurTmp_, bw, bh);
                        ensureRt(device_, glassBlur_, bw, bh);
                        auto pass1d = [&](void* dst, int dw, int dh, void* src, float dx, float dy,
                                          float su0, float sv0, float su1, float sv1) {
                            gpu::PassDesc d;
                            d.nativeColor = dst;
                            d.load = gpu::LoadOp::Clear;
                            d.clear = {0, 0, 0, 0};
                            d.viewportW = dw;
                            d.viewportH = dh;
                            gpu::Pass p = encoder.beginPass(d);
                            Uniforms uu{};
                            const Mat4 proj =
                                Mat4::orthoYDown(0, 0, static_cast<float>(dw), static_cast<float>(dh));
                            std::memcpy(uu.projection, proj.m, sizeof(uu.projection));
                            p.setBytes(1, &uu, sizeof(uu));
                            BlitInstance inst{};
                            inst.rect[2] = static_cast<float>(dw);
                            inst.rect[3] = static_cast<float>(dh);
                            inst.uv[0] = su0;
                            inst.uv[1] = sv0;
                            inst.uv[2] = su1;
                            inst.uv[3] = sv1;
                            inst.extra[0] = sigma;
                            inst.extra[1] = dx;
                            inst.extra[2] = dy;
                            p.setPipeline(blur1d_);
                            p.setBytes(0, &inst, sizeof(inst));
                            p.setFragmentBytes(0, &inst, sizeof(inst));
                            p.setFragmentTexture(0, src);
                            p.setFragmentSampler(0, device_.nativeSampler());
                            p.draw(6, 1, 0, 0);
                            p.end();
                        };
                        if (glassBlurTmp_.native() && glassBlur_.native()) {
                            // V (+downsample), then (n-1) H+V pairs and a final H:
                            // 2n directional passes of sigmaK == one wide Gaussian.
                            pass1d(glassBlurTmp_.native(), bw, bh, sharp, 0.f, 1.f, iso.glassU0,
                                   iso.glassV0, iso.glassU1, iso.glassV1);
                            void* src = glassBlurTmp_.native();
                            void* dst = glassBlur_.native();
                            for (int i = 1; i < n; ++i) {
                                pass1d(dst, bw, bh, src, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f);
                                void* t = src;
                                src = dst;
                                dst = t;
                                pass1d(dst, bw, bh, src, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f);
                                t = src;
                                src = dst;
                                dst = t;
                            }
                            pass1d(dst, bw, bh, src, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f);
                            blur = dst;
                        }
                    }
                    if (glass_.native() && !iso.glassPills.empty()) {
                        gpu::PassDesc plate;
                        plate.nativeColor = ft->native();
                        plate.load = gpu::LoadOp::Clear;
                        plate.clear = {0, 0, 0, 0};
                        plate.viewportW = iso.contentW;
                        plate.viewportH = iso.contentH;
                        gpu::Pass gp = encoder.beginPass(plate);
                        Uniforms uu{};
                        const Mat4 localProj = Mat4::orthoYDown(
                            0, 0, iso.destW > 0.f ? iso.destW : static_cast<float>(iso.contentW),
                            iso.destH > 0.f ? iso.destH : static_cast<float>(iso.contentH));
                        std::memcpy(uu.projection, localProj.m, sizeof(uu.projection));
                        gp.setBytes(1, &uu, sizeof(uu));
                        gp.setPipeline(glass_);
                        gp.setBytes(0, &gu, sizeof(gu));
                        gp.setFragmentBytes(0, &gu, sizeof(gu));
                        gp.setFragmentTexture(0, sharp);
                        gp.setFragmentTexture(1, blur);
                        gp.setFragmentSampler(0, device_.nativeSampler());
                        gp.setFragmentSampler(1, device_.nativeSampler());
                        gp.draw(6, 1, 0, 0);
                        gp.end();
                    } else {
                        blitTex(ft->native(), iso.contentW, iso.contentH, sharp, iso.glassU0,
                                iso.glassV0, iso.glassU1, iso.glassV1, 1.f, 1.f, 1.f, 1.f);
                    }
                    plateLoad = gpu::LoadOp::Load;
            }
            ++stats_.glassPassCount;
            stats_.glassPillCount += static_cast<unsigned>(iso.glassPills.size());
        } else if (iso.backdropSigma > 0.f || iso.backdropBend > 0.f) {
            const int hw = std::max(1, viewportW / 2);
            const int hh = std::max(1, viewportH / 2);
            if (!backdrop_.native() || backdrop_.width() != hw || backdrop_.height() != hh) {
                gpu::FrameTargetDesc bgDesc;
                bgDesc.width = hw;
                bgDesc.height = hh;
                bgDesc.mipmaps = true;
                auto bg = device_.createFrameTarget(bgDesc);
                if (bg.ok()) {
                    backdrop_ = std::move(bg.value());
                }
            }
            if (backdrop_.native()) {
                if (!encoder.copyColorTo(backdrop_)) {
                    void* src = nativeColor ? nativeColor : device_.colorNative();
                    if (src) {
                        gpu::PassDesc down;
                        down.nativeColor = backdrop_.native();
                        down.load = gpu::LoadOp::DontCare;
                        down.viewportW = hw;
                        down.viewportH = hh;
                        gpu::Pass dp = encoder.beginPass(down);
                        Uniforms du{};
                        const Mat4 dproj =
                            Mat4::orthoYDown(0, 0, static_cast<float>(hw), static_cast<float>(hh));
                        std::memcpy(du.projection, dproj.m, sizeof(du.projection));
                        dp.setBytes(1, &du, sizeof(du));
                        BlitInstance blit{};
                        blit.rect[0] = 0.f;
                        blit.rect[1] = 0.f;
                        blit.rect[2] = static_cast<float>(hw);
                        blit.rect[3] = static_cast<float>(hh);
                        blit.uv[2] = 1.f;
                        blit.uv[3] = 1.f;
                        blit.extra[0] = blit.extra[1] = blit.extra[2] = blit.extra[3] = 1.f;
                        dp.setPipeline(blit_);
                        dp.setBytes(0, &blit, sizeof(blit));
                        dp.setFragmentTexture(0, src);
                        dp.setFragmentSampler(0, device_.nativeSampler());
                        dp.draw(6, 1, 0, 0);
                        dp.end();
                    }
                }
                encoder.generateMips(backdrop_);
                gpu::PassDesc plate;
                plate.nativeColor = ft->native();
                plate.load = gpu::LoadOp::Clear;
                plate.clear = {0, 0, 0, 0};
                plate.viewportW = iso.contentW;
                plate.viewportH = iso.contentH;
                gpu::Pass gp = encoder.beginPass(plate);
                Uniforms gu{};
                const Mat4 localProj =
                    Mat4::orthoYDown(0, 0, iso.destW > 0.f ? iso.destW : static_cast<float>(iso.contentW),
                                     iso.destH > 0.f ? iso.destH : static_cast<float>(iso.contentH));
                std::memcpy(gu.projection, localProj.m, sizeof(gu.projection));
                gp.setBytes(1, &gu, sizeof(gu));
                const float pw = iso.destW > 0.f ? iso.destW : static_cast<float>(iso.contentW);
                const float ph = iso.destH > 0.f ? iso.destH : static_cast<float>(iso.contentH);
                const PlateInstance back = makePlate(
                    0.f, 0.f, pw, ph, iso.backdropU0, iso.backdropV0, iso.backdropU1, iso.backdropV1,
                    iso.backdropSigma, iso.backdropBend, iso.backdropMerge, iso.backdropPress,
                    iso.backdropFlat, iso.backdropLightX, iso.backdropLightY, iso.backdropLightZ,
                    iso.backdropPills.empty() ? nullptr : iso.backdropPills.data(),
                    static_cast<int>(iso.backdropPills.size()));
                gp.setPipeline(blur_.native() ? blur_ : blit_);
                gp.setBytes(0, &back, sizeof(back));
                gp.setFragmentBytes(0, &back, sizeof(back));
                gp.setFragmentTexture(0, backdrop_.native());
                gp.setFragmentSampler(0, device_.nativeSampler());
                gp.draw(6, 1, 0, 0);
                gp.end();
                plateLoad = gpu::LoadOp::Load;
            }
        }
        const Mat4 localProj =
            Mat4::orthoYDown(0, 0, iso.destW > 0.f ? iso.destW : static_cast<float>(iso.contentW),
                             iso.destH > 0.f ? iso.destH : static_cast<float>(iso.contentH));
        submitLayer(encoder, iso.quads, iso.blits, iso.gradients, iso.isolates, localProj,
                    iso.contentW, iso.contentH, ft->native(), plateLoad);

        passDesc.load = gpu::LoadOp::Load;
        passDesc.nativeColor = nativeColor;
        pass = encoder.beginPass(passDesc);
        Uniforms parentU{};
        std::memcpy(parentU.projection, projection.m, sizeof(parentU.projection));
        pass.setBytes(1, &parentU, sizeof(parentU));
        BlitInstance blit{};
        blit.rect[0] = iso.destX;
        blit.rect[1] = iso.destY;
        blit.rect[2] = iso.destW;
        blit.rect[3] = iso.destH;
        blit.uv[0] = 0.f;
        blit.uv[1] = 0.f;
        blit.uv[2] = isoU1;
        blit.uv[3] = isoV1;
        blit.extra[0] = iso.opacity;
        blit.extra[1] = iso.opacity;
        blit.extra[2] = iso.opacity;
        blit.extra[3] = iso.opacity;
        pass.setPipeline(blit_);
        pass.setBytes(0, &blit, sizeof(blit));
        pass.setFragmentTexture(0, ft->native());
        pass.setFragmentSampler(0, device_.nativeSampler());
        pass.draw(6, 1, 0, 0);
        stats_.draws += 1;
        stats_.instances += 1;
    }
    pass.end();
}

void Renderer::submit(const FramePacket& packet) {
    const auto t0 = std::chrono::steady_clock::now();
    stats_ = packet.stats;
    if (!ensurePipelines()) {
        return;
    }
    auto drawable = device_.nextDrawable();
    if (!drawable.ok()) {
        return;
    }
    images_ = &packet.images;
    scratchUsed_ = 0;
    for (const BlitQuad& q : packet.blits) {
        gpuTexture(q.imageId);
    }
    gpu::CommandEncoder encoder = device_.encoder();
    const Mat4 proj = presentProjection(Mat4::orthoYDown(0, 0, packet.logicalSize.x, packet.logicalSize.y),
                                        device_.presentRotationDegrees());
    submitLayer(encoder, packet.quads, packet.blits, packet.gradients, packet.isolates, proj,
                drawable->width(), drawable->height(), nullptr, gpu::LoadOp::Clear);
    encoder.present(drawable.value());
    encoder.submit(device_.queue());
    stats_.encodeMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

#else

void Renderer::encodeGroup(gpu::CommandEncoder& encoder, const Group& group, const Mat4& projection,
                           int viewportW, int viewportH, void* nativeColor, gpu::LoadOp load,
                           float pixelRatio, Vec2 logicalSize, const Mat4& extraRoot) {
    gpu::PassDesc passDesc;
    passDesc.nativeColor = nativeColor;
    passDesc.load = load;
    passDesc.clear = {0, 0, 0, nativeColor ? 0.f : 1.f};
    passDesc.viewportW = viewportW;
    passDesc.viewportH = viewportH;
    gpu::Pass pass = encoder.beginPass(passDesc);
    Uniforms u{};
    std::memcpy(u.projection, projection.m, sizeof(u.projection));
    pass.setBytes(1, &u, sizeof(u));

    pending_.clear();
    pendingRounded_.clear();
    pendingGradient_.clear();
    pendingBlit_.clear();
    pendingBlitTex_ = nullptr;
    pendingGlyph_.clear();
    pendingGlyphTex_ = nullptr;

    const auto scissorFor = [&](const ClipState& clip) {
        const int vw = std::max(0, viewportW);
        const int vh = std::max(0, viewportH);
        if (!clip.active || vw <= 0 || vh <= 0 || clip.rect.size.x <= 0.f || clip.rect.size.y <= 0.f) {
            pass.setScissor(0, 0, static_cast<std::uint32_t>(vw), static_cast<std::uint32_t>(vh));
            return;
        }
        const float x0 = clip.rect.origin.x;
        const float y0 = clip.rect.origin.y;
        const float x1 = x0 + clip.rect.size.x;
        const float y1 = y0 + clip.rect.size.y;
        const Vec2 corners[4] = {{x0, y0}, {x1, y0}, {x0, y1}, {x1, y1}};
        float minX = 0.f;
        float minY = 0.f;
        float maxX = 0.f;
        float maxY = 0.f;
        for (int i = 0; i < 4; ++i) {
            const Vec4 clipP = projection * Vec4{corners[i].x, corners[i].y, 0.f, 1.f};
            const float iw = clipP.w != 0.f ? 1.f / clipP.w : 1.f;
            const float fx = (clipP.x * iw * 0.5f + 0.5f) * static_cast<float>(vw);
            const float fy = (0.5f - clipP.y * iw * 0.5f) * static_cast<float>(vh);
            if (i == 0) {
                minX = maxX = fx;
                minY = maxY = fy;
            } else {
                minX = std::min(minX, fx);
                minY = std::min(minY, fy);
                maxX = std::max(maxX, fx);
                maxY = std::max(maxY, fy);
            }
        }
        int x = static_cast<int>(std::floor(minX));
        int y = static_cast<int>(std::floor(minY));
        int w = static_cast<int>(std::ceil(maxX)) - x;
        int h = static_cast<int>(std::ceil(maxY)) - y;
        x = std::max(0, std::min(x, vw));
        y = std::max(0, std::min(y, vh));
        w = std::max(0, std::min(w, vw - x));
        h = std::max(0, std::min(h, vh - y));
        pass.setScissor(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y),
                        static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h));
    };

    auto addQuad = [this](const Quad& q) {
        SolidInstance inst{};
        inst.rect[0] = q.x;
        inst.rect[1] = q.y;
        inst.rect[2] = q.w;
        inst.rect[3] = q.h;
        inst.color[0] = q.r;
        inst.color[1] = q.g;
        inst.color[2] = q.b;
        inst.color[3] = q.a;
        pending_.push_back(inst);
    };
    auto addGradient = [this, &pass](const GradientQuad& q) {
        if (q.w <= 0.f || q.h <= 0.f || q.coverage <= 1e-4f || q.stopCount == 0) {
            return;
        }
        flushSolid(pass);
        flushRounded(pass);
        flushBlit(pass);
        flushGlyph(pass);
        GradientInstance inst{};
        inst.rect[0] = q.x;
        inst.rect[1] = q.y;
        inst.rect[2] = q.w;
        inst.rect[3] = q.h;
        inst.grad[0] = q.p0x;
        inst.grad[1] = q.p0y;
        inst.grad[2] = q.p1x;
        inst.grad[3] = q.p1y;
        inst.misc[0] = static_cast<float>(q.kind);
        inst.misc[1] = q.radius;
        inst.misc[2] = q.coverage;
        inst.misc[3] = static_cast<float>(q.stopCount);
        for (int i = 0; i < q.stopCount && i < kMaxGradientStops; ++i) {
            inst.colors[i * 4 + 0] = q.r[i];
            inst.colors[i * 4 + 1] = q.g[i];
            inst.colors[i * 4 + 2] = q.b[i];
            inst.colors[i * 4 + 3] = q.a[i];
            inst.offsets[i] = q.offsets[i];
        }
        pendingGradient_.push_back(inst);
    };
    auto addBlit = [this, &pass](const BlitQuad& q) {
        void* tex = gpuTexture(q.imageId);
        if (!tex) {
            return;
        }
        flushSolid(pass);
        flushRounded(pass);
        flushGradient(pass);
        BlitInstance inst{};
        inst.rect[0] = q.x;
        inst.rect[1] = q.y;
        inst.rect[2] = q.w;
        inst.rect[3] = q.h;
        inst.uv[0] = q.u0;
        inst.uv[1] = q.v0;
        inst.uv[2] = q.u1;
        inst.uv[3] = q.v1;
        inst.extra[0] = q.r;
        inst.extra[1] = q.g;
        inst.extra[2] = q.b;
        inst.extra[3] = q.a;
        if (q.sdf) {
            flushBlit(pass);
            if (pendingGlyphTex_ && pendingGlyphTex_ != tex) {
                flushGlyph(pass);
            }
            pendingGlyphTex_ = tex;
            pendingGlyph_.push_back(inst);
            return;
        }
        flushGlyph(pass);
        if (pendingBlitTex_ && pendingBlitTex_ != tex) {
            flushBlit(pass);
        }
        pendingBlitTex_ = tex;
        pendingBlit_.push_back(inst);
    };

    bool seenBackdrop = false;
    bool afterBackdrop = false;
    bool seenGlass = false;
    bool afterGlass = false;
    bool snapshotted = false;
    bool glassFrozen = false;

    const auto resumePass = [&]() {
        passDesc.load = gpu::LoadOp::Load;
        passDesc.nativeColor = nativeColor;
        pass = encoder.beginPass(passDesc);
        Uniforms parentU{};
        std::memcpy(parentU.projection, projection.m, sizeof(parentU.projection));
        pass.setBytes(1, &parentU, sizeof(parentU));
    };

    const auto snapshotBackdrop = [&]() -> void* {
        const int hw = std::max(1, viewportW / 2);
        const int hh = std::max(1, viewportH / 2);
        if (!backdrop_.native() || backdrop_.width() != hw || backdrop_.height() != hh) {
            gpu::FrameTargetDesc d;
            d.width = hw;
            d.height = hh;
            d.mipmaps = true;
            auto ft = device_.createFrameTarget(d);
            if (!ft.ok()) {
                return nullptr;
            }
            backdrop_ = std::move(ft.value());
        }
        if (encoder.copyColorTo(backdrop_)) {
            encoder.generateMips(backdrop_);
            return backdrop_.native();
        }
        void* src = nativeColor ? nativeColor : device_.colorNative();
        if (!src) {
            return nullptr;
        }
        gpu::PassDesc down;
        down.nativeColor = backdrop_.native();
        down.load = gpu::LoadOp::DontCare;
        down.viewportW = hw;
        down.viewportH = hh;
        gpu::Pass dp = encoder.beginPass(down);
        Uniforms du{};
        const Mat4 dproj = Mat4::orthoYDown(0, 0, static_cast<float>(hw), static_cast<float>(hh));
        std::memcpy(du.projection, dproj.m, sizeof(du.projection));
        dp.setBytes(1, &du, sizeof(du));
        BlitInstance blit{};
        blit.rect[0] = 0.f;
        blit.rect[1] = 0.f;
        blit.rect[2] = static_cast<float>(hw);
        blit.rect[3] = static_cast<float>(hh);
        blit.uv[0] = 0.f;
        blit.uv[1] = 0.f;
        blit.uv[2] = 1.f;
        blit.uv[3] = 1.f;
        blit.extra[0] = 1.f;
        blit.extra[1] = 1.f;
        blit.extra[2] = 1.f;
        blit.extra[3] = 1.f;
        dp.setPipeline(blit_);
        dp.setBytes(0, &blit, sizeof(blit));
        dp.setFragmentTexture(0, src);
        dp.setFragmentSampler(0, device_.nativeSampler());
        dp.draw(6, 1, 0, 0);
        stats_.draws += 1;
        dp.end();
        encoder.generateMips(backdrop_);
        return backdrop_.native();
    };

    const auto blitTex = [&](void* dst, int dw, int dh, void* src, float x, float y, float w, float h,
                             float u0, float v0, float u1, float v1, float r, float g, float b, float a) {
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
        Uniforms uu{};
        const Mat4 proj = Mat4::orthoYDown(0, 0, static_cast<float>(dw), static_cast<float>(dh));
        std::memcpy(uu.projection, proj.m, sizeof(uu.projection));
        p.setBytes(1, &uu, sizeof(uu));
        BlitInstance blit{};
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
        p.setPipeline(blit_);
        p.setBytes(0, &blit, sizeof(blit));
        p.setFragmentTexture(0, src);
        p.setFragmentSampler(0, device_.nativeSampler());
        p.draw(6, 1, 0, 0);
        stats_.draws += 1;
        p.end();
    };

    const auto freezeGlassSrc = [&]() -> void* {
        const int hw = std::max(1, viewportW / 2);
        const int hh = std::max(1, viewportH / 2);
        if (!glassSrc_.native() || glassSrc_.width() != hw || glassSrc_.height() != hh) {
            auto ft = device_.createFrameTarget({hw, hh});
            if (!ft.ok()) {
                return nullptr;
            }
            glassSrc_ = std::move(ft.value());
        }
        if (!nativeColor && encoder.copyColorTo(glassSrc_)) {
            return glassSrc_.native();
        }
        void* src = nativeColor ? nativeColor : device_.colorNative();
        if (!src) {
            return nullptr;
        }
        blitTex(glassSrc_.native(), hw, hh, src, 0.f, 0.f, static_cast<float>(hw), static_cast<float>(hh), 0.f,
                0.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f);
        return glassSrc_.native();
    };

    const auto emitTree = [&](auto& self, const Group& g, const Mat4& extra,
                             const ClipState& parentClip) -> void {
        const ClipState clip = intersectClip(parentClip, clipOf(g.params, extra));
        flushSolid(pass);
        flushRounded(pass);
        flushGradient(pass);
        flushBlit(pass);
        flushGlyph(pass);
        scissorFor(clip);
        const auto addRounded = [this, &pass, &clip, &addQuad, &addGradient](const Shape& xf) {
            flushBlit(pass);
            flushGlyph(pass);
            flushGradient(pass);
            if (rounded_.native()) {
                RoundedInstance inst{};
                const Rect* rect = nullptr;
                const Radius* radius = nullptr;
                const Matter* matter = nullptr;
                float strokeWidth = 0.f;
                if (const auto* r = std::get_if<FillRounded>(&xf)) {
                    rect = &r->rect;
                    radius = &r->radius;
                    matter = &r->matter;
                } else if (const auto* st = std::get_if<Stroke>(&xf)) {
                    rect = &st->rect;
                    radius = &st->radius;
                    matter = &st->matter;
                    strokeWidth = st->width;
                }
                if (!rect || !radius || !matter) {
                    return;
                }
                if (matter->isGradient()) {
                    // Gradient rounded/stroke always rasterizes via strips.
                    std::vector<Quad> unused;
                    std::vector<BlitQuad> unusedBlits;
                    std::vector<GradientQuad> grads;
                    appendShape(unused, unusedBlits, grads, xf, clip);
                    for (const GradientQuad& g : grads) {
                        addGradient(g);
                    }
                    return;
                }
                inst.rect[0] = rect->origin.x;
                inst.rect[1] = rect->origin.y;
                inst.rect[2] = rect->size.x;
                inst.rect[3] = rect->size.y;
                inst.radii[0] = radius->lt;
                inst.radii[1] = radius->rt;
                inst.radii[2] = radius->lb;
                inst.radii[3] = radius->rb;
                const Vec4 premul = matter->color.premul();
                inst.color[0] = premul.x;
                inst.color[1] = premul.y;
                inst.color[2] = premul.z;
                inst.color[3] = premul.w;
                inst.extra[0] = strokeWidth;
                pendingRounded_.push_back(inst);
                return;
            }
            std::vector<Quad> quads;
            std::vector<BlitQuad> unused;
            std::vector<GradientQuad> unusedGrads;
            appendShape(quads, unused, unusedGrads, xf, clip);
            flushRounded(pass);
            flushGradient(pass);
            for (const Quad& q : quads) {
                addQuad(q);
            }
        };
        const auto emitShape = [&](const Shape& s) {
            const Shape xf = transformShape(extra, s);
            if (const auto* fill = std::get_if<FillRect>(&xf)) {
                flushBlit(pass);
                flushGlyph(pass);
                flushRounded(pass);
                flushGradient(pass);
                if (fill->matter.isGradient()) {
                    std::vector<Quad> unused;
                    std::vector<BlitQuad> unusedBlits;
                    std::vector<GradientQuad> grads;
                    appendShape(unused, unusedBlits, grads, xf, clip);
                    for (const GradientQuad& g : grads) {
                        addGradient(g);
                    }
                    return;
                }
                if (!clip.active) {
                    const Vec4 p = fill->matter.color.premul();
                    Quad q;
                    q.x = fill->rect.origin.x;
                    q.y = fill->rect.origin.y;
                    q.w = fill->rect.size.x;
                    q.h = fill->rect.size.y;
                    q.r = p.x;
                    q.g = p.y;
                    q.b = p.z;
                    q.a = p.w;
                    addQuad(q);
                } else {
                    std::vector<Quad> quads;
                    std::vector<BlitQuad> unused;
                    std::vector<GradientQuad> unusedGrads;
                    appendShape(quads, unused, unusedGrads, xf, clip);
                    for (const Quad& q : quads) {
                        addQuad(q);
                    }
                }
            } else if (std::get_if<FillRounded>(&xf) || std::get_if<Stroke>(&xf)) {
                addRounded(xf);
            } else if (std::get_if<Blit>(&xf) || std::get_if<GlyphRun>(&xf)) {
                std::vector<Quad> unused;
                std::vector<BlitQuad> blits;
                std::vector<GradientQuad> unusedGrads;
                appendShape(unused, blits, unusedGrads, xf, clip);
                for (const BlitQuad& q : blits) {
                    addBlit(q);
                }
            }
        };
        visitGroup(
            g, emitShape,
            [&](const Group& childRef) {
            const Group* child = &childRef;
            const bool backdrop = hasBackdrop(*child);
            const bool glassWork = hasGlassWork(*child);
            if (backdrop) {
                GLIM_ASSERT(!afterBackdrop && !seenGlass,
                            "backdrop Groups must be consecutive ([under*][backdrop*][glass*][overlay*])");
                seenBackdrop = true;
            } else if (glassWork) {
                GLIM_ASSERT(!afterGlass,
                            "glass Groups must be consecutive ([under*][backdrop*][glass*][overlay*])");
                if (seenBackdrop) {
                    afterBackdrop = true;
                }
                seenGlass = true;
            } else if (seenGlass) {
                afterGlass = true;
            } else if (seenBackdrop) {
                afterBackdrop = true;
            }
            if (needsIsolate(*child)) {
                flushSolid(pass);
                flushRounded(pass);
                flushBlit(pass);
                flushGlyph(pass);
                pass.end();
                void* backdropTex = nullptr;
                if (backdrop) {
                    if (!snapshotted) {
                        backdropTex = snapshotBackdrop();
                        snapshotted = true;
                        stats_.backdropCount = 1;
                    } else {
                        backdropTex = backdrop_.native();
                    }
                }
                void* glassTex = nullptr;
                if (glassWork) {
                    if (!glassFrozen) {
                        if (freezeGlassSrc()) {
                            glassFrozen = true;
                        }
                    }
                    ++stats_.glassPassCount;
                }
                GLIM_ASSERT(!(backdrop && glassWork), "a Group cannot be both backdrop and glass");
                const Rect surface = glassWork ? glassSurface(*child)
                                               : (backdrop ? backdropSurface(*child)
                                                          : (child->params.bounds.size.x > 0.f &&
                                                                     child->params.bounds.size.y > 0.f
                                                                 ? child->params.bounds
                                                                 : contentBounds(*child)));
                const int iw = isolatePixelSize(surface.size.x, pixelRatio);
                const int ih = isolatePixelSize(surface.size.y, pixelRatio);
                gpu::FrameTarget* ft = acquireScratch(iw, ih);
                if (ft && ft->native()) {
                    const float isoU1 =
                        static_cast<float>(iw) / static_cast<float>(std::max(1, ft->width()));
                    const float isoV1 =
                        static_cast<float>(ih) / static_cast<float>(std::max(1, ft->height()));
                    auto content = cloneGroup(*child);
                    content->params.opacity = 1.0f;
                    content->params.isolate = false;
                    clearBackdropParams(content->params);
                    content->params.transform = Mat4::identity();
                    if (backdrop) {
                        dropBackdropPills(*content);
                    }
                    if (glassWork) {
                        GlassPill pills[kMaxGlassPills];
                        const int n = collectGlassPills(*child, pills);
                        stats_.glassPillCount += static_cast<unsigned>(n);
                        stripGlassForIsolate(*content);
                    }
                    if (!hasClip(content->params) && surface.size.x > 0.f && surface.size.y > 0.f) {
                        content->params.clip = Rect{{0, 0}, surface.size};
                    }
                    const Mat4 localProj = Mat4::orthoYDown(0, 0, surface.size.x, surface.size.y);
                    // Width-based: isolate content is uniformly scaled by pr
                    // (iw = ceil(w * pr)), so pr == iw/w up to rounding.
                    const float localPr =
                        surface.size.x > 0.f ? static_cast<float>(iw) / surface.size.x : 1.f;
                    if (glassWork) {
                        const Rect dest = transformRect(extra * child->params.transform, surface);
                        float du0 = 0.f;
                        float dv0 = 0.f;
                        float du1 = 1.f;
                        float dv1 = 1.f;
                        if (logicalSize.x > 0.f && logicalSize.y > 0.f) {
                            du0 = dest.origin.x / logicalSize.x;
                            dv0 = dest.origin.y / logicalSize.y;
                            du1 = (dest.origin.x + dest.size.x) / logicalSize.x;
                            dv1 = (dest.origin.y + dest.size.y) / logicalSize.y;
                        }
                        glassTex = glassSrc_.native();
                        Glass mat{};
                        if (child->params.glass.has_value()) {
                            mat = *child->params.glass;
                        } else {
                            for (const auto& gc : child->children) {
                                if (gc && gc->params.glass.has_value()) {
                                    mat = *gc->params.glass;
                                    break;
                                }
                            }
                        }
                        GlassPill gpills[kMaxGlassPills];
                        int gn = collectGlassPills(*child, gpills);
                        for (int i = 0; i < gn; ++i) {
                            gpills[i].rect.origin.x -= surface.origin.x;
                            gpills[i].rect.origin.y -= surface.origin.y;
                        }
                        const GlassUniforms gu =
                            makeGlassUniforms(mat, gpills, gn, iw, ih, localPr, du0, dv0, du1, dv1);
                        void* sharpTex = glassTex;
                        void* blurTex = glassTex;
                        const float blurR = glassBlurRadius(mat);
                        if (sharpTex && blurR > 0.f && blur1d_.native() && !mat.flatten) {
                            // Same adaptive scheme as the isolate path above: never below
                            // 1/8 res, iterate H+V pairs for the remaining sigma.
                            const float sigmaFull = blurR * localPr * 0.5f;
                            int div = 2;
                            float sigmaEff = sigmaFull;
                            while (sigmaEff > 4.f && div < 8) {
                                div *= 2;
                                sigmaEff = sigmaFull * 2.f / static_cast<float>(div);
                            }
                            const float ratio = sigmaEff / 4.f;
                            const int n = std::max(1, static_cast<int>(std::ceil(ratio * ratio)));
                            const float sigma = sigmaEff / std::sqrt(static_cast<float>(n));
                            const int bw = std::max(1, iw / div);
                            const int bh = std::max(1, ih / div);
                            ensureRt(device_, glassBlurTmp_, bw, bh);
                            ensureRt(device_, glassBlur_, bw, bh);
                            auto blur1dPass = [&](void* dst, int dw, int dh, void* src, float dx, float dy,
                                                  float su0, float sv0, float su1, float sv1) {
                                gpu::PassDesc d;
                                d.nativeColor = dst;
                                d.load = gpu::LoadOp::Clear;
                                d.clear = {0, 0, 0, 0};
                                d.viewportW = dw;
                                d.viewportH = dh;
                                gpu::Pass p = encoder.beginPass(d);
                                Uniforms uu{};
                                const Mat4 proj =
                                    Mat4::orthoYDown(0, 0, static_cast<float>(dw), static_cast<float>(dh));
                                std::memcpy(uu.projection, proj.m, sizeof(uu.projection));
                                p.setBytes(1, &uu, sizeof(uu));
                                BlitInstance inst{};
                                inst.rect[2] = static_cast<float>(dw);
                                inst.rect[3] = static_cast<float>(dh);
                                inst.uv[0] = su0;
                                inst.uv[1] = sv0;
                                inst.uv[2] = su1;
                                inst.uv[3] = sv1;
                                inst.extra[0] = sigma;
                                inst.extra[1] = dx;
                                inst.extra[2] = dy;
                                inst.extra[3] = 0.f;
                                p.setPipeline(blur1d_);
                                p.setBytes(0, &inst, sizeof(inst));
                                p.setFragmentBytes(0, &inst, sizeof(inst));
                                p.setFragmentTexture(0, src);
                                p.setFragmentSampler(0, device_.nativeSampler());
                                p.draw(6, 1, 0, 0);
                                stats_.draws += 1;
                                p.end();
                            };
                            if (glassBlurTmp_.native() && glassBlur_.native()) {
                                blur1dPass(glassBlurTmp_.native(), bw, bh, sharpTex, 0.f, 1.f, du0, dv0,
                                           du1, dv1);
                                void* src = glassBlurTmp_.native();
                                void* dst = glassBlur_.native();
                                for (int i = 1; i < n; ++i) {
                                    blur1dPass(dst, bw, bh, src, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f);
                                    void* t = src;
                                    src = dst;
                                    dst = t;
                                    blur1dPass(dst, bw, bh, src, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f);
                                    t = src;
                                    src = dst;
                                    dst = t;
                                }
                                blur1dPass(dst, bw, bh, src, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f);
                                blurTex = dst;
                            }
                        }
                        if (sharpTex && glass_.native() && gn > 0) {
                            gpu::PassDesc plate;
                            plate.nativeColor = ft->native();
                            plate.load = gpu::LoadOp::Clear;
                            plate.clear = {0, 0, 0, 0};
                            plate.viewportW = iw;
                            plate.viewportH = ih;
                            gpu::Pass gp = encoder.beginPass(plate);
                            Uniforms uu{};
                            std::memcpy(uu.projection, localProj.m, sizeof(uu.projection));
                            gp.setBytes(1, &uu, sizeof(uu));
                            gp.setPipeline(glass_);
                            gp.setBytes(0, &gu, sizeof(gu));
                            gp.setFragmentBytes(0, &gu, sizeof(gu));
                            gp.setFragmentTexture(0, sharpTex);
                            gp.setFragmentTexture(1, blurTex ? blurTex : sharpTex);
                            gp.setFragmentSampler(0, device_.nativeSampler());
                            gp.setFragmentSampler(1, device_.nativeSampler());
                            gp.draw(6, 1, 0, 0);
                            stats_.draws += 1;
                            gp.end();
                            glassTex = ft->native();
                        } else if (glassTex) {
                            blitTex(ft->native(), iw, ih, glassTex, 0.f, 0.f, static_cast<float>(iw),
                                    static_cast<float>(ih), du0, dv0, du1, dv1, 1.f, 1.f, 1.f, 1.f);
                        }
                        encodeGroup(encoder, *content, localProj, iw, ih, ft->native(),
                                    glassTex ? gpu::LoadOp::Load : gpu::LoadOp::Clear, localPr,
                                    surface.size, Mat4::translate(-surface.origin.x, -surface.origin.y));
                    } else if (backdrop && backdropTex) {
                        gpu::PassDesc plate;
                        plate.nativeColor = ft->native();
                        plate.load = gpu::LoadOp::Clear;
                        plate.clear = {0, 0, 0, 0};
                        plate.viewportW = iw;
                        plate.viewportH = ih;
                        gpu::Pass gp = encoder.beginPass(plate);
                        Uniforms gu{};
                        std::memcpy(gu.projection, localProj.m, sizeof(gu.projection));
                        gp.setBytes(1, &gu, sizeof(gu));
                        const Rect dest = transformRect(extra * child->params.transform, surface);
                        float u0 = 0.f;
                        float v0 = 0.f;
                        float u1 = 1.f;
                        float v1 = 1.f;
                        if (logicalSize.x > 0.f && logicalSize.y > 0.f) {
                            u0 = dest.origin.x / logicalSize.x;
                            v0 = dest.origin.y / logicalSize.y;
                            u1 = (dest.origin.x + dest.size.x) / logicalSize.x;
                            v1 = (dest.origin.y + dest.size.y) / logicalSize.y;
                        }
                        BackdropPill pills[kMaxBackdropPills];
                        const int n = collectBackdropPills(*child, pills);
                        for (int i = 0; i < n; ++i) {
                            pills[i].rect.origin.x -= surface.origin.x;
                            pills[i].rect.origin.y -= surface.origin.y;
                        }
                        const float sigma = snapBackdropSigma(child->params.backdropBlur);
                        const PlateInstance back = makePlate(
                            0.f, 0.f, surface.size.x, surface.size.y, u0, v0, u1, v1, sigma,
                            child->params.backdropBend, child->params.backdropMerge,
                            child->params.backdropPress, child->params.backdropFlat,
                            child->params.backdropLightX, child->params.backdropLightY,
                            child->params.backdropLightZ, pills, n);
                        gp.setPipeline(blur_.native() ? blur_ : blit_);
                        gp.setBytes(0, &back, sizeof(back));
                        gp.setFragmentBytes(0, &back, sizeof(back));
                        gp.setFragmentTexture(0, backdropTex);
                        gp.setFragmentSampler(0, device_.nativeSampler());
                        gp.draw(6, 1, 0, 0);
                        stats_.draws += 1;
                        gp.end();
                        encodeGroup(encoder, *content, localProj, iw, ih, ft->native(), gpu::LoadOp::Load,
                                    localPr, surface.size,
                                    Mat4::translate(-surface.origin.x, -surface.origin.y));
                    } else {
                        encodeGroup(encoder, *content, localProj, iw, ih, ft->native(), gpu::LoadOp::Clear,
                                    localPr, surface.size,
                                    Mat4::translate(-surface.origin.x, -surface.origin.y));
                    }
                    ++stats_.isolateCount;
                    resumePass();
                    const Rect dest = transformRect(extra * child->params.transform, surface);
                    BlitInstance blit{};
                    blit.rect[0] = dest.origin.x;
                    blit.rect[1] = dest.origin.y;
                    blit.rect[2] = dest.size.x;
                    blit.rect[3] = dest.size.y;
                    blit.uv[0] = 0.f;
                    blit.uv[1] = 0.f;
                    blit.uv[2] = isoU1;
                    blit.uv[3] = isoV1;
                    blit.extra[0] = child->params.opacity;
                    blit.extra[1] = child->params.opacity;
                    blit.extra[2] = child->params.opacity;
                    blit.extra[3] = child->params.opacity;
                    pass.setPipeline(blit_);
                    pass.setBytes(0, &blit, sizeof(blit));
                    pass.setFragmentTexture(0, ft->native());
                    pass.setFragmentSampler(0, device_.nativeSampler());
                    pass.draw(6, 1, 0, 0);
                    stats_.draws += 1;
                    stats_.instances += 1;
                } else {
                    resumePass();
                }
                return;
            }
            self(self, *child, extra * child->params.transform, clip);
            });
        flushSolid(pass);
        flushRounded(pass);
        flushGradient(pass);
        flushBlit(pass);
        flushGlyph(pass);
        scissorFor(parentClip);
    };
    emitTree(emitTree, group, extraRoot, {});
    pass.end();
}

void Renderer::draw(const Scene& scene) {
    const auto t0 = std::chrono::steady_clock::now();
    stats_ = Stats{};
    if (!ensurePipelines()) {
        return;
    }
    auto drawable = device_.nextDrawable();
    if (!drawable.ok()) {
        return;
    }
    images_ = &scene.images;
    scratchUsed_ = 0;
    Group root = merge(scene.root, &stats_);
    const auto prefetch = [this](auto& self, const Group& g) -> void {
        for (const Shape& s : g.shapes) {
            if (const auto* b = std::get_if<Blit>(&s)) {
                gpuTexture(b->matter.imageId);
            }
        }
        for (const auto& child : g.children) {
            if (child) {
                self(self, *child);
            }
        }
    };
    prefetch(prefetch, root);
    gpu::CommandEncoder encoder = device_.encoder();
    const int rotation = device_.presentRotationDegrees();
    const bool swapped = (rotation == 90 || rotation == 270);
    const float logicalW = swapped ? scene.logicalSize.y : scene.logicalSize.x;
    const float logicalH = swapped ? scene.logicalSize.x : scene.logicalSize.y;
    // Width and height ratios agree when the drawable aspect matches logical.
    // On mismatch (letterbox/stretch) take the larger so isolates never
    // undersample an axis.
    const float prW =
        logicalW > 0.f ? static_cast<float>(drawable->width()) / logicalW : 1.f;
    const float prH =
        logicalH > 0.f ? static_cast<float>(drawable->height()) / logicalH : 1.f;
    const float pr = std::max(prW > 0.f ? prW : 1.f, prH > 0.f ? prH : 1.f);
    const Mat4 proj = presentProjection(Mat4::orthoYDown(0, 0, scene.logicalSize.x, scene.logicalSize.y),
                                        rotation);
    encodeGroup(encoder, root, proj, drawable->width(), drawable->height(), nullptr, gpu::LoadOp::Clear,
                pr, scene.logicalSize);
    encoder.present(drawable.value());
    encoder.submit(device_.queue());
    stats_.encodeMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

#endif
#else

Renderer::Renderer(std::uint8_t* rgba, int width, int height)
    : rgba_(rgba), width_(width), height_(height) {}

void Renderer::setTarget(std::uint8_t* rgba, int width, int height) {
    rgba_ = rgba;
    width_ = width;
    height_ = height;
}

Renderer::~Renderer() = default;

#if GLIM_EMBED

void Renderer::submit(const FramePacket& packet) {
    stats_ = packet.stats;
    if (!rgba_ || width_ <= 0 || height_ <= 0) {
        return;
    }
    rasterPacket(packet, width_, height_, rgba_);
}

#else

void Renderer::draw(const Scene& scene) {
    stats_ = Stats{};
    if (!rgba_ || width_ <= 0 || height_ <= 0) {
        return;
    }
    rasterScene(scene, width_, height_, rgba_);
}

#endif
#endif
}  // namespace glim::paint
