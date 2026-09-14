#include <glim/paint/Renderer.h>

#if GLIM_SOFTWARE
#include <glim/paint/Software.h>
#else
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
    if (solidSrc.empty() || blitSrc.empty() || roundedSrc.empty()) {
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
    for (const BlitQuad& q : blits) {
        void* tex = gpuTexture(q.imageId);
        if (!tex) {
            continue;
        }
        if (pendingBlitTex_ && pendingBlitTex_ != tex) {
            flushBlit(pass);
        }
        pendingBlitTex_ = tex;
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
        pendingBlit_.push_back(inst);
    }
    flushBlit(pass);

    for (const Isolate& iso : isolates) {
        auto ft = device_.createFrameTarget({iso.contentW, iso.contentH});
        if (!ft.ok()) {
            continue;
        }
        pass.end();
        const Mat4 localProj =
            Mat4::orthoYDown(0, 0, static_cast<float>(iso.contentW), static_cast<float>(iso.contentH));
        submitLayer(encoder, iso.quads, iso.blits, iso.isolates, localProj, iso.contentW, iso.contentH,
                    ft->native(), gpu::LoadOp::Clear);

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
                           int viewportW, int viewportH, void* nativeColor, gpu::LoadOp load) {
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
        if (pendingBlitTex_ && pendingBlitTex_ != tex) {
            flushBlit(pass);
        }
        pendingBlitTex_ = tex;
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
        pendingBlit_.push_back(inst);
    };

    struct PendingIso {
        const Group* group = nullptr;
        Mat4 extra = Mat4::identity();
    };
    std::vector<PendingIso> pendingIso;

    const auto emitTree = [&](auto& self, const Group& g, const Mat4& extra,
                             const ClipState& parentClip) -> void {
        const ClipState clip = intersectClip(parentClip, clipOf(g.params, extra));
        flushSolid(pass);
        flushRounded(pass);
        flushBlit(pass);
        scissorFor(clip);
        const auto addRounded = [this, &pass, &clip, &addQuad](const Shape& xf) {
            flushBlit(pass);
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
        for (const Shape& s : g.shapes) {
            const Shape xf = transformShape(extra, s);
            if (std::get_if<FillRect>(&xf)) {
                std::vector<Quad> quads;
                std::vector<BlitQuad> unused;
                appendShape(quads, unused, xf, clip);
                if (!quads.empty()) {
                    flushBlit(pass);
                    flushRounded(pass);
                    for (const Quad& q : quads) {
                        addQuad(q);
                    }
                }
            } else if (std::get_if<FillRounded>(&xf) || std::get_if<Stroke>(&xf)) {
                addRounded(xf);
            } else if (std::get_if<Blit>(&xf)) {
                std::vector<Quad> unused;
                std::vector<BlitQuad> blits;
                appendShape(unused, blits, xf, clip);
                for (const BlitQuad& q : blits) {
                    addBlit(q);
                }
            }
        }
        for (const auto& child : g.children) {
            if (!child) {
                continue;
            }
            if (needsIsolate(*child)) {
                pendingIso.push_back({child.get(), extra});
                continue;
            }
            self(self, *child, extra * child->params.transform, clip);
        }
        flushSolid(pass);
        flushRounded(pass);
        flushBlit(pass);
        scissorFor(parentClip);
    };
    emitTree(emitTree, group, Mat4::identity(), {});

    for (const PendingIso& item : pendingIso) {
        const Group* child = item.group;
        Rect b = child->params.bounds.size.x > 0 ? child->params.bounds : contentBounds(*child);
        const int iw = std::max(1, static_cast<int>(std::ceil(b.size.x)));
        const int ih = std::max(1, static_cast<int>(std::ceil(b.size.y)));
        auto ft = device_.createFrameTarget({iw, ih});
        if (!ft.ok()) {
            continue;
        }
        pass.end();
        auto content = cloneGroup(*child);
        content->params.opacity = 1.0f;
        content->params.isolate = false;
        content->params.transform = Mat4::identity();
        if (!hasClip(content->params) && b.size.x > 0.f && b.size.y > 0.f) {
            content->params.clip = Rect{{0, 0}, b.size};
        }
        const Mat4 localProj = Mat4::orthoYDown(0, 0, static_cast<float>(iw), static_cast<float>(ih));
        encodeGroup(encoder, *content, localProj, iw, ih, ft->native(), gpu::LoadOp::Clear);
        ++stats_.isolateCount;

        passDesc.load = gpu::LoadOp::Load;
        passDesc.nativeColor = nativeColor;
        pass = encoder.beginPass(passDesc);
        Uniforms parentU{};
        std::memcpy(parentU.projection, projection.m, sizeof(parentU.projection));
        pass.setBytes(1, &parentU, sizeof(parentU));
        const Rect dest = transformRect(item.extra * child->params.transform, Rect{{0, 0}, b.size});
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
    }
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
    encodeGroup(encoder, root, proj, drawable->width(), drawable->height(), nullptr, gpu::LoadOp::Clear);
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
