#version 450

struct Instance {
    vec4 rect;
    vec4 uv;
    vec4 extra;
    vec4 light;
    vec4 pill0;
    vec4 pill1;
    vec4 pill2;
    vec4 pill3;
    vec4 radii;
};

layout(std430, set = 0, binding = 0) readonly buffer Instances {
    Instance instances[];
};

layout(std140, set = 0, binding = 1) uniform Uniforms {
    mat4 projection;
};

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec2 v_local;
layout(location = 2) flat out int v_iid;

void main() {
    const vec2 unit[6] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
        vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0));
    const vec2 p = unit[gl_VertexIndex];
    const Instance inst = instances[gl_InstanceIndex];
    gl_Position = projection * vec4(inst.rect.xy + p * inst.rect.zw, 0.0, 1.0);
    v_uv = mix(inst.uv.xy, inst.uv.zw, p);
    v_local = p;
    v_iid = gl_InstanceIndex;
}
