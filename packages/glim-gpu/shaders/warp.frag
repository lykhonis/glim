#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_tint;
layout(location = 2) in vec2 v_unit;
layout(location = 3) flat in vec4 v_fade;
layout(location = 4) flat in vec2 v_alphas;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 2) uniform texture2D tex;
layout(set = 0, binding = 6) uniform sampler samp;

void main() {
  // Frame targets are sRGB views, decoded on sample (as blit_linear).
  // Fade ramp over the unit square (UnitFade).
  const float t = clamp(dot(v_unit - v_fade.xy, v_fade.zw), 0.0, 1.0);
  out_color = texture(sampler2D(tex, samp), v_uv) * v_tint * mix(v_alphas.x, v_alphas.y, t);
}
