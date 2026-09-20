struct GlassShape {
    center: vec2<f32>,
    halfExtent: vec2<f32>,
    corner: f32,
    n: f32,
    mergeK: f32,
    _pad: f32,
};

struct GlassUniforms {
    resolution: vec2<f32>,
    dpr: f32,
    shapeCount: f32,
    shapes: array<GlassShape, 8>,
    thickness: f32,
    ior: f32,
    refDistance: f32,
    dispersion: f32,
    fresnelRange: f32,
    fresnelHardness: f32,
    fresnelIntensity: f32,
    glareAngle: f32,
    glareRange: f32,
    glareHardness: f32,
    glareConvergence: f32,
    glareIntensity: f32,
    tint: vec4<f32>,
    blurEdge: f32,
    lumaLift: f32,
    lumaShadow: f32,
    lumaOn: f32,
    dimmer: f32,
    interactive: f32,
    flatten: f32,
    _pad1: f32,
    destUv0: vec2<f32>,
    destUv1: vec2<f32>,
};

struct Uniforms {
    projection: mat4x4<f32>,
};

@group(0) @binding(0) var<storage, read> instances: array<GlassUniforms>;
@group(0) @binding(1) var<storage, read> uniforms: Uniforms;
@group(0) @binding(2) var u_sharp: texture_2d<f32>;
@group(0) @binding(3) var u_blur: texture_2d<f32>;
@group(0) @binding(6) var samp: sampler;

struct VSOut {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
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
    let logical = inst.resolution / max(inst.dpr, 0.001);
    var out: VSOut;
    out.position = uniforms.projection * vec4<f32>(p * logical, 0.0, 1.0);
    out.uv = p;
    return out;
}

fn sminCirc(a: f32, b: f32, kIn: f32) -> f32 {
    if (kIn <= 0.001) {
        return min(a, b);
    }
    let k = kIn * 1.0 / (1.0 - sqrt(0.5));
    let h = max(k - abs(a - b), 0.0) / k;
    let t = max(1.0 - h * (h - 2.0), 0.0);
    return min(a, b) - k * 0.5 * (1.0 + h - sqrt(t));
}

fn sdCapsulePx(p: vec2<f32>, c: vec2<f32>, he: vec2<f32>, r: f32) -> f32 {
    var a: vec2<f32>;
    var b: vec2<f32>;
    if (he.x >= he.y) {
        a = c - vec2<f32>(he.x - r, 0.0);
        b = c + vec2<f32>(he.x - r, 0.0);
    } else {
        a = c - vec2<f32>(0.0, he.y - r);
        b = c + vec2<f32>(0.0, he.y - r);
    }
    let pa = p - a;
    let ba = b - a;
    let h = saturate(dot(pa, ba) / max(dot(ba, ba), 1e-4));
    return length(pa - ba * h) - r;
}

fn smax(a: f32, b: f32, k: f32) -> f32 {
    let h = saturate(0.5 + 0.5 * (b - a) / max(k, 1e-3));
    return mix(a, b, h) + k * h * (1.0 - h);
}

fn sdRoundedPx(p: vec2<f32>, c: vec2<f32>, he: vec2<f32>, r: f32, n: f32) -> f32 {
    let q = abs(p - c) - he + r;
    if (q.x > 0.0 && q.y > 0.0 && n > 2.01) {
        let pn = abs(q);
        return pow(pow(pn.x, n) + pow(pn.y, n), 1.0 / n) - r;
    }
    if (q.x < 0.0 && q.y < 0.0) {
        return smax(q.x, q.y, 8.0) - r;
    }
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2<f32>(0.0))) - r;
}

fn shapeSdf(u: GlassUniforms, p: vec2<f32>) -> f32 {
    let count = clamp(i32(u.shapeCount), 0, 8);
    if (count <= 0) {
        return 1e6;
    }
    var d = 1e6;
    for (var i = 0; i < 8; i++) {
        if (i >= count) {
            break;
        }
        let s = u.shapes[i];
        let c = s.center * u.dpr;
        let he = s.halfExtent * u.dpr;
        let r = min(s.corner * u.dpr, min(he.x, he.y));
        let k = min(s.mergeK * u.dpr, min(he.x, he.y) * 0.42);
        var sd: f32;
        if (r >= min(he.x, he.y) - 0.5) {
            sd = sdCapsulePx(p, c, he, r);
        } else {
            sd = sdRoundedPx(p, c, he, r, s.n);
        }
        if (i == 0) {
            d = sd;
        } else {
            d = sminCirc(d, sd, k);
        }
    }
    if (u.interactive > 0.5) {
        d -= 0.03 * min(u.resolution.x, u.resolution.y);
    }
    return d;
}

