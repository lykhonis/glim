#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_tint;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 2) uniform sampler2D tex;

void main() {
    const float sdf = texture(tex, v_uv).r;
    const float w = max(0.03, fwidth(sdf) * 0.6);
    const float a = smoothstep(0.5 - w, 0.5 + w, sdf);
    out_color = vec4(v_tint.rgb * a, v_tint.a * a);
}
