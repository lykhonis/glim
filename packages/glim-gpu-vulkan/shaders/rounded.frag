#version 450

layout(location = 0) in vec2 v_logical;
layout(location = 1) flat in vec4 v_color;
layout(location = 2) flat in vec4 v_rect;
layout(location = 3) flat in vec4 v_radii;
layout(location = 4) flat in float v_strokeWidth;
layout(location = 0) out vec4 out_color;

float coverageAt(vec2 p, vec4 rect, vec4 radii) {
    const vec2 center = rect.xy + 0.5 * rect.zw;
    const vec2 b = 0.5 * rect.zw;
    const vec2 q = p - center;
    float r = 0.0;
    if (q.x < 0.0 && q.y < 0.0) {
        r = radii.x;
    } else if (q.x >= 0.0 && q.y < 0.0) {
        r = radii.y;
    } else if (q.x < 0.0 && q.y >= 0.0) {
        r = radii.z;
    } else {
        r = radii.w;
    }
    const vec2 d = abs(q) - b + vec2(r);
    const float outside = length(max(d, vec2(0.0))) + min(max(d.x, d.y), 0.0) - r;
    return clamp(0.5 - outside, 0.0, 1.0);
}

void main() {
    float cov = 0.0;
    if (v_strokeWidth > 0.0) {
        const float halfw = v_strokeWidth * 0.5;
        const vec4 outer = vec4(v_rect.xy - halfw, v_rect.zw + v_strokeWidth);
        const vec4 orad = v_radii + halfw;
        float outerCov = coverageAt(v_logical, outer, orad);
        const vec2 innerSize = v_rect.zw - v_strokeWidth;
        if (innerSize.x > 0.0 && innerSize.y > 0.0) {
            const vec4 inner = vec4(v_rect.xy + halfw, innerSize);
            const vec4 irad = max(v_radii - halfw, vec4(0.0));
            cov = max(0.0, outerCov - coverageAt(v_logical, inner, irad));
        } else {
            cov = outerCov;
        }
    } else {
        cov = coverageAt(v_logical, v_rect, v_radii);
    }
    out_color = v_color * cov;
}
