struct Instance {
    rect: vec4<f32>,
    uv: vec4<f32>,
    extra: vec4<f32>,
    pill0: vec4<f32>,
    pill1: vec4<f32>,
    pill2: vec4<f32>,
    pill3: vec4<f32>,
    radii: vec4<f32>,
};

struct Uniforms {
    projection: mat4x4<f32>,
};

@group(0) @binding(0) var<storage, read> instances: array<Instance>;
@group(0) @binding(1) var<storage, read> uniforms: Uniforms;
@group(0) @binding(2) var tex: texture_2d<f32>;
@group(0) @binding(6) var texSampler: sampler;

struct VSOut {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) local: vec2<f32>,
    @location(2) @interpolate(flat) iid: u32,
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
    var out: VSOut;
    out.position = uniforms.projection * vec4<f32>(inst.rect.xy + p * inst.rect.zw, 0.0, 1.0);
    out.uv = mix(inst.uv.xy, inst.uv.zw, p);
    out.local = p;
    out.iid = iid;
    return out;
}

fn sdRoundBox(p: vec2<f32>, b: vec2<f32>, r: f32) -> f32 {
    let rr = min(r, min(b.x, b.y));
    let q = abs(p) - b + vec2<f32>(rr, rr);
    return length(max(q, vec2<f32>(0.0))) + min(max(q.x, q.y), 0.0) - rr;
}

fn pillAt(inst: Instance, i: i32) -> vec4<f32> {
    if (i == 1) {
        return inst.pill1;
    }
    if (i == 2) {
        return inst.pill2;
    }
    if (i == 3) {
        return inst.pill3;
    }
    return inst.pill0;
}

fn radiusAt(inst: Instance, i: i32) -> f32 {
    if (i == 1) {
        return inst.radii.y;
    }
    if (i == 2) {
        return inst.radii.z;
    }
    if (i == 3) {
        return inst.radii.w;
    }
    return inst.radii.x;
}

fn fieldSdf(inst: Instance, p: vec2<f32>) -> f32 {
    let count = max(1, i32(inst.extra.w) & 7);
    var sdf = 1e6;
    for (var i = 0; i < 4; i++) {
        if (i >= count) {
            break;
        }
        let pill = pillAt(inst, i);
        let c = pill.xy + 0.5 * pill.zw;
        let d = sdRoundBox(p - c, 0.5 * pill.zw, radiusAt(inst, i));
        if (i == 0) {
            sdf = d;
        } else {
            sdf = min(sdf, d);
        }
    }
    return sdf;
}

fn sampleBlur(uvIn: vec2<f32>, sigma: f32, texel: vec2<f32>) -> vec3<f32> {
    let uv = clamp(uvIn, vec2<f32>(0.0), vec2<f32>(1.0));
    let span = mix(1.0, 2.6, saturate(sigma / 12.0));
    let s = texel * span;
    let w = array<f32, 5>(0.0625, 0.25, 0.375, 0.25, 0.0625);
    var c = vec3<f32>(0.0);
    for (var j = 0; j < 5; j++) {
        let y = f32(j - 2) * s.y;
        for (var i = 0; i < 5; i++) {
            let p = uv + vec2<f32>(f32(i - 2) * s.x, y);
            c += textureSampleLevel(tex, texSampler, clamp(p, vec2<f32>(0.0), vec2<f32>(1.0)), 0.0).rgb * (w[i] * w[j]);
        }
    }
    return c;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
    let inst = instances[in.iid];
    let sigma = inst.extra.x;
    let flatten = ((i32(inst.extra.w) >> 3) & 1) != 0;
    let size = inst.rect.zw;
    let p = in.local * size;
    let sdf = fieldSdf(inst, p);
    let fw = fwidth(sdf);
    let aa = max(fw, 1e-3);
    let mask = saturate(-sdf / aa);
    if (mask < 0.001) {
        return vec4<f32>(0.0);
    }
    let dims = vec2<f32>(textureDimensions(tex));
    let texel = vec2<f32>(1.0 / max(dims.x, 1.0), 1.0 / max(dims.y, 1.0));
    let uv = clamp(in.uv, vec2<f32>(0.0), vec2<f32>(1.0));
    var col = sampleBlur(uv, sigma, texel);
    if (flatten) {
        let lum = dot(col, vec3<f32>(0.2126, 0.7152, 0.0722));
        if (lum >= 0.45) {
            col = vec3<f32>(0.92);
        } else {
            col = vec3<f32>(0.18);
        }
    } else {
        let frost = mix(0.06, 0.36, saturate(sigma / 12.0));
        col = mix(col, vec3<f32>(1.0), frost);
    }
    return vec4<f32>(col * mask, mask);
}
