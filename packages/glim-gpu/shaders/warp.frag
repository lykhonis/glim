#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_tint;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 2) uniform texture2D tex;
layout(set = 0, binding = 6) uniform sampler samp;

void main() {
  // Frame targets are sRGB views, decoded on sample (as blit_linear).
  out_color = texture(sampler2D(tex, samp), v_uv) * v_tint;
}