fn sdfNormal(u: GlassUniforms, p: vec2<f32>) -> vec2<f32> {
    let e = max(1.5 * u.dpr, 1.0);
    let g = vec2<f32>(shapeSdf(u, p + vec2<f32>(e, 0.0)) - shapeSdf(u, p - vec2<f32>(e, 0.0)),
                      shapeSdf(u, p + vec2<f32>(0.0, e)) - shapeSdf(u, p - vec2<f32>(0.0, e)));
    return g / max(length(g), 1e-5);
}

fn filletWeight(u: GlassUniforms, p: vec2<f32>) -> f32 {
    let count = clamp(i32(u.shapeCount), 0, 8);
    var w = 0.0;
    for (var i = 0; i < 8; i++) {
        if (i >= count) {
            break;
        }
        let s = u.shapes[i];
        let c = s.center * u.dpr;
        let he = s.halfExtent * u.dpr;
        let r = min(s.corner * u.dpr, min(he.x, he.y));
        let rr = max(r, 1.0);
        var fw: f32;
        if (r >= min(he.x, he.y) - 0.5) {
            var a: vec2<f32>;
            var ba: vec2<f32>;
            if (he.x >= he.y) {
                a = c - vec2<f32>(he.x - r, 0.0);
                ba = vec2<f32>(2.0 * (he.x - r), 0.0);
            } else {
                a = c - vec2<f32>(0.0, he.y - r);
                ba = vec2<f32>(0.0, 2.0 * (he.y - r));
            }
            let h = saturate(dot(p - a, ba) / max(dot(ba, ba), 1e-4));
            fw = smoothstep(0.12, 0.5, abs(h - 0.5) * 2.0);
        } else {
            let f = abs(p - c) - (he - r);
            fw = saturate(f.x / rr) * saturate(f.y / rr);
        }
        w = max(w, fw);
    }
    return w;
}

fn safeAsin(x: f32) -> f32 {
    return asin(clamp(x, -1.0, 1.0));
}

fn rec709(c: vec3<f32>) -> f32 {
    return dot(c, vec3<f32>(0.2126, 0.7152, 0.0722));
}

