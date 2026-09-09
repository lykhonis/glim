#include <glim/paint/Renderer.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

namespace glim::paint {
namespace {

struct Uniforms {
    float projection[16];
};

struct BlitInstance {
    float rect[4];
    float opacity;
    float pad[3];
};

std::string loadShader(const char* name) {
#ifdef GLIM_METAL_SHADER_DIR
    std::string path = std::string(GLIM_METAL_SHADER_DIR) + "/" + name;
    std::ifstream in(path);
    if (in) {
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
#endif
    (void)name;
    return {};
}

}  // namespace

Renderer::Renderer(gpu::Device& device) : device_(device) {}

Renderer::~Renderer() = default;

bool Renderer::ensurePipelines() {
    if (ready_) {
        return true;
    }
    const std::string solidSrc = loadShader("solid.metal");
    const std::string blitSrc = loadShader("blit.metal");
    if (solidSrc.empty() || blitSrc.empty()) {
        return false;
    }
    auto vs = device_.createShader(gpu::ShaderStage::Vertex, solidSrc.data(), solidSrc.size());
    auto fs = device_.createShader(gpu::ShaderStage::Fragment, solidSrc.data(), solidSrc.size());
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

    auto bvs = device_.createShader(gpu::ShaderStage::Vertex, blitSrc.data(), blitSrc.size());
    auto bfs = device_.createShader(gpu::ShaderStage::Fragment, blitSrc.data(), blitSrc.size());
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

    gpu::BufferDesc bd;
    bd.size = sizeof(SolidInstance) * 256;
    bd.vertex = true;
    auto buf = device_.createBuffer(bd);
    if (!buf.ok()) {
        return false;
    }
    instanceBuffer_ = std::move(buf.value());
    ready_ = true;
    return true;
}

void Renderer::flushSolid(gpu::Pass& pass) {
    if (pending_.empty()) {
        return;
    }
    const std::uint64_t bytes = sizeof(SolidInstance) * pending_.size();
    if (bytes > instanceBuffer_.size()) {
        gpu::BufferDesc bd;
        bd.size = bytes * 2;
        bd.vertex = true;
        auto buf = device_.createBuffer(bd);
        if (!buf.ok()) {
            pending_.clear();
            return;
        }
        instanceBuffer_ = std::move(buf.value());
    }
    device_.writeBuffer(instanceBuffer_, pending_.data(), bytes);
    pass.setPipeline(solid_);
    pass.setVertexBuffer(0, instanceBuffer_, 0);
    pass.draw(6, static_cast<std::uint32_t>(pending_.size()), 0, 0);
    stats_.draws += 1;
    stats_.instances += static_cast<unsigned>(pending_.size());
    pending_.clear();
}

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

    auto addFill = [this](const FillRect& f) {
        SolidInstance inst{};
        inst.rect[0] = f.rect.origin.x;
        inst.rect[1] = f.rect.origin.y;
        inst.rect[2] = f.rect.size.x;
        inst.rect[3] = f.rect.size.y;
        const Vec4 premul = f.color.premul();
        inst.color[0] = premul.x;
        inst.color[1] = premul.y;
        inst.color[2] = premul.z;
        inst.color[3] = premul.w;
        pending_.push_back(inst);
    };

    pending_.clear();
    for (const Shape& s : group.shapes) {
        if (const auto* f = std::get_if<FillRect>(&s)) {
            addFill(*f);
        }
    }
    for (const auto& child : group.children) {
        if (!child || needsIsolate(*child)) {
            continue;
        }
        for (const Shape& s : child->shapes) {
            if (const auto* f = std::get_if<FillRect>(&s)) {
                addFill(transformFill(child->params.transform, *f));
            }
        }
    }
    flushSolid(pass);

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
        passDesc.clear = {0, 0, 0, nativeColor ? 0.f : 1.f};
        pass = encoder.beginPass(passDesc);
        Uniforms parentU{};
        std::memcpy(parentU.projection, projection.m, sizeof(parentU.projection));
        pass.setBytes(1, &parentU, sizeof(parentU));
        const FillRect dest = transformFill(child->params.transform, FillRect{Rect{{0, 0}, b.size}, Color{}});
        BlitInstance blit{};
        blit.rect[0] = dest.rect.origin.x;
        blit.rect[1] = dest.rect.origin.y;
        blit.rect[2] = dest.rect.size.x;
        blit.rect[3] = dest.rect.size.y;
        blit.opacity = child->params.opacity;
        device_.writeBuffer(instanceBuffer_, &blit, sizeof(blit));
        pass.setPipeline(blit_);
        pass.setVertexBuffer(0, instanceBuffer_, 0);
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
    Group root = merge(scene.root, &stats_);
    const float pr = scene.logicalSize.x > 0
                         ? static_cast<float>(drawable->width()) / scene.logicalSize.x
                         : 1.0f;
    stats_.tileCount = static_cast<unsigned>(coarseTileCount(scene.logicalSize, pr));

    gpu::CommandEncoder encoder = device_.encoder();
    const Mat4 proj = Mat4::orthoYDown(0, 0, scene.logicalSize.x, scene.logicalSize.y);
    encodeGroup(encoder, root, proj, drawable->width(), drawable->height(), nullptr, gpu::LoadOp::Clear);
    encoder.present(drawable.value());
    encoder.submit(device_.queue());
    stats_.encodeMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace glim::paint
