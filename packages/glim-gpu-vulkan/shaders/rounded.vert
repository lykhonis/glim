#version 450

struct Instance {
    vec4 rect;
    vec4 radii;
    vec4 color;
    vec4 extra;
};

layout(std430, set = 0, binding = 0) readonly buffer Instances {
    Instance instances[];
};

layout(std140, set = 0, binding = 1) uniform Uniforms {
    mat4 projection;
};

layout(location = 0) out vec2 v_logical;
layout(location = 1) flat out vec4 v_color;
layout(location = 2) flat out vec4 v_rect;
layout(location = 3) flat out vec4 v_radii;
layout(location = 4) flat out float v_strokeWidth;

void main() {
    const vec2 unit[6] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
        vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0));
    const Instance inst = instances[gl_InstanceIndex];
    const float pad = inst.extra.x * 0.5 + 1.0;
    const vec2 origin = inst.rect.xy - pad;
    const vec2 size = inst.rect.zw + 2.0 * pad;
    const vec2 pos = origin + unit[gl_VertexIndex] * size;
    gl_Position = projection * vec4(pos, 0.0, 1.0);
    v_logical = pos;
    v_color = inst.color;
    v_rect = inst.rect;
    v_radii = inst.radii;
    v_strokeWidth = inst.extra.x;
}
