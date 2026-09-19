#version 450

layout(location = 0) in vec2 v_pos;
layout(location = 1) in vec4 v_grad;
layout(location = 2) in vec4 v_misc;
layout(location = 3) in vec4 v_colors[8];
layout(location = 11) in vec4 v_off0;
layout(location = 12) in vec4 v_off1;

layout(location = 0) out vec4 out_color;

void main() {
    const int n = int(v_misc.w + 0.5);
    if (n <= 0) {
        out_color = vec4(0.0);
        return;
    }
    float t;
    if (v_misc.x < 0.5) {
        const vec2 d = v_grad.zw - v_grad.xy;
        const float denom = dot(d, d);
        t = denom > 1e-8 ? dot(v_pos - v_grad.xy, d) / denom : 0.0;
    } else {
        t = v_misc.y > 1e-6 ? length(v_pos - v_grad.xy) / v_misc.y : 0.0;
    }
    t = clamp(t, 0.0, 1.0);
    const float off[8] = float[](
        v_off0.x, v_off0.y, v_off0.z, v_off0.w,
        v_off1.x, v_off1.y, v_off1.z, v_off1.w);
    vec4 c = v_colors[0];
    for (int i = 1; i < 8; ++i) {
        if (i >= n) {
            break;
        }
        const float o0 = off[i - 1];
        const float o1 = off[i];
        const float f = o1 > o0 ? clamp((t - o0) / (o1 - o0), 0.0, 1.0) : (t >= o1 ? 1.0 : 0.0);
        c = mix(c, v_colors[i], f);
    }
    out_color = c * v_misc.z;
}
