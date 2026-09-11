#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 1) in float v_opacity;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 2) uniform sampler2D tex;

void main() {
    out_color = texture(tex, v_uv) * v_opacity;
}
