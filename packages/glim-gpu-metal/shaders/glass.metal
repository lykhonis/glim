#include <metal_stdlib>
using namespace metal;

struct GlassShape {
    float2 center;
    float2 halfExtent;
    float corner;
    float n;
    float mergeK;
    float _pad;
};

struct GlassUniforms {
    float2 resolution;
    float dpr;
    float shapeCount;
    GlassShape shapes[8];
    float thickness;
    float ior;
    float refDistance;
    float dispersion;
    float fresnelRange;
    float fresnelHardness;
    float fresnelIntensity;
    float glareAngle;
    float glareRange;
    float glareHardness;
    float glareConvergence;
    float glareIntensity;
    float4 tint;
    float blurEdge;
    float lumaLift;
    float lumaShadow;
    float lumaOn;
    float dimmer;
    float interactive;
    float flatten;
    float edge;
    float2 destUv0;
    float2 destUv1;
    float material;
    float transmit;
    float rippleAmp;
    float timeSec;
    float swellAmp;
    float envSoft;
    float absorbK;
    float envLight;
    float2 swellScale;
    float swellPhase;
    float _padS;
};

struct Uniforms {
    float4x4 projection;
};

struct VSOut {
    float4 position [[position]];
    float2 uv;
};

vertex VSOut vs_main(uint vid [[vertex_id]],
                     uint iid [[instance_id]],
                     constant GlassUniforms* instances [[buffer(0)]],
                     constant Uniforms& uniforms [[buffer(1)]]) {
    const float2 unit[6] = {
        float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0),
        float2(0.0, 1.0), float2(1.0, 0.0), float2(1.0, 1.0),
    };
    const float2 p = unit[vid];
    const GlassUniforms inst = instances[iid];
    const float2 logical = inst.resolution / max(inst.dpr, 0.001);
    VSOut out;
    out.position = uniforms.projection * float4(p * logical, 0.0, 1.0);
    out.uv = p;
    return out;
}

float sminCirc(float a, float b, float k) {
    if (k <= 0.001) {
        return min(a, b);
    }
    k *= 1.0 / (1.0 - sqrt(0.5));
    const float h = max(k - abs(a - b), 0.0) / k;
    const float t = max(1.0 - h * (h - 2.0), 0.0);
    return min(a, b) - k * 0.5 * (1.0 + h - sqrt(t));
}

float sdCapsulePx(float2 p, float2 c, float2 he, float r) {
    float2 a;
    float2 b;
    if (he.x >= he.y) {
        a = c - float2(he.x - r, 0.0);
        b = c + float2(he.x - r, 0.0);
    } else {
        a = c - float2(0.0, he.y - r);
        b = c + float2(0.0, he.y - r);
    }
    const float2 pa = p - a;
    const float2 ba = b - a;
    const float h = saturate(dot(pa, ba) / max(dot(ba, ba), 1e-4));
    return length(pa - ba * h) - r;
}

float smax(float a, float b, float k) {
    const float h = saturate(0.5 + 0.5 * (b - a) / max(k, 1e-3));
    return mix(a, b, h) + k * h * (1.0 - h);
}

