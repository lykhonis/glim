struct Instance {
    rect: vec4<f32>,
    uv: vec4<f32>,
    extra: vec4<f32>,
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
    @location(1) extra: vec4<f32>,
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
    out.extra = inst.extra;
    return out;
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
    let sigma = max(in.extra.x, 0.001);
    let radius = clamp(i32(ceil(3.0 * sigma)), 1, 12);
    let dims = vec2<f32>(textureDimensions(tex));
    let texel = vec2<f32>(in.extra.y / max(dims.x, 1.0), in.extra.z / max(dims.y, 1.0));
    var acc = vec4<f32>(0.0);
    var wt = 0.0;
    for (var k = -12; k <= 12; k++) {
        if (abs(k) > radius) {
            continue;
        }
        let d = f32(k) / sigma;
        let w = exp(-0.5 * d * d);
        acc += textureSampleLevel(tex, texSampler, in.uv + texel * f32(k), 0.0) * w;
        wt += w;
    }
    return acc / max(wt, 1e-5);
}
