#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_tint;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 2) uniform sampler2D tex;

void main() {
  // Frame-target copy: source view is sRGB, so the sample already
  // arrives decoded to linear (hardware). No manual EOTF — that
  // path is for UNORM image textures in blit.frag.
  out_color = texture(tex, v_uv) * v_tint;
}