float sdRoundedPx(float2 p, float2 c, float2 he, float r, float n) {
    const float2 q = abs(p - c) - he + r;
    if (q.x > 0.0 && q.y > 0.0 && n > 2.01) {
        const float2 pn = abs(q);
        return pow(pow(pn.x, n) + pow(pn.y, n), 1.0 / n) - r;
    }
    if (q.x < 0.0 && q.y < 0.0) {
        return smax(q.x, q.y, 8.0) - r;
    }
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float shapeSdf(constant GlassUniforms& u, float2 p) {
    const int count = clamp(int(u.shapeCount), 0, 8);
    if (count <= 0) {
        return 1e6;
    }
    float minHalf = 1e6;
    for (int j = 0; j < 8; ++j) {
        if (j >= count) {
            break;
        }
        const float2 hej = u.shapes[j].halfExtent * u.dpr;
        minHalf = min(minHalf, min(hej.x, hej.y));
    }
    const float globalK = min(u.shapes[0].mergeK * u.dpr, minHalf * 0.42);
    float d = 1e6;
    for (int i = 0; i < 8; ++i) {
        if (i >= count) {
            break;
        }
        const GlassShape s = u.shapes[i];
        const float2 c = s.center * u.dpr;
        const float2 he = s.halfExtent * u.dpr;
        const float r = min(s.corner * u.dpr, min(he.x, he.y));
        const float k = globalK;
        float sd;
        if (r >= min(he.x, he.y) - 0.5) {
            sd = sdCapsulePx(p, c, he, r);
        } else {
            sd = sdRoundedPx(p, c, he, r, s.n);
        }
        d = (i == 0) ? sd : sminCirc(d, sd, k);
    }
    if (u.interactive > 0.5) {
        d -= 0.03 * min(u.resolution.x, u.resolution.y);
    }
    return d;
}

float2 sdfNormal(constant GlassUniforms& u, float2 p) {
    const float e = max(1.5 * u.dpr, 1.0);
    float2 g = float2(shapeSdf(u, p + float2(e, 0.0)) - shapeSdf(u, p - float2(e, 0.0)),
                      shapeSdf(u, p + float2(0.0, e)) - shapeSdf(u, p - float2(0.0, e)));
    const float glen = length(g);
    if (glen > 1e-3) {
        return g / glen;
    }
    const int count = clamp(int(u.shapeCount), 0, 8);
    float2 best = float2(0.0, 1.0);
    float bd = 1e30;
    for (int i = 0; i < 8; ++i) {
        if (i >= count) {
            break;
        }
        const float2 v = p - u.shapes[i].center * u.dpr;
        const float dd = dot(v, v);
        if (dd < bd) {
            bd = dd;
            best = v;
        }
    }
    const float bl = length(best);
    return bl > 1e-4 ? best / bl : float2(0.0, 1.0);
}

float filletWeight(constant GlassUniforms& u, float2 p) {
    const int count = clamp(int(u.shapeCount), 0, 8);
    float w = 0.0;
    for (int i = 0; i < 8; ++i) {
        if (i >= count) {
            break;
        }
        const GlassShape s = u.shapes[i];
        const float2 c = s.center * u.dpr;
        const float2 he = s.halfExtent * u.dpr;
        const float r = min(s.corner * u.dpr, min(he.x, he.y));
        const float rr = max(r, 1.0);
        float fw;
        if (r >= min(he.x, he.y) - 0.5) {
            // Curvature lives only in the end caps: distance past the
            // straight segment, ramped smoothly from half a radius before the seam.
            const float2 f = abs(p - c) - (he - r);
            fw = smoothstep(-0.5 * rr, rr, (he.x >= he.y) ? f.x : f.y);
        } else {
            const float2 f = abs(p - c) - (he - r);
            fw = saturate(f.x / rr) * saturate(f.y / rr);
        }
        w = max(w, fw);
    }
    return w;
}

float safeAsin(float x) {
    return asin(clamp(x, -1.0, 1.0));
}

// Surface swell for water and liquid metal (lens::swellAt): slope xy, crest focus z.
// ph drifts the waves at rates 1, -1, 2 (loops every 2*pi).
float3 swellAt(float2 q, float ph) {
    const float a1 = 1.9 * q.x + 1.1 * q.y + 0.4 + ph;
    const float a2 = -1.3 * q.x + 2.6 * q.y + 2.1 - ph;
    const float a3 = 3.4 * q.x - 2.1 * q.y + 4.7 + 2.0 * ph;
    const float3 c = cos(float3(a1, a2, a3));
    return float3((0.95 * c.x - 0.39 * c.y + 0.68 * c.z) * 0.5,
                  (0.55 * c.x + 0.78 * c.y - 0.42 * c.z) * 0.5,
                  dot(float3(2.41, 2.535, 3.194), sin(float3(a1, a2, a3))) * 0.125);
}

// Chrome studio (lens::streak / lens::chromeStudio): soft white walls with
// dark seams and one bright softbox, seen along reflection R.
float streak(float e, float c, float w) {
    const float t = saturate(1.0 - abs(e - c) / max(w, 1e-4));
    return t * t * (3.0 - 2.0 * t);
}

// Chrome rim (lens::chromeRim): bright upper lip, dark lower lip.
float chromeRim(float ny) {
    return 0.18 + 0.82 * smoothstep(-0.5, 0.6, -ny);
}

float chromeStudio(float ry) {
    const float e = -ry;
    const float wall = 0.55 + 0.3 * smoothstep(-1.0, 0.9, e);
    const float seams = 0.9 * streak(e, -0.15, 0.32) + 0.75 * streak(e, 0.62, 0.22) +
                        0.6 * streak(e, -0.85, 0.25);
    return wall * (1.0 - seams) + 0.4 * streak(e, 0.25, 0.16);
}

// Metal F0 from its tint (CPU: metalF0): silver near-white, gold saturated.
float3 metalF0(float4 tint) {
    const float3 t = tint.a > 0.0 ? tint.rgb : float3(1.0);
    return t / max(max(t.r, max(t.g, t.b)), 1e-4) * 0.95;
}

float rec709(float3 c) {
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

fragment float4 fs_main(VSOut in [[stage_in]],
                         constant GlassUniforms* instances [[buffer(0)]],
                         texture2d<float> u_sharp [[texture(0)]],
                         texture2d<float> u_blur [[texture(1)]],
                         sampler samp [[sampler(0)]]) {
    constant GlassUniforms& u = instances[0];
    const float2 p = in.uv * u.resolution;
    float d = shapeSdf(u, p);
    const float2 he0 = u.shapes[0].halfExtent * u.dpr;
    const float r0 = min(u.shapes[0].corner * u.dpr, min(he0.x, he0.y));
    const float alphaShape = smoothstep(0.75, -0.75, d);
    if (alphaShape <= 0.001) {
        return float4(0.0);
    }

    if (u.flatten > 0.5) {
        const float fill = 0.94;
        float3 color = float3(fill);
        const float rim = smoothstep(1.6, 0.2, abs(d));
        color = mix(color, float3(1.0), rim * 0.05);
        return float4(color * alphaShape, alphaShape);
    }
    const bool isMetal = u.material > 0.5 && u.material < 1.5 && u.transmit <= 0.0;
    const bool isWater = u.material > 1.5;
    const float T = max(u.thickness * u.dpr, 1.0);
    const float delta = max(-d, 0.0);
    const float dprN0 = max(u.dpr, 1.0);
    // Deep interior: beyond any possible wrap radius, refraction is zero.
    // Skip the normal (4 SDF evals) and fillet loop for these bulk pixels.
    const float maxWrap = max(12.0 * dprN0, max(T, r0 * 0.6));
    // Metal reads N at full weight everywhere, so it never takes the
    // default-normal shortcut (which would seam the env interior).
    const bool deep = delta > maxWrap && !isMetal && !isWater;
    const float2 n2 = deep ? float2(0.0, 1.0) : sdfNormal(u, p);
    const float fillet = deep ? 0.0 : filletWeight(u, p);
    const float wrapT = mix(12.0 * dprN0, max(T, r0 * 0.6), fillet);
    float wrapX = 0.0;
    float wrapE = 0.0;
    if (delta < wrapT && u.thickness > 0.001) {
        const float t = saturate(delta / max(wrapT, 1.0));
        // Smooth profile, zero slope at the band edge so the
        // refraction field stays C1 where it meets the flat interior.
        wrapX = 1.0 - t * t * (3.0 - 2.0 * t);
        const float thetaI = safeAsin(pow(wrapX, 1.25));
        const float eta = max(u.ior, 1.01);
        const float thetaT = safeAsin(sin(thetaI) / eta);
        wrapE = -tan(thetaT - thetaI);
    }
    const float distK = u.refDistance / 0.35;
    const float diag = saturate(abs(n2.x * n2.y) * 2.0);
    const float magPx = wrapE * 32.0 * distK * dprN0 * mix(0.7, 1.2, fillet) * mix(1.0, 0.5, diag);
    float2 offset = float2(0.0);
    if (abs(magPx) > 1e-5) {
        offset = n2 * magPx / max(u.resolution, float2(1.0));
    }

    // Water ripple: animated normal-field wobble of the sample point.
    float2 rip = float2(0.0);
    if (u.material > 1.5) {
        const float t = u.timeSec;
        const float ph1 = 6.28318 * (in.uv.x * 3.0 + in.uv.y * 2.0 + t * 0.12);
        const float ph2 = 6.28318 * (in.uv.x * 5.0 - in.uv.y * 3.0 - t * 0.09);
        rip = u.rippleAmp * float2(cos(ph1) + 0.6 * cos(ph2), sin(ph1 * 1.3) + 0.6 * sin(ph2 * 1.1));
        offset += rip / max(u.resolution, float2(1.0)) * 0.6;
    }
    // Water body lens: inward shift falling linearly rim -> dome center
    // (monotonic, magnifies R/(R-bend)). CPU: sampleGlassPlate water block.
    float2 bodyOff = float2(0.0);
    if (isWater) {
        const float domeR = max(min(he0.x, he0.y), 1.0);
        const float tiltw = saturate(1.0 - delta / domeR);
        const float bend = min((1.0 - 1.0 / max(u.ior, 1.01)) * T * 1.6, domeR * 0.45);
        // Still surface swell bends what lies beneath into soft waves.
        bodyOff = (-n2 * tiltw + swellAt(p / domeR * u.swellScale, u.swellPhase).xy * u.swellAmp) * bend /
                  max(u.resolution, float2(1.0));
        offset += bodyOff;
    }
    const float2 uvIso = clamp(in.uv + bodyOff, float2(0.001), float2(0.999));
    const float2 uvL = mix(u.destUv0, u.destUv1, uvIso);
    // Clear is sharp: no interior frost. Regular keeps the milk blur core.
    const float frost = (u.blurEdge > 0.5) ? 0.88 : 0.0;
    const float3 pane = mix(u_sharp.sample(samp, uvL, level(0.0)).rgb,
                            u_blur.sample(samp, uvIso, level(0.0)).rgb, frost);
    // Outside the wrap band the refracted sample has zero weight: skip
    // its texture reads (most of a pane's pixels).
    float3 sampleRgb = pane;
    if (wrapX > 0.0) {
        const float gamma = u.dispersion;
        float3 wrapRgb;
        const float2 uvIsoW = clamp(in.uv + offset, float2(0.001), float2(0.999));
        const float2 uvW = mix(u.destUv0, u.destUv1, uvIsoW);
        if (gamma > 0.0 && wrapX > 0.08) {
            const float2 uvIsoR = clamp(in.uv + offset * (1.0 - gamma), float2(0.001), float2(0.999));
            const float2 uvIsoB = clamp(in.uv + offset * (1.0 + gamma), float2(0.001), float2(0.999));
            const float2 uvR = mix(u.destUv0, u.destUv1, uvIsoR);
            const float2 uvB = mix(u.destUv0, u.destUv1, uvIsoB);
            wrapRgb = float3(u_sharp.sample(samp, uvR, level(0.0)).r,
                             u_sharp.sample(samp, uvW, level(0.0)).g,
                             u_sharp.sample(samp, uvB, level(0.0)).b);
        } else {
            wrapRgb = u_sharp.sample(samp, uvW, level(0.0)).rgb;
        }
        wrapRgb = mix(wrapRgb, u_blur.sample(samp, uvIsoW, level(0.0)).rgb, frost);
        const float wrapMix = pow(saturate(wrapX), (u.blurEdge > 0.5) ? 1.25 : 1.0);
        sampleRgb = mix(pane, wrapRgb, wrapMix);
    }

    float3 color = sampleRgb;
    const float L = rec709(pane);
    if (isMetal) {
        // Dome folded by the still swell (CPU: metalNormal): full in the body,
        // gone at the rim so the bevel stays clean.
        const float domeM = max(min(he0.x, he0.y), 1.0);
        const float tiltm = saturate(1.0 - delta / domeM) * 0.97;
        const float3 swM = swellAt(p / domeM * u.swellScale, u.swellPhase);
        float2 nm = n2 * tiltm + swM.xy * (u.swellAmp * (1.0 - tiltm));
        nm *= min(1.0, 0.97 / max(length(nm), 1e-6));
        const float3 Nc = float3(nm, sqrt(max(1.0 - dot(nm, nm), 0.0)));
        const float la = u.glareAngle;
        const float3 R = reflect(float3(0.0, 0.0, -1.0), Nc);
        // envLight: the surroundings' brightness along R (reach 2.2x dome,
        // 3x3 roughness blur) lights the studio; the backdrop never shows.
        float envL = 0.0;
        if (u.envLight > 0.0) {
            const float reach = 2.2 * domeM;
            const float2 duv = (u.destUv1 - u.destUv0) / max(u.resolution, float2(1.0));
            const float2 uvM = mix(u.destUv0, u.destUv1, in.uv) + R.xy * reach * duv;
            const float rough = 1.0 + u.envSoft * 4.0;
            float3 env = float3(0.0);
            for (int j = -1; j <= 1; ++j) {
                for (int i = -1; i <= 1; ++i) {
                    const float2 q = clamp(uvM + float2(float(i), float(j)) * rough * duv,
                                           float2(0.0), float2(1.0));
                    env += u_sharp.sample(samp, q, level(0.0)).rgb;
                }
            }
            envL = rec709(env / 9.0);
        }
        // Chrome (CPU: metalEnvGlass): mirrors the studio; the channels split
        // toward the rim (dispersion); the rolled rim (chromeRim) lights the
        // upper lip and darkens the lower one.
        const float lit = 0.8 + u.envLight * envL;
        const float t2 = tiltm * tiltm;
        const float split = u.dispersion * 0.6 * t2 * t2;
        float3 mc = float3(chromeStudio(R.y - split), chromeStudio(R.y), chromeStudio(R.y + split)) * lit;
        const float3 rimC = float3(chromeRim(n2.y - 2.0 * split), chromeRim(n2.y), chromeRim(n2.y + 2.0 * split)) * lit;
        mc = mix(mc, rimC, smoothstep(0.65, 0.97, tiltm) * 0.85);
        const float3 F0m = metalF0(u.tint);
        const float3 Fm = F0m + (1.0 - F0m) * pow(1.0 - saturate(Nc.z), 5.0);
        const float3 Ldm = normalize(float3(cos(la), sin(la), 0.8));
        const float3 Hvm = normalize(Ldm + float3(0.0, 0.0, 1.0));
        const float specm = pow(saturate(dot(Nc, Hvm)), 56.0) * u.glareIntensity;
        const float bevel = pow(saturate(1.0 - abs(delta - 1.2 * dprN0) / (1.6 * dprN0)), 5.0) * 0.55;
        color = mix(mc * Fm + specm, float3(1.0), bevel);
    } else if (!isWater) {
    const float milkPull = (u.blurEdge > 0.5) ? 0.2 : 0.7;
    // Plate wash: a set tint drives it, else the white milk.
    float3 washRgb;
    float washA;
    if (u.tint.a > 0.0) {
        washRgb = u.tint.rgb;
        washA = u.tint.a;
    } else {
        washRgb = float3(1.0);
        washA = (u.lumaOn > 0.5 ? 0.04 : 0.008) * (1.0 - wrapX * milkPull);
    }
    color = mix(color, washRgb, washA);
    if (u.lumaOn > 0.5) {
        const float high = saturate((L - 0.72) / 0.28);
        const float low = saturate((0.28 - L) / 0.28);
        color *= 1.0 - high * u.lumaShadow;
        color = mix(color, color + float3(0.06, 0.07, 0.08), low * u.lumaLift);
    }
    // Clear dimmer: 35% darkening when plate luma exceeds 0.6 (spec S9).
    if (u.dimmer > 0.0 && L > 0.6) {
        color *= 1.0 - u.dimmer;
    }
    }

    // Thin inner hairline, per-side like the software renderer.
    const float dprN = max(u.dpr, 1.0);
    const float inside = max(-d, 0.0);
    const float hair = pow(saturate(1.0 - abs(inside - 1.2 * dprN) / (1.8 * dprN)), 5.0);
    const float ebits = u.edge;
    const bool eL = fmod(ebits, 2.0) > 0.5;
    const bool eR = fmod(floor(ebits / 2.0), 2.0) > 0.5;
    const bool eT = fmod(floor(ebits / 4.0), 2.0) > 0.5;
    const bool eB = fmod(floor(ebits / 8.0), 2.0) > 0.5;
    float sideW = 0.0;
    if (eL) {
        sideW += pow(saturate(-n2.x), 2.0);
    }
    if (eR) {
        sideW += pow(saturate(n2.x), 2.0);
    }
    color *= 1.0 - sideW * hair * 0.42;
    const float top = saturate(-n2.y) * (eT ? 1.0 : 0.0);
    const float bot = saturate(n2.y) * (eB ? 1.0 : 0.0);
    const float hi = hair * (top * 0.35 + bot * 0.22) * u.glareIntensity;
    color = mix(color, float3(1.0), saturate(hi));
    // Body Fresnel (Schlick, F0 = fresnelIntensity) + key-light specular.
    // Light azimuth = glareAngle (-60 deg), shininess = glareRange * 4.
    // Edge highlight lives in the outer 5px (CPU parity), fading from 3px;
    // the interior skips the lighting math it would scale by zero.
    const float edgeBand = 1.0 - smoothstep(3.0 * dprN, 5.0 * dprN, inside);
    if (!isMetal && edgeBand > 0.0) {
        float2 n3xy = n2 * 0.9;  // CPU parity: flat edges stay off the glint lobe
        if (isWater) {
            n3xy += rip * 0.18;
        }
        const float3 N3 = normalize(float3(n3xy, 1.0));
        const float cosT = saturate(N3.z);
        const float fresPow = 5.0 + u.fresnelHardness;
        const float F0 = u.fresnelIntensity;
        const float fres = F0 + (1.0 - F0) * pow(1.0 - cosT, fresPow);
        const float rimW = saturate(wrapX + hair * 0.5);
        float edgeSpec = 0.0;
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
        const float la = u.glareAngle;
        const float3 Ld = normalize(float3(cos(la), sin(la), 0.8));
        const float3 Hv = normalize(Ld + float3(0.0, 0.0, 1.0));
        const float shin = max(u.glareRange * 4.0, 8.0);
        const float ndh = saturate(dot(N3, Hv));
        float spec = pow(ndh, shin) * u.glareIntensity;
        spec = saturate(spec - u.glareHardness * 0.1);
        spec *= mix(1.0, rimW, saturate(u.glareConvergence));
        spec *= edgeSpec;
        const float fresWash = fres * (u.fresnelRange / 18.0) * rimW * edgeSpec;
        color += (fresWash * 0.30 + spec * (0.35 + 0.65 * rimW)) * edgeBand;
    }
    if (isWater) {
        // Depth absorption in the tint; swell crests focus light.
        // CPU: shadeGlass water block.
        const float domeW = max(min(he0.x, he0.y), 1.0);
        const float depthW = smoothstep(0.0, domeW, delta);
        color = mix(color, color * u.tint.rgb * 1.6, 0.2 * saturate(u.absorbK) * depthW);
        const float3 sw = swellAt(p / domeW * u.swellScale, u.swellPhase);
        color *= 1.0 + 0.23 * u.swellAmp * sw.z * depthW;
        // Crest lobe only for rippling water; still water has no crests.
        const float crest = u.rippleAmp > 0.0 ? saturate((rip.x + rip.y) * 0.25 + 0.5) : 0.0;
        const float rimWw = saturate(wrapX + hair * 0.5);
        const float ca = pow(rimWw * (1.0 - rimWw) * 4.0, 2.0);
        color += float3(0.85, 1.0, 0.92) * (ca * 0.14 * crest);
    }
    if (u.interactive > 0.5) {
        color = mix(color, float3(1.0), top * hair * 0.18);
    }

    return float4(color * alphaShape, alphaShape);
}
