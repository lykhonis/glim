#include <glim/paint/Renderer.h>

#if GLIM_SOFTWARE
#include <glim/paint/Software.h>
#else
#include <glim/assert.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
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
    return {};
}
#endif

}  // namespace

Renderer::Renderer(gpu::Device& device) : device_(device) {}

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
    if (solidSrc.empty() || blitSrc.empty() || roundedSrc.empty() || glyphSrc.empty() || blurSrc.empty()) {
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
    ready_ = true;
    return true;
}

void Renderer::flushSolid(gpu::Pass& pass) {
    if (pending_.empty()) {
        return;
    }
    pass.setPipeline(solid_);
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
                           const std::vector<BlitQuad>& blits, const std::vector<Isolate>& isolates,
                           const Mat4& projection, int viewportW, int viewportH, void* nativeColor,
                           gpu::LoadOp load) {
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

    for (const Isolate& iso : isolates) {
        auto ft = device_.createFrameTarget({iso.contentW, iso.contentH});
        if (!ft.ok()) {
            continue;
        }
        pass.end();
        gpu::LoadOp plateLoad = gpu::LoadOp::Clear;
        if (iso.backdropSigma > 0.f || iso.backdropBend > 0.f) {
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
        submitLayer(encoder, iso.quads, iso.blits, iso.isolates, localProj, iso.contentW, iso.contentH,
                    ft->native(), plateLoad);

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
        blit.uv[2] = 1.f;
        blit.uv[3] = 1.f;
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
    for (const BlitQuad& q : packet.blits) {
        gpuTexture(q.imageId);
    }
    gpu::CommandEncoder encoder = device_.encoder();
    const Mat4 proj = presentProjection(Mat4::orthoYDown(0, 0, packet.logicalSize.x, packet.logicalSize.y),
                                        device_.presentRotationDegrees());
    submitLayer(encoder, packet.quads, packet.blits, packet.isolates, proj, drawable->width(),
                drawable->height(), nullptr, gpu::LoadOp::Clear);
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
    auto addBlit = [this, &pass](const BlitQuad& q) {
        void* tex = gpuTexture(q.imageId);
        if (!tex) {
            return;
        }
        flushSolid(pass);
        flushRounded(pass);
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
    bool snapshotted = false;

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

    const auto emitTree = [&](auto& self, const Group& g, const Mat4& extra,
                             const ClipState& parentClip) -> void {
        const ClipState clip = intersectClip(parentClip, clipOf(g.params, extra));
        flushSolid(pass);
        flushRounded(pass);
        flushBlit(pass);
        flushGlyph(pass);
        scissorFor(clip);
        const auto addRounded = [this, &pass, &clip, &addQuad](const Shape& xf) {
            flushBlit(pass);
            flushGlyph(pass);
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
            appendShape(quads, unused, xf, clip);
            flushRounded(pass);
            for (const Quad& q : quads) {
                addQuad(q);
            }
        };
        const auto emitShape = [&](const Shape& s) {
            const Shape xf = transformShape(extra, s);
            if (std::get_if<FillRect>(&xf)) {
                std::vector<Quad> quads;
                std::vector<BlitQuad> unused;
                appendShape(quads, unused, xf, clip);
                if (!quads.empty()) {
                    flushBlit(pass);
                    flushGlyph(pass);
                    flushRounded(pass);
                    for (const Quad& q : quads) {
                        addQuad(q);
                    }
                }
            } else if (std::get_if<FillRounded>(&xf) || std::get_if<Stroke>(&xf)) {
                addRounded(xf);
            } else if (std::get_if<Blit>(&xf) || std::get_if<GlyphRun>(&xf)) {
                std::vector<Quad> unused;
                std::vector<BlitQuad> blits;
                appendShape(unused, blits, xf, clip);
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
            if (backdrop) {
                GLIM_ASSERT(!afterBackdrop,
                            "backdrop Groups must be consecutive ([under*][backdrop*][overlay*])");
                seenBackdrop = true;
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
                const Rect surface = backdrop ? backdropSurface(*child)
                                              : (child->params.bounds.size.x > 0 ? child->params.bounds
                                                                                : contentBounds(*child));
                const int iw = isolatePixelSize(surface.size.x, pixelRatio);
                const int ih = isolatePixelSize(surface.size.y, pixelRatio);
                auto ft = device_.createFrameTarget({iw, ih});
                if (ft.ok()) {
                    auto content = cloneGroup(*child);
                    content->params.opacity = 1.0f;
                    content->params.isolate = false;
                    clearBackdropParams(content->params);
                    content->params.transform = Mat4::identity();
                    if (backdrop) {
                        dropBackdropPills(*content);
                    }
                    if (!hasClip(content->params) && surface.size.x > 0.f && surface.size.y > 0.f) {
                        content->params.clip = Rect{{0, 0}, surface.size};
                    }
                    const Mat4 localProj = Mat4::orthoYDown(0, 0, surface.size.x, surface.size.y);
                    const float localPr =
                        surface.size.x > 0.f ? static_cast<float>(iw) / surface.size.x : 1.f;
                    if (backdrop && backdropTex) {
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
                    blit.uv[2] = 1.f;
                    blit.uv[3] = 1.f;
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
    const Mat4 proj = presentProjection(Mat4::orthoYDown(0, 0, scene.logicalSize.x, scene.logicalSize.y),
                                        device_.presentRotationDegrees());
    const float pr = scene.logicalSize.x > 0.f
                         ? static_cast<float>(drawable->width()) / scene.logicalSize.x
                         : 1.f;
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