@fragment
fn fs_main(in: VSOut) -> @location(0) vec4<f32> {
    let u = instances[0];
    let p = in.uv * u.resolution;
    let d = shapeSdf(u, p);
    let he0 = u.shapes[0].halfExtent * u.dpr;
    let r0 = min(u.shapes[0].corner * u.dpr, min(he0.x, he0.y));
    let alphaShape = smoothstep(0.75, -0.75, d);
    if (alphaShape <= 0.001) {
        return vec4<f32>(0.0);
    }

    if (u.flatten > 0.5) {
        let fill = 0.94;
        var color = vec3<f32>(fill);
        let rim = smoothstep(1.6, 0.2, abs(d));
        color = mix(color, vec3<f32>(1.0), rim * 0.05);
        return vec4<f32>(color * alphaShape, alphaShape);
    }
    let tt = max(u.thickness * u.dpr, 1.0);
    let delta = max(-d, 0.0);
    let dprN0 = max(u.dpr, 1.0);
    let maxWrap = max(12.0 * dprN0, max(tt, r0 * 0.6));
    let deep = delta > maxWrap;
    var n2 = vec2<f32>(0.0, 1.0);
    var fillet = 0.0;
    if (!deep) {
        n2 = sdfNormal(u, p);
        fillet = filletWeight(u, p);
    }
    let wrapT = mix(12.0 * dprN0, max(tt, r0 * 0.6), fillet);
    var wrapX = 0.0;
    var wrapE = 0.0;
    if (delta < wrapT && u.thickness > 0.001) {
        wrapX = 1.0 - saturate(delta / max(wrapT, 1.0));
        let thetaI = safeAsin(pow(wrapX, 1.25));
        let eta = max(u.ior, 1.01);
        let thetaT = safeAsin(sin(thetaI) / eta);
        wrapE = -tan(thetaT - thetaI);
    }
    let distK = u.refDistance / 0.35;
    let diag = saturate(abs(n2.x * n2.y) * 2.0);
    let magPx = wrapE * 32.0 * distK * dprN0 * mix(0.7, 1.2, fillet) * mix(1.0, 0.5, diag);
    var offset = vec2<f32>(0.0);
    if (abs(magPx) > 1e-5) {
        offset = n2 * magPx / max(u.resolution, vec2<f32>(1.0));
    }

    let uvIso = clamp(in.uv, vec2<f32>(0.001), vec2<f32>(0.999));
    let uvL = mix(u.destUv0, u.destUv1, uvIso);
    var frost = 0.32;
    if (u.blurEdge > 0.5) {
        frost = 0.88;
    }
    let pane = mix(textureSampleLevel(u_sharp, samp, uvL, 0.0).rgb,
                    textureSampleLevel(u_blur, samp, uvIso, 0.0).rgb, frost);
    let gamma = u.dispersion;
    var wrapRgb: vec3<f32>;
    let uvIsoW = clamp(in.uv + offset, vec2<f32>(0.001), vec2<f32>(0.999));
    let uvW = mix(u.destUv0, u.destUv1, uvIsoW);
    if (gamma > 0.0 && wrapX > 0.08) {
        let uvIsoR = clamp(in.uv + offset * (1.0 - gamma), vec2<f32>(0.001), vec2<f32>(0.999));
        let uvIsoB = clamp(in.uv + offset * (1.0 + gamma), vec2<f32>(0.001), vec2<f32>(0.999));
        let uvR = mix(u.destUv0, u.destUv1, uvIsoR);
        let uvB = mix(u.destUv0, u.destUv1, uvIsoB);
        wrapRgb = vec3<f32>(textureSampleLevel(u_sharp, samp, uvR, 0.0).r,
                            textureSampleLevel(u_sharp, samp, uvW, 0.0).g,
                            textureSampleLevel(u_sharp, samp, uvB, 0.0).b);
    } else {
        wrapRgb = textureSampleLevel(u_sharp, samp, uvW, 0.0).rgb;
    }
    wrapRgb = mix(wrapRgb, textureSampleLevel(u_blur, samp, uvIsoW, 0.0).rgb, frost);
    var wrapMix = pow(saturate(wrapX), 0.95);
    if (u.blurEdge > 0.5) {
        wrapMix = pow(saturate(wrapX), 1.25);
    }
    var color = mix(pane, wrapRgb, wrapMix);
    let ll = rec709(pane);
    var milkPull = 0.7;
    if (u.blurEdge > 0.5) {
        milkPull = 0.2;
    }
    var milk = 0.08 * (1.0 - wrapX * milkPull);
    if (u.lumaOn > 0.5) {
        milk = 0.22 * (1.0 - wrapX * milkPull);
    }
    color = mix(color, vec3<f32>(1.0), milk);
    if (u.lumaOn > 0.5) {
        let high = saturate((ll - 0.72) / 0.28);
        let low = saturate((0.28 - ll) / 0.28);
        color *= 1.0 - high * u.lumaShadow;
        color = mix(color, color + vec3<f32>(0.06, 0.07, 0.08), low * u.lumaLift);
    }
    color = mix(color, u.tint.rgb, u.tint.a * 0.12);
    if (u.dimmer > 0.0 && ll > 0.72) {
        color = mix(color, color * 0.82, u.dimmer);
    }

    let dprN = max(u.dpr, 1.0);
    let inside = max(-d, 0.0);
    let hair = pow(saturate(1.0 - abs(inside - 1.2 * dprN) / (1.8 * dprN)), 5.0);
    let horiz = n2.x * n2.x;
    color *= 1.0 - horiz * hair * 0.42;
    let top = saturate(-n2.y);
    let bot = saturate(n2.y);
    let hi = hair * (top * 1.0 + bot * 0.62) * max(u.glareIntensity, 0.7);
    color = mix(color, vec3<f32>(1.0), saturate(hi));
    if (u.interactive > 0.5) {
        color = mix(color, vec3<f32>(1.0), top * hair * 0.18);
    }

    return vec4<f32>(color * alphaShape, alphaShape);
}
