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

layout(location = 0) in vec2 v_pos;
layout(location = 1) flat in uint v_iid;

layout(location = 0) out vec4 out_color;

void main() {
    const Instance inst = instances[v_iid];
    const int n = int(inst.misc.w + 0.5);
    if (n <= 0) {
        out_color = vec4(0.0);
        return;
    }
    float t;
    if (inst.misc.x < 0.5) {
        const vec2 d = inst.grad.zw - inst.grad.xy;
        const float denom = dot(d, d);
        t = denom > 1e-8 ? dot(v_pos - inst.grad.xy, d) / denom : 0.0;
    } else {
        t = inst.misc.y > 1e-6 ? length(v_pos - inst.grad.xy) / inst.misc.y : 0.0;
    }
    t = clamp(t, 0.0, 1.0);
    vec4 c = inst.colors[0];
    for (int i = 1; i < 8; ++i) {
        if (i >= n) {
            break;
        }
        const float o0 = i <= 4 ? inst.off0[i - 1] : inst.off1[i - 5];
        const float o1 = i < 4 ? inst.off0[i] : inst.off1[i - 4];
        const float f = o1 > o0 ? clamp((t - o0) / (o1 - o0), 0.0, 1.0) : (t >= o1 ? 1.0 : 0.0);
        c = mix(c, inst.colors[i], f);
    }
    out_color = c * inst.misc.z;
}
