#include <glim/paint/Renderer.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

namespace glim::paint {
namespace {

struct Instance {
    float rect[4];
    float color[4];
};

struct Uniforms {
    float projection[16];
};

constexpr char kSolidMetalFallback[] = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct Instance {
    float4 rect;
    float4 color;
};

struct Uniforms {
    float4x4 projection;
};

struct VSOut {
    float4 position [[position]];
    float4 color;
};

vertex VSOut vs_main(uint vid [[vertex_id]],
                     uint iid [[instance_id]],
                     constant Instance* instances [[buffer(0)]],
                     constant Uniforms& uniforms [[buffer(1)]]) {
    const float2 unit[6] = {
        float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0),
        float2(0.0, 1.0), float2(1.0, 0.0), float2(1.0, 1.0),
    };
    const float2 p = unit[vid];
    const Instance inst = instances[iid];
    const float2 pos = inst.rect.xy + p * inst.rect.zw;
    VSOut out;
    out.position = uniforms.projection * float4(pos, 0.0, 1.0);
    out.color = inst.color;
    return out;
}

fragment float4 fs_main(VSOut in [[stage_in]]) {
    return in.color;
}
)METAL";

std::string loadMetalSource() {
#ifdef GLIM_METAL_SHADER_DIR
    const char* path = GLIM_METAL_SHADER_DIR "/solid.metal";
    std::ifstream in(path);
    if (in) {
        std::ostringstream ss;
        ss << in.rdbuf();
        const std::string s = ss.str();
        if (!s.empty()) {
            return s;
        }
    }
#endif
    return kSolidMetalFallback;
}

}  // namespace

Renderer::Renderer(gpu::Device& device) : device_(device) {}

Renderer::~Renderer() = default;

bool Renderer::ensurePipeline() {
    if (ready_) {
        return true;
    }
    const std::string src = loadMetalSource();
    if (src.empty()) {
        return false;
    }
    auto vs = device_.createShader(gpu::ShaderStage::Vertex, src.data(), src.size());
    auto fs = device_.createShader(gpu::ShaderStage::Fragment, src.data(), src.size());
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
    pipeline_ = std::move(pipe.value());

    gpu::BufferDesc bd;
    bd.size = sizeof(Instance) * 64;
    bd.vertex = true;
    auto buf = device_.createBuffer(bd);
    if (!buf.ok()) {
        return false;
    }
    instanceBuffer_ = std::move(buf.value());
    ready_ = true;
    return true;
}

void Renderer::draw(const Scene& scene) {
    if (!ensurePipeline()) {
        return;
    }
    auto drawable = device_.nextDrawable();
    if (!drawable.ok()) {
        return;
    }

    std::vector<Instance> instances;
    instances.reserve(scene.fills.size());
    for (const FillCommand& cmd : scene.fills) {
        Instance inst{};
        inst.rect[0] = cmd.rect.origin.x;
        inst.rect[1] = cmd.rect.origin.y;
        inst.rect[2] = cmd.rect.size.x;
        inst.rect[3] = cmd.rect.size.y;
        const Vec4 premul = cmd.color.premul();
        inst.color[0] = premul.x;
        inst.color[1] = premul.y;
        inst.color[2] = premul.z;
        inst.color[3] = premul.w;
        instances.push_back(inst);
    }
    if (instances.empty()) {
        return;
    }
    const std::uint64_t bytes = sizeof(Instance) * instances.size();
    if (bytes > instanceBuffer_.size()) {
        gpu::BufferDesc bd;
        bd.size = bytes * 2;
        bd.vertex = true;
        auto buf = device_.createBuffer(bd);
        if (!buf.ok()) {
            return;
        }
        instanceBuffer_ = std::move(buf.value());
    }
    device_.writeBuffer(instanceBuffer_, instances.data(), bytes);

    Uniforms u{};
    const Mat4 proj = Mat4::orthoYDown(0, 0, scene.logicalSize.x, scene.logicalSize.y);
    std::memcpy(u.projection, proj.m, sizeof(u.projection));

    gpu::CommandEncoder encoder = device_.encoder();
    gpu::PassDesc passDesc;
    passDesc.load = gpu::LoadOp::Clear;
    passDesc.clear = {0, 0, 0, 1};
    passDesc.viewportW = drawable->width();
    passDesc.viewportH = drawable->height();
    gpu::Pass pass = encoder.beginPass(passDesc);
    pass.setPipeline(pipeline_);
    pass.setVertexBuffer(0, instanceBuffer_, 0);
    pass.setBytes(1, &u, sizeof(u));
    pass.draw(6, static_cast<std::uint32_t>(instances.size()), 0, 0);
    pass.end();
    encoder.present(drawable.value());
    encoder.submit(device_.queue());
}

}  // namespace glim::paint
