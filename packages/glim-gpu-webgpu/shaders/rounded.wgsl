struct Instance {
    rect: vec4<f32>,
    radii: vec4<f32>,
    color: vec4<f32>,
    extra: vec4<f32>,
};

struct Uniforms {
    projection: mat4x4<f32>,
};

@group(0) @binding(0) var<storage, read> instances: array<Instance>;
@group(0) @binding(1) var<storage, read> uniforms: Uniforms;

struct VSOut {
    @builtin(position) position: vec4<f32>,
    @location(0) logical: vec2<f32>,
    @location(1) @interpolate(flat) iid: u32,
};

fn coverageAt(p: vec2<f32>, rect: vec4<f32>, radii: vec4<f32>) -> f32 {
    let center = rect.xy + 0.5 * rect.zw;
    let b = 0.5 * rect.zw;
    let q = p - center;
    var r = 0.0;
    if (q.x < 0.0 && q.y < 0.0) {
        r = radii.x;
    } else if (q.x >= 0.0 && q.y < 0.0) {
        r = radii.y;
    } else if (q.x < 0.0 && q.y >= 0.0) {
        r = radii.z;
    } else {
        r = radii.w;
    }
    let d = abs(q) - b + vec2<f32>(r, r);
    let outside = length(max(d, vec2<f32>(0.0))) + min(max(d.x, d.y), 0.0) - r;
    return saturate(0.5 - outside);
}

fn coverage(p: vec2<f32>, inst: Instance) -> f32 {
    let strokeWidth = inst.extra.x;
    if (strokeWidth > 0.0) {
        let halfw = strokeWidth * 0.5;
        let outer = vec4<f32>(inst.rect.xy - halfw, inst.rect.zw + strokeWidth);
        let orad = inst.radii + halfw;
        let outerCov = coverageAt(p, outer, orad);
        let innerSize = inst.rect.zw - strokeWidth;
        if (innerSize.x > 0.0 && innerSize.y > 0.0) {
            let inner = vec4<f32>(inst.rect.xy + halfw, innerSize);
            let irad = max(inst.radii - halfw, vec4<f32>(0.0));
            return max(0.0, outerCov - coverageAt(p, inner, irad));
        }
        return outerCov;
    }
    return coverageAt(p, inst.rect, inst.radii);
}

@vertex
fn vs_main(
    @builtin(vertex_index) vid: u32,
    @builtin(instance_index) iid: u32,
) -> VSOut {
    var unit = array<vec2<f32>, 6>(
        vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 0.0), vec2<f32>(0.0, 1.0),
        vec2<f32>(0.0, 1.0), vec2<f32>(1.0, 0.0), vec2<f32>(1.0, 1.0),
    );
    let inst = instances[iid];
    let pad = inst.extra.x * 0.5 + 1.0;
    let origin = inst.rect.xy - pad;
    let size = inst.rect.zw + 2.0 * pad;
    let pos = origin + unit[vid] * size;
    var out: VSOut;
    out.position = uniforms.projection * vec4<f32>(pos, 0.0, 1.0);
    out.logical = pos;
    out.iid = iid;
    return out;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
    let inst = instances[in.iid];
    return inst.color * coverage(in.logical, inst);
}
