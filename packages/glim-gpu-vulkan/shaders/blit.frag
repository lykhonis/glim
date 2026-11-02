#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_tint;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 2) uniform sampler2D tex;

void main() {
  // Image bytes are sRGB in a UNORM texture (no hardware decode),
  // so decode manually to the linear working space. Frame targets
  // are sRGB views (hardware decodes on sample) and must use
  // blit_linear instead — never this shader.
  vec4 s = texture(tex, v_uv);
  vec3 lo = s.rgb / 12.92;
  vec3 hi = pow((s.rgb + vec3(0.055)) / 1.055, vec3(2.4));
  s.rgb = mix(hi, lo, step(s.rgb, vec3(0.04045)));
  out_color = s * v_tint;
}
