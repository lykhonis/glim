#include <glim/paint/Renderer.h>

#include <cstring>
#include <string>

#if GLIM_GPU_VULKAN
#include "glim_vulkan_shaders.h"
#elif GLIM_GPU_WEBGPU
#include "glim_webgpu_shaders.h"
#else
#include <fstream>
#include <sstream>

#include "glim_metal_shaders.h"
#endif

namespace glim::paint {

#if !GLIM_GPU_VULKAN && !GLIM_GPU_WEBGPU
namespace {

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

}  // namespace
#endif

namespace {

#if GLIM_GPU_WEBGPU
const char* webgpuSource(const char* name) {
    if (std::strcmp(name, "solid") == 0) {
        return glim::webgpu_shaders::solid;
    }
    if (std::strcmp(name, "blit") == 0) {
        return glim::webgpu_shaders::blit;
    }
    if (std::strcmp(name, "rounded") == 0) {
        return glim::webgpu_shaders::rounded;
    }
    if (std::strcmp(name, "glyph") == 0) {
        return glim::webgpu_shaders::glyph;
    }
    if (std::strcmp(name, "blur") == 0) {
        return glim::webgpu_shaders::blur;
    }
    if (std::strcmp(name, "blur1d") == 0) {
        return glim::webgpu_shaders::blur1d;
    }
    if (std::strcmp(name, "glass") == 0) {
        return glim::webgpu_shaders::glass;
    }
    return glim::webgpu_shaders::gradient;
}
#endif

auto makeVertex(gpu::Device& device, const char* name) {
#if GLIM_GPU_VULKAN
    if (std::strcmp(name, "solid") == 0) {
        return device.createShader(gpu::ShaderStage::Vertex,
                                   reinterpret_cast<const char*>(glim::vulkan_shaders::solid_vert),
                                   sizeof(glim::vulkan_shaders::solid_vert));
    }
    if (std::strcmp(name, "blit") == 0) {
        return device.createShader(gpu::ShaderStage::Vertex,
                                   reinterpret_cast<const char*>(glim::vulkan_shaders::blit_vert),
                                   sizeof(glim::vulkan_shaders::blit_vert));
    }
    if (std::strcmp(name, "rounded") == 0) {
        return device.createShader(gpu::ShaderStage::Vertex,
                                   reinterpret_cast<const char*>(glim::vulkan_shaders::rounded_vert),
                                   sizeof(glim::vulkan_shaders::rounded_vert));
    }
    if (std::strcmp(name, "glyph") == 0) {
        return device.createShader(gpu::ShaderStage::Vertex,
                                   reinterpret_cast<const char*>(glim::vulkan_shaders::glyph_vert),
                                   sizeof(glim::vulkan_shaders::glyph_vert));
    }
    if (std::strcmp(name, "blur") == 0) {
        return device.createShader(gpu::ShaderStage::Vertex,
                                   reinterpret_cast<const char*>(glim::vulkan_shaders::blur_vert),
                                   sizeof(glim::vulkan_shaders::blur_vert));
    }
    if (std::strcmp(name, "blur1d") == 0) {
        return device.createShader(gpu::ShaderStage::Vertex,
                                   reinterpret_cast<const char*>(glim::vulkan_shaders::blur1d_vert),
                                   sizeof(glim::vulkan_shaders::blur1d_vert));
    }
    if (std::strcmp(name, "glass") == 0) {
        return device.createShader(gpu::ShaderStage::Vertex,
                                   reinterpret_cast<const char*>(glim::vulkan_shaders::glass_vert),
                                   sizeof(glim::vulkan_shaders::glass_vert));
    }
    return device.createShader(gpu::ShaderStage::Vertex,
                               reinterpret_cast<const char*>(glim::vulkan_shaders::gradient_vert),
                               sizeof(glim::vulkan_shaders::gradient_vert));
#elif GLIM_GPU_WEBGPU
    const char* src = webgpuSource(name);
    return device.createShader(gpu::ShaderStage::Vertex, src, std::strlen(src));
#else
    const std::string src = loadShader((std::string(name) + ".metal").c_str());
    return device.createShader(gpu::ShaderStage::Vertex, src.data(), src.size());
#endif
}

auto makeFragment(gpu::Device& device, const char* name) {
#if GLIM_GPU_VULKAN
    if (std::strcmp(name, "solid") == 0) {
        return device.createShader(gpu::ShaderStage::Fragment,
                                   reinterpret_cast<const char*>(glim::vulkan_shaders::solid_frag),
                                   sizeof(glim::vulkan_shaders::solid_frag));
    }
    if (std::strcmp(name, "blit") == 0) {
        return device.createShader(gpu::ShaderStage::Fragment,
                                   reinterpret_cast<const char*>(glim::vulkan_shaders::blit_frag),
                                   sizeof(glim::vulkan_shaders::blit_frag));
    }
    if (std::strcmp(name, "rounded") == 0) {
        return device.createShader(gpu::ShaderStage::Fragment,
                                   reinterpret_cast<const char*>(glim::vulkan_shaders::rounded_frag),
                                   sizeof(glim::vulkan_shaders::rounded_frag));
    }
    if (std::strcmp(name, "glyph") == 0) {
        return device.createShader(gpu::ShaderStage::Fragment,
                                   reinterpret_cast<const char*>(glim::vulkan_shaders::glyph_frag),
                                   sizeof(glim::vulkan_shaders::glyph_frag));
    }
    if (std::strcmp(name, "blur") == 0) {
        return device.createShader(gpu::ShaderStage::Fragment,
                                   reinterpret_cast<const char*>(glim::vulkan_shaders::blur_frag),
                                   sizeof(glim::vulkan_shaders::blur_frag));
    }
    if (std::strcmp(name, "blur1d") == 0) {
        return device.createShader(gpu::ShaderStage::Fragment,
                                   reinterpret_cast<const char*>(glim::vulkan_shaders::blur1d_frag),
                                   sizeof(glim::vulkan_shaders::blur1d_frag));
    }
    if (std::strcmp(name, "glass") == 0) {
        return device.createShader(gpu::ShaderStage::Fragment,
                                   reinterpret_cast<const char*>(glim::vulkan_shaders::glass_frag),
                                   sizeof(glim::vulkan_shaders::glass_frag));
    }
    return device.createShader(gpu::ShaderStage::Fragment,
                               reinterpret_cast<const char*>(glim::vulkan_shaders::gradient_frag),
                               sizeof(glim::vulkan_shaders::gradient_frag));
#elif GLIM_GPU_WEBGPU
    const char* src = webgpuSource(name);
    return device.createShader(gpu::ShaderStage::Fragment, src, std::strlen(src));
#else
    const std::string src = loadShader((std::string(name) + ".metal").c_str());
    return device.createShader(gpu::ShaderStage::Fragment, src.data(), src.size());
#endif
}

}  // namespace

bool Renderer::Pipelines::ensure(gpu::Device& device) {
    if (ready_) {
        return true;
    }
    gpu::PipelineDesc pd;
    // Follow-up: per-blend pipelines. merge() now groups both-Plus, but every
    // content pipeline below is created SrcOver, so Plus still draws as
    // SrcOver. A Plus pipeline family (solid/rounded/blit/glyph) plus a blend
    // switch on flush is needed to make Plus visually correct.
    pd.blend = gpu::Blend::SrcOver;
    const struct Entry {
        const char* name;
        bool required;
        gpu::Pipeline* out;
    } entries[] = {
        {"solid", true, &solid},     {"blit", true, &blit},     {"rounded", false, &rounded},
        {"glyph", true, &glyph},     {"blur", true, &blur},     {"blur1d", true, &blur1d},
        {"glass", true, &glass},     {"gradient", false, &gradient},
    };
    for (const Entry& e : entries) {
        auto vs = makeVertex(device, e.name);
        auto fs = makeFragment(device, e.name);
        if (!vs.ok() || !fs.ok()) {
            if (!e.required) {
                continue;
            }
            return false;
        }
        pd.vertexShader = vs->handle();
        pd.fragmentShader = fs->handle();
        auto pipe = device.createPipeline(pd);
        if (!pipe.ok()) {
            if (!e.required) {
                continue;
            }
            return false;
        }
        *e.out = std::move(pipe.value());
    }
    ready_ = true;
    return true;
}

}  // namespace glim::paint
