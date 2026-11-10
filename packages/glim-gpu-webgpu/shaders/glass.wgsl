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
    edge: f32,
    destUv0: vec2<f32>,
    destUv1: vec2<f32>,
    material: f32,
    transmit: f32,
    rippleAmp: f32,
    timeSec: f32,
    swellAmp: f32,
    envSoft: f32,
    absorbK: f32,
    envLight: f32,
    swellScale: vec2<f32>,
    swellPhase: f32,
    _padS: f32,
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
    var minHalf = 1e6;
    for (var j = 0; j < 8; j++) {
        if (j >= count) {
            break;
        }
        let hej = u.shapes[j].halfExtent * u.dpr;
        minHalf = min(minHalf, min(hej.x, hej.y));
    }
    let globalK = min(u.shapes[0].mergeK * u.dpr, minHalf * 0.42);
    var d = 1e6;
    for (var i = 0; i < 8; i++) {
        if (i >= count) {
            break;
        }
        let s = u.shapes[i];
        let c = s.center * u.dpr;
        let he = s.halfExtent * u.dpr;
        let r = min(s.corner * u.dpr, min(he.x, he.y));
        let k = globalK;
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
    let glen = length(g);
    if (glen > 1e-3) {
        return g / glen;
    }
    let count = clamp(i32(u.shapeCount), 0, 8);
    var best = vec2<f32>(0.0, 1.0);
    var bd = 1e30;
    for (var i = 0; i < 8; i++) {
        if (i >= count) {
            break;
        }
        let v = p - u.shapes[i].center * u.dpr;
        let dd = dot(v, v);
        if (dd < bd) {
            bd = dd;
            best = v;
        }
    }
    // placeholder to keep structure; real assignment below
    let bl = length(best);
    if (bl > 1e-4) {
        return best / bl;
    }
    return vec2<f32>(0.0, 1.0);
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
            // Curvature lives only in the end caps: distance past the
            // straight segment, ramped smoothly from half a radius before the seam.
            let fc = abs(p - c) - (he - r);
            fw = smoothstep(-0.5 * rr, rr, select(fc.y, fc.x, he.x >= he.y));
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

// Surface swell for water and liquid metal (lens::swellAt): slope xy, crest focus z.
// ph drifts the waves at rates 1, -1, 2 (loops every 2*pi).
fn swellAt(q: vec2<f32>, ph: f32) -> vec3<f32> {
    let a = vec3<f32>(1.9 * q.x + 1.1 * q.y + 0.4 + ph,
                      -1.3 * q.x + 2.6 * q.y + 2.1 - ph,
                      3.4 * q.x - 2.1 * q.y + 4.7 + 2.0 * ph);
    let c = cos(a);
    return vec3<f32>((0.95 * c.x - 0.39 * c.y + 0.68 * c.z) * 0.5,
                     (0.55 * c.x + 0.78 * c.y - 0.42 * c.z) * 0.5,
                     dot(vec3<f32>(2.41, 2.535, 3.194), sin(a)) * 0.125);
}

// Chrome studio (lens::streak / lens::chromeStudio): soft white walls with
// dark seams and one bright softbox, seen along reflection R.
fn streak(e: f32, c: f32, w: f32) -> f32 {
    let t = saturate(1.0 - abs(e - c) / max(w, 1e-4));
    return t * t * (3.0 - 2.0 * t);
}

// Chrome rim (lens::chromeRim): bright upper lip, dark lower lip.
fn chromeRim(ny: f32) -> f32 {
    return 0.18 + 0.82 * smoothstep(-0.5, 0.6, -ny);
}

fn chromeStudio(ry: f32) -> f32 {
    let e = -ry;
    let wall = 0.55 + 0.3 * smoothstep(-1.0, 0.9, e);
    let seams = 0.9 * streak(e, -0.15, 0.32) + 0.75 * streak(e, 0.62, 0.22) +
                0.6 * streak(e, -0.85, 0.25);
    return wall * (1.0 - seams) + 0.4 * streak(e, 0.25, 0.16);
}

// Metal F0 from its tint (CPU: metalF0): silver near-white, gold saturated.
fn metalF0(tint: vec4<f32>) -> vec3<f32> {
    var t = vec3<f32>(1.0);
    if (tint.a > 0.0) {
        t = tint.rgb;
    }
    return t / max(max(t.r, max(t.g, t.b)), 1e-4) * 0.95;
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
    let isMetal = u.material > 0.5 && u.material < 1.5 && u.transmit <= 0.0;
    let isWater = u.material > 1.5;
    let tt = max(u.thickness * u.dpr, 1.0);
    let delta = max(-d, 0.0);
    let dprN0 = max(u.dpr, 1.0);
    let maxWrap = max(12.0 * dprN0, max(tt, r0 * 0.6));
    let deep = delta > maxWrap && !isMetal && !isWater;
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
        let t = saturate(delta / max(wrapT, 1.0));
        // Smooth profile, zero slope at the band edge so the
        // refraction field stays C1 where it meets the flat interior.
        wrapX = 1.0 - t * t * (3.0 - 2.0 * t);
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

    var rip = vec2<f32>(0.0);
    if (u.material > 1.5) {
        let t = u.timeSec;
        let ph1 = 6.28318 * (in.uv.x * 3.0 + in.uv.y * 2.0 + t * 0.12);
        let ph2 = 6.28318 * (in.uv.x * 5.0 - in.uv.y * 3.0 - t * 0.09);
        rip = u.rippleAmp * vec2<f32>(cos(ph1) + 0.6 * cos(ph2), sin(ph1 * 1.3) + 0.6 * sin(ph2 * 1.1));
        offset += rip / max(u.resolution, vec2<f32>(1.0)) * 0.6;
    }
    // Water body lens: inward shift falling linearly rim -> dome center
    // (monotonic, magnifies R/(R-bend)). CPU: sampleGlassPlate water block.
    var bodyOff = vec2<f32>(0.0);
    if (isWater) {
        let domeR = max(min(he0.x, he0.y), 1.0);
        let tiltw = saturate(1.0 - delta / domeR);
        let bend = min((1.0 - 1.0 / max(u.ior, 1.01)) * tt * 1.6, domeR * 0.45);
        // Still surface swell bends what lies beneath into soft waves.
        bodyOff = (-n2 * tiltw + swellAt(p / domeR * u.swellScale, u.swellPhase).xy * u.swellAmp) * bend /
                  max(u.resolution, vec2<f32>(1.0));
        offset += bodyOff;
    }
    let uvIso = clamp(in.uv + bodyOff, vec2<f32>(0.001), vec2<f32>(0.999));
    let uvL = mix(u.destUv0, u.destUv1, uvIso);
    // Clear is sharp: no interior frost. Regular keeps the milk blur core.
    var frost = 0.0;
    if (u.blurEdge > 0.5) {
        frost = 0.88;
    }
    let pane = mix(textureSampleLevel(u_sharp, samp, uvL, 0.0).rgb,
                    textureSampleLevel(u_blur, samp, uvIso, 0.0).rgb, frost);
    // Outside the wrap band the refracted sample has zero weight: skip
    // its texture reads (most of a pane's pixels).
    var color = pane;
    if (wrapX > 0.0) {
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
        var wrapMix = pow(saturate(wrapX), 1.0);
        if (u.blurEdge > 0.5) {
            wrapMix = pow(saturate(wrapX), 1.25);
        }
        color = mix(pane, wrapRgb, wrapMix);
    }
    let ll = rec709(pane);
    if (isMetal) {
        // Dome: |n.xy| 1 at the rim falling to 0 at depth = half height.
        // Dome folded by the still swell (CPU: metalNormal): full in the body,
        // gone at the rim so the bevel stays clean.
        let domeM = max(min(he0.x, he0.y), 1.0);
        let tiltm = saturate(1.0 - delta / domeM) * 0.97;
        let swM = swellAt(p / domeM * u.swellScale, u.swellPhase);
        var nm = n2 * tiltm + swM.xy * (u.swellAmp * (1.0 - tiltm));
        nm = nm * min(1.0, 0.97 / max(length(nm), 1e-6));
        let Nc = vec3<f32>(nm, sqrt(max(1.0 - dot(nm, nm), 0.0)));
        let la = u.glareAngle;
        let R = reflect(vec3<f32>(0.0, 0.0, -1.0), Nc);
        // envLight: the surroundings' brightness along R (reach 2.2x dome,
        // 3x3 roughness blur) lights the studio; the backdrop never shows.
        var envL = 0.0;
        if (u.envLight > 0.0) {
            let reach = 2.2 * domeM;
            let duv = (u.destUv1 - u.destUv0) / max(u.resolution, vec2<f32>(1.0));
            let uvM = mix(u.destUv0, u.destUv1, in.uv) + R.xy * reach * duv;
            let rough = 1.0 + u.envSoft * 4.0;
            var env = vec3<f32>(0.0);
            for (var j = -1; j <= 1; j = j + 1) {
                for (var i = -1; i <= 1; i = i + 1) {
                    let q = clamp(uvM + vec2<f32>(f32(i), f32(j)) * rough * duv,
                                  vec2<f32>(0.0), vec2<f32>(1.0));
                    env += textureSampleLevel(u_sharp, samp, q, 0.0).rgb;
                }
            }
            envL = rec709(env / 9.0);
        }
        // Chrome (CPU: metalEnvGlass): mirrors the studio; the channels split
        // toward the rim (dispersion); the rolled rim (chromeRim) lights the
        // upper lip and darkens the lower one.
        let lit = 0.8 + u.envLight * envL;
        let t2 = tiltm * tiltm;
        let split = u.dispersion * 0.6 * t2 * t2;
        var mc = vec3<f32>(chromeStudio(R.y - split), chromeStudio(R.y), chromeStudio(R.y + split)) * lit;
        let rimC = vec3<f32>(chromeRim(n2.y - 2.0 * split), chromeRim(n2.y), chromeRim(n2.y + 2.0 * split)) * lit;
        mc = mix(mc, rimC, smoothstep(0.65, 0.97, tiltm) * 0.85);
        let F0m = metalF0(u.tint);
        let Fm = F0m + (vec3<f32>(1.0) - F0m) * pow(1.0 - saturate(Nc.z), 5.0);
        let Ldm = normalize(vec3<f32>(cos(la), sin(la), 0.8));
        let Hvm = normalize(Ldm + vec3<f32>(0.0, 0.0, 1.0));
        let specm = pow(saturate(dot(Nc, Hvm)), 56.0) * u.glareIntensity;
        let bevel = pow(saturate(1.0 - abs(delta - 1.2 * dprN0) / (1.6 * dprN0)), 5.0) * 0.55;
        color = mix(mc * Fm + vec3<f32>(specm), vec3<f32>(1.0), bevel);
    } else if (!isWater) {
    var milkPull = 0.7;
    if (u.blurEdge > 0.5) {
        milkPull = 0.2;
    }
    var milk = 0.008 * (1.0 - wrapX * milkPull);
    if (u.lumaOn > 0.5) {
        milk = 0.04 * (1.0 - wrapX * milkPull);
    }
    color = mix(color, vec3<f32>(1.0), milk);
    if (u.lumaOn > 0.5) {
        let high = saturate((ll - 0.72) / 0.28);
        let low = saturate((0.28 - ll) / 0.28);
        color *= 1.0 - high * u.lumaShadow;
        color = mix(color, color + vec3<f32>(0.06, 0.07, 0.08), low * u.lumaLift);
    }
    // Plate wash: a set tint drives it, else the white milk.
    var washRgb = vec3<f32>(1.0);
    var washA = 0.0;
    if (u.tint.a > 0.0) {
        washRgb = u.tint.rgb;
        washA = u.tint.a;
    } else {
        washA = milk;
    }
    color = mix(color, washRgb, washA);
    // Clear dimmer: 35% darkening when plate luma exceeds 0.6 (spec S9).
    if (u.dimmer > 0.0 && ll > 0.6) {
        color *= 1.0 - u.dimmer;
    }
    }

    let dprN = max(u.dpr, 1.0);
    let inside = max(-d, 0.0);
    let hair = pow(saturate(1.0 - abs(inside - 1.2 * dprN) / (1.8 * dprN)), 5.0);
    let ebits = u.edge;
    let eL = (ebits - floor(ebits / 2.0) * 2.0) > 0.5;
    let eR = (floor(ebits / 2.0) - floor(ebits / 4.0) * 2.0) > 0.5;
    let eT = (floor(ebits / 4.0) - floor(ebits / 8.0) * 2.0) > 0.5;
    let eB = floor(ebits / 8.0) > 0.5;
    var sideW = 0.0;
    if (eL) {
        sideW += pow(saturate(-n2.x), 2.0);
    }
    if (eR) {
        sideW += pow(saturate(n2.x), 2.0);
    }
    color *= 1.0 - sideW * hair * 0.42;
    var top = 0.0;
    if (eT) {
        top = saturate(-n2.y);
    }
    var bot = 0.0;
    if (eB) {
        bot = saturate(n2.y);
    }
    let hi = hair * (top * 0.35 + bot * 0.22) * u.glareIntensity;
    color = mix(color, vec3<f32>(1.0), saturate(hi));
    // Body Fresnel (Schlick, F0 = fresnelIntensity) + key-light specular.
    // Edge highlight lives in the outer 5px (CPU parity), fading from 3px;
    // the interior skips the lighting math it would scale by zero.
    let edgeBand = 1.0 - smoothstep(3.0 * dprN, 5.0 * dprN, inside);
    if (!isMetal && edgeBand > 0.0) {
        var n3xy = n2 * 0.9;  // CPU parity: flat edges stay off the glint lobe
        if (isWater) {
            n3xy += rip * 0.18;
        }
        let N3 = normalize(vec3<f32>(n3xy, 1.0));
        let cosT = saturate(N3.z);
        let fresPow = 5.0 + u.fresnelHardness;
        let F0 = u.fresnelIntensity;
        let fres = F0 + (1.0 - F0) * pow(1.0 - cosT, fresPow);
        let rimW = saturate(wrapX + hair * 0.5);
        var edgeSpec = 0.0;
        if (eL) {
            edgeSpec += saturate(-n2.x);
        }
        if (eR) {
            edgeSpec += saturate(n2.x);
        }
        if (eT) {
            edgeSpec += saturate(-n2.y);
        }
        if (eB) {
            edgeSpec += saturate(n2.y);
        }
        edgeSpec = saturate(edgeSpec);
        let la = u.glareAngle;
        let Ld = normalize(vec3<f32>(cos(la), sin(la), 0.8));
        let Hv = normalize(Ld + vec3<f32>(0.0, 0.0, 1.0));
        let shin = max(u.glareRange * 4.0, 8.0);
        let ndh = saturate(dot(N3, Hv));
        var spec = pow(ndh, shin) * u.glareIntensity;
        spec = saturate(spec - u.glareHardness * 0.1);
        spec *= mix(1.0, rimW, saturate(u.glareConvergence));
        spec *= edgeSpec;
        let fresWash = fres * (u.fresnelRange / 18.0) * rimW * edgeSpec;
        color += (fresWash * 0.30 + spec * (0.35 + 0.65 * rimW)) * edgeBand;
    }
    if (isWater) {
        // Depth absorption in the tint; swell crests focus light.
        // CPU: shadeGlass water block.
        let domeW = max(min(he0.x, he0.y), 1.0);
        let depthW = smoothstep(0.0, domeW, delta);
        color = mix(color, color * u.tint.rgb * 1.6, 0.2 * saturate(u.absorbK) * depthW);
        let sw = swellAt(p / domeW * u.swellScale, u.swellPhase);
        color = color * (1.0 + 0.23 * u.swellAmp * sw.z * depthW);
        // Crest lobe only for rippling water; still water has no crests.
        let crest = select(0.0, saturate((rip.x + rip.y) * 0.25 + 0.5), u.rippleAmp > 0.0);
        let rimWw = saturate(wrapX + hair * 0.5);
        let ca = pow(rimWw * (1.0 - rimWw) * 4.0, 2.0);
        color += vec3<f32>(0.85, 1.0, 0.92) * (ca * 0.14 * crest);
    }
    if (u.interactive > 0.5) {
        color = mix(color, vec3<f32>(1.0), top * hair * 0.18);
    }

    return vec4<f32>(color * alphaShape, alphaShape);
}
