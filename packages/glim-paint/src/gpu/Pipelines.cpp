#include <glim/assert.h>
#include <glim/paint/Renderer.h>

#include <cstring>
#include <string>
#include <vector>

#if GLIM_GPU_VULKAN
#include "glim_vulkan_shaders.h"
namespace shaders = glim::vulkan_shaders;
#elif GLIM_GPU_WEBGPU
#include "glim_webgpu_shaders.h"
namespace shaders = glim::webgpu_shaders;
#else
#include <fstream>
#include <sstream>

#include "glim_metal_shaders.h"
namespace shaders = glim::metal_shaders;
#endif

namespace glim::paint {

namespace {

std::string shaderCode(const char* name, gpu::ShaderStage stage) {
  const bool vertex = stage == gpu::ShaderStage::Vertex;
#if defined(GLIM_METAL_SHADER_DIR) && !GLIM_GPU_VULKAN && !GLIM_GPU_WEBGPU
  // Dev builds read the committed source so a regenerated shader needs no rebuild.
  std::ifstream in(
      std::string(GLIM_METAL_SHADER_DIR) + "/" + name + (vertex ? ".vert.metal" : ".frag.metal")
  );
  if (in) {
    std::ostringstream ss;
    ss << in.rdbuf();
    if (!ss.str().empty()) {
      return ss.str();
    }
  }
#endif
  for (const shaders::Source& src : shaders::kSources) {
    if (std::strcmp(src.name, name) == 0) {
      const shaders::Stage& code = vertex ? src.vert : src.frag;
      return std::string(static_cast<const char*>(code.data), code.size);
    }
  }
  return {};
}

// Every pipeline's shaders, compiled as one batch: vertex then fragment per name.
constexpr const char* kShaderNames[] = {
    "solid",
    "blit",
    "blit_linear",
    "rounded",
    "glyph",
    "blur",
    "blur1d",
    "glass",
    "gradient",
};
constexpr std::size_t kShaderCount = sizeof(kShaderNames) / sizeof(kShaderNames[0]);

struct ShaderBatch {
  std::vector<std::string> code;
  std::vector<gpu::ShaderSource> sources() const {
    std::vector<gpu::ShaderSource> out;
    for (std::size_t i = 0; i < code.size(); ++i) {
      const auto stage = i % 2 == 0 ? gpu::ShaderStage::Vertex : gpu::ShaderStage::Fragment;
      out.push_back({stage, code[i].data(), code[i].size()});
    }
    return out;
  }
};

ShaderBatch shaderBatch() {
  ShaderBatch batch;
  for (const char* name : kShaderNames) {
    batch.code.push_back(shaderCode(name, gpu::ShaderStage::Vertex));
    batch.code.push_back(shaderCode(name, gpu::ShaderStage::Fragment));
  }
  return batch;
}

}  // namespace

void Renderer::Pipelines::prepare(gpu::Device& device) {
  device.prepareShaders(shaderBatch().sources());
}

bool Renderer::Pipelines::ensure(gpu::Device& device) {
  if (ready_) {
    return true;
  }
  gpu::PipelineDesc pd;
  pd.blend = gpu::Blend::SrcOver;
  // Same order as kShaderNames.
  const struct Entry {
    const char* name;
    bool required;
    gpu::Pipeline* out;
  } entries[kShaderCount] = {
      {"solid", true, &solid},
      {"blit", true, &blit},
      {"blit_linear", true, &blitLinear},
      {"rounded", false, &rounded},
      {"glyph", true, &glyph},
      {"blur", true, &blur},
      {"blur1d", true, &blur1d},
      {"glass", true, &glass},
      {"gradient", false, &gradient},
  };
  struct Cached {
    const char* name;
    gpu::Handle vs = 0;
    gpu::Handle fs = 0;
    bool vsOk = false;
    bool fsOk = false;
  };
  const ShaderBatch batch = shaderBatch();
  const std::vector<gpu::Result<gpu::Shader>> compiled = device.createShaders(batch.sources());
  Cached cached[kShaderCount]{};
  for (std::size_t i = 0; i < kShaderCount; ++i) {
    const Entry& e = entries[i];
    GLIM_ASSERT(std::strcmp(e.name, kShaderNames[i]) == 0, "entries follow kShaderNames");
    const auto& vs = compiled[2 * i];
    const auto& fs = compiled[2 * i + 1];
    if (!vs.ok() || !fs.ok()) {
      if (!e.required) {
        continue;
      }
      return false;
    }
    cached[i].name = e.name;
    cached[i].vs = vs->handle();
    cached[i].fs = fs->handle();
    cached[i].vsOk = true;
    cached[i].fsOk = true;
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
  pd.blend = gpu::Blend::Plus;
  const struct Entry plusEntries[] = {
      {"solid", true, &solidPlus},
      {"blit", true, &blitPlus},
      {"rounded", false, &roundedPlus},
      {"glyph", true, &glyphPlus},
      {"gradient", false, &gradientPlus},
  };
  for (const Entry& e : plusEntries) {
    gpu::Handle vsH = 0;
    gpu::Handle fsH = 0;
    bool found = false;
    for (const Cached& c : cached) {
      if (c.name && std::strcmp(c.name, e.name) == 0 && c.vsOk && c.fsOk) {
        vsH = c.vs;
        fsH = c.fs;
        found = true;
        break;
      }
    }
    if (!found) {
      if (!e.required) {
        continue;
      }
      return false;
    }
    pd.vertexShader = vsH;
    pd.fragmentShader = fsH;
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
