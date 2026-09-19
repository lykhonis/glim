#version 450

struct Instance {
    vec4 rect;
    vec4 grad;
    vec4 misc;
    vec4 colors[8];
    vec4 off0;
    vec4 off1;
};

layout(std430, set = 0, binding = 0) readonly buffer Instances {
    Instance instances[];
};

layout(std140, set = 0, binding = 1) uniform Uniforms {
    mat4 projection;
};

layout(location = 0) out vec2 v_pos;
layout(location = 1) out vec4 v_grad;
layout(location = 2) out vec4 v_misc;
layout(location = 3) out vec4 v_colors[8];
layout(location = 11) out vec4 v_off0;
layout(location = 12) out vec4 v_off1;

void main() {
    const vec2 unit[6] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
        vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0));
    const vec2 p = unit[gl_VertexIndex];
    const Instance inst = instances[gl_InstanceIndex];
    const vec2 pos = inst.rect.xy + p * inst.rect.zw;
    gl_Position = projection * vec4(pos, 0.0, 1.0);
    v_pos = pos;
    v_grad = inst.grad;
    v_misc = inst.misc;
    for (int i = 0; i < 8; ++i) {
        v_colors[i] = inst.colors[i];
    }
    v_off0 = inst.off0;
    v_off1 = inst.off1;
}
