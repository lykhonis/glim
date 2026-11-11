#include <glim/paint/Renderer.h>

#include <cstring>
#include <string>

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

auto makeShader(gpu::Device& device, const char* name, gpu::ShaderStage stage) {
  const bool vertex = stage == gpu::ShaderStage::Vertex;
#if defined(GLIM_METAL_SHADER_DIR) && !GLIM_GPU_VULKAN && !GLIM_GPU_WEBGPU
  // Dev builds read the committed source so a regenerated shader needs no rebuild.
  std::ifstream in(
      std::string(GLIM_METAL_SHADER_DIR) + "/" + name + (vertex ? ".vert.metal" : ".frag.metal")
  );
  if (in) {
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    if (!text.empty()) {
      return device.createShader(stage, text.data(), text.size());
    }
  }
#endif
  for (const shaders::Source& src : shaders::kSources) {
    if (std::strcmp(src.name, name) == 0) {
      const shaders::Stage& code = vertex ? src.vert : src.frag;
      return device.createShader(stage, static_cast<const char*>(code.data), code.size);
    }
  }
  return device.createShader(stage, nullptr, 0);
}

auto makeVertex(gpu::Device& device, const char* name) {
  return makeShader(device, name, gpu::ShaderStage::Vertex);
}

auto makeFragment(gpu::Device& device, const char* name) {
  return makeShader(device, name, gpu::ShaderStage::Fragment);
}

}  // namespace

bool Renderer::Pipelines::ensure(gpu::Device& device) {
  if (ready_) {
    return true;
  }
  gpu::PipelineDesc pd;
  pd.blend = gpu::Blend::SrcOver;
  const struct Entry {
    const char* name;
    bool required;
    gpu::Pipeline* out;
  } entries[] = {
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
  Cached cached[9]{};
  for (std::size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i) {
    const Entry& e = entries[i];
    auto vs = makeVertex(device, e.name);
    auto fs = makeFragment(device, e.name);
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
      auto vs = makeVertex(device, e.name);
      auto fs = makeFragment(device, e.name);
      if (!vs.ok() || !fs.ok()) {
        if (!e.required) {
          continue;
        }
        return false;
      }
      vsH = vs->handle();
      fsH = fs->handle();
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
