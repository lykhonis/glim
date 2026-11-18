#version 450

// A frame target composited through a homography (perspective isolate).
// h maps the unit square to target px (x, y, w); keeping w in the clip
// position makes the rasterizer interpolate uv perspective-correct.
struct Instance {
    vec4 h0;
    vec4 h1;
    vec4 h2;
    vec4 uv;
    vec4 extra;
    vec4 fade;
    vec4 alphas;
};

layout(std430, set = 0, binding = 0) readonly buffer Instances {
    Instance instances[];
};

layout(std430, set = 0, binding = 1) readonly buffer Uniforms {
    mat4 projection;
};

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_tint;
layout(location = 2) out vec2 v_unit;
layout(location = 3) flat out vec4 v_fade;
layout(location = 4) flat out vec2 v_alphas;

void main() {
    const vec2 unit[6] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
        vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0));
    const vec2 p = unit[gl_VertexIndex];
    const Instance inst = instances[gl_InstanceIndex];
    const mat3 h = mat3(inst.h0.xyz, vec3(inst.h0.w, inst.h1.xy), vec3(inst.h1.zw, inst.h2.x));
    const vec3 q = h * vec3(p, 1.0);
    gl_Position = (projection * vec4(q.xy / q.z, 0.0, 1.0)) * q.z;
    v_uv = mix(inst.uv.xy, inst.uv.zw, p);
    v_tint = inst.extra;
    v_unit = p;
    v_fade = inst.fade;
    v_alphas = inst.alphas.xy;
}
