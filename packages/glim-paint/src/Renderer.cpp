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
    if (solidSrc.empty() || blitSrc.empty()) {
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
    ready_ = true;
    return true;
}

void Renderer::flushSolid(gpu::Pass& pass) {
    if (pending_.empty()) {
        return;
    }
    const std::uint64_t bytes = sizeof(SolidInstance) * pending_.size();
    pass.setPipeline(solid_);
    pass.setBytes(0, pending_.data(), bytes);
    pass.draw(6, static_cast<std::uint32_t>(pending_.size()), 0, 0);
    stats_.draws += 1;
    stats_.instances += static_cast<unsigned>(pending_.size());
    pending_.clear();
}

void Renderer::flushBlit(gpu::Pass& pass) {
    if (pendingBlit_.empty() || !pendingBlitTex_) {
        pendingBlit_.clear();
        pendingBlitTex_ = nullptr;
        return;
    }
    pass.setPipeline(blit_);
    pass.setBytes(0, pendingBlit_.data(), sizeof(BlitInstance) * pendingBlit_.size());
    pass.setFragmentTexture(0, pendingBlitTex_);
    pass.setFragmentSampler(0, device_.nativeSampler());
    pass.draw(6, static_cast<std::uint32_t>(pendingBlit_.size()), 0, 0);
    stats_.draws += 1;
    stats_.instances += static_cast<unsigned>(pendingBlit_.size());
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
    pendingBlit_.clear();
    pendingBlitTex_ = nullptr;
    auto addFill = [this](const FillRect& f) {
        SolidInstance inst{};
        inst.rect[0] = f.rect.origin.x;
        inst.rect[1] = f.rect.origin.y;
        inst.rect[2] = f.rect.size.x;
        inst.rect[3] = f.rect.size.y;
        const Vec4 premul = f.matter.color.premul();
        inst.color[0] = premul.x;
        inst.color[1] = premul.y;
        inst.color[2] = premul.z;
        inst.color[3] = premul.w;
        pending_.push_back(inst);
    };
    auto addBlit = [this, &pass](const Blit& b) {
        void* tex = gpuTexture(b.matter.imageId);
        if (!tex) {
            return;
        }
        flushSolid(pass);
        if (pendingBlitTex_ && pendingBlitTex_ != tex) {
            flushBlit(pass);
        }
        pendingBlitTex_ = tex;
        const Vec4 tint = b.matter.color.premul();
        BlitInstance inst{};
        inst.rect[0] = b.rect.origin.x;
        inst.rect[1] = b.rect.origin.y;
        inst.rect[2] = b.rect.size.x;
        inst.rect[3] = b.rect.size.y;
        inst.uv[0] = b.matter.uv.origin.x;
        inst.uv[1] = b.matter.uv.origin.y;
        inst.uv[2] = b.matter.uv.origin.x + b.matter.uv.size.x;
        inst.uv[3] = b.matter.uv.origin.y + b.matter.uv.size.y;
        inst.extra[0] = tint.x;
        inst.extra[1] = tint.y;
        inst.extra[2] = tint.z;
        inst.extra[3] = tint.w;
        pendingBlit_.push_back(inst);
    };
    auto emit = [&](const Shape& s) {
        if (const auto* f = std::get_if<FillRect>(&s)) {
            flushBlit(pass);
            addFill(*f);
        } else if (const auto* b = std::get_if<Blit>(&s)) {
            addBlit(*b);
        }
    };
    for (const Shape& s : group.shapes) {
        emit(s);
    }
    for (const auto& child : group.children) {
        if (!child || needsIsolate(*child)) {
            continue;
        }
        for (const Shape& s : child->shapes) {
            if (const auto* f = std::get_if<FillRect>(&s)) {
                emit(transformFill(child->params.transform, *f));
            } else if (const auto* b = std::get_if<Blit>(&s)) {
                emit(transformBlit(child->params.transform, *b));
            }
        }
    }
    flushSolid(pass);
    flushBlit(pass);

    for (const auto& child : group.children) {
        if (!child || !needsIsolate(*child)) {
            continue;
        }
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
        const Mat4 localProj = Mat4::orthoYDown(0, 0, static_cast<float>(iw), static_cast<float>(ih));
        encodeGroup(encoder, *content, localProj, iw, ih, ft->native(), gpu::LoadOp::Clear);
        ++stats_.isolateCount;

        passDesc.load = gpu::LoadOp::Load;
        passDesc.nativeColor = nativeColor;
        pass = encoder.beginPass(passDesc);
        Uniforms parentU{};
        std::memcpy(parentU.projection, projection.m, sizeof(parentU.projection));
        pass.setBytes(1, &parentU, sizeof(parentU));
        const Rect dest = transformRect(child->params.transform, Rect{{0, 0}, b.size});
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
