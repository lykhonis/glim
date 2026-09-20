struct Instance {
    rect: vec4<f32>,
    grad: vec4<f32>,
    misc: vec4<f32>,
    colors: array<vec4<f32>, 8>,
    offsets: array<f32, 8>,
};

struct Uniforms {
    projection: mat4x4<f32>,
};

@group(0) @binding(0) var<storage, read> instances: array<Instance>;
@group(0) @binding(1) var<storage, read> uniforms: Uniforms;

struct VSOut {
    @builtin(position) position: vec4<f32>,
    @location(0) pos: vec2<f32>,
    @location(1) @interpolate(flat) iid: u32,
};

@vertex
fn vs_main(
    @builtin(vertex_index) vid: u32,
    @builtin(instance_index) iid: u32,
) -> VSOut {
    var unit = array<vec2<f32>, 6>(
        vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 0.0), vec2<f32>(0.0, 1.0),
        vec2<f32>(0.0, 1.0), vec2<f32>(1.0, 0.0), vec2<f32>(1.0, 1.0),
    );
    let p = unit[vid];
    let inst = instances[iid];
    let pos = inst.rect.xy + p * inst.rect.zw;
    var out: VSOut;
    out.position = uniforms.projection * vec4<f32>(pos, 0.0, 1.0);
    out.pos = pos;
    out.iid = iid;
    return out;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
    let inst = instances[in.iid];
    let n = i32(inst.misc.w + 0.5);
    if (n <= 0) {
        return vec4<f32>(0.0);
    }
    var t: f32;
    if (inst.misc.x < 0.5) {
        let d = inst.grad.zw - inst.grad.xy;
        let denom = dot(d, d);
        if (denom > 1e-8) {
            t = dot(in.pos - inst.grad.xy, d) / denom;
        } else {
            t = 0.0;
        }
    } else {
        if (inst.misc.y > 1e-6) {
            t = length(in.pos - inst.grad.xy) / inst.misc.y;
        } else {
            t = 0.0;
        }
    }
    t = clamp(t, 0.0, 1.0);
    var c = inst.colors[0];
    for (var i = 1; i < 8; i++) {
        if (i >= n) {
            break;
        }
        let o0 = inst.offsets[i - 1];
        let o1 = inst.offsets[i];
        var f: f32;
        if (o1 > o0) {
            f = clamp((t - o0) / (o1 - o0), 0.0, 1.0);
        } else {
            if (t >= o1) {
                f = 1.0;
            } else {
                f = 0.0;
            }
        }
        c = mix(c, inst.colors[i], f);
    }
    return c * inst.misc.z;
}
