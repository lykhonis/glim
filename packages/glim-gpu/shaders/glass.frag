#version 450

struct GlassShape {
    vec2 center;
    vec2 halfExtent;
    float corner, n, mergeK, _pad;
};

struct GlassUniforms {
    vec2 resolution;
    float dpr;
    float shapeCount;
    GlassShape shapes[8];
    float thickness, ior, refDistance, dispersion;
    float fresnelRange, fresnelHardness, fresnelIntensity, glareAngle;
    float glareRange, glareHardness, glareConvergence, glareIntensity;
    vec4 tint;
    float blurEdge, lumaLift, lumaShadow, lumaOn;
    float dimmer, interactive, flatten, edge;
    vec2 destUv0;
    vec2 destUv1;
    float material, transmit, rippleAmp, timeSec;
    float swellAmp, envSoft, absorbK, envLight;
    vec2 swellScale;
    float swellPhase;
    float _padS;
};

layout(std430, set = 0, binding = 0) readonly buffer Instances {
    GlassUniforms u;
};

layout(set = 0, binding = 2) uniform texture2D u_sharp;
layout(set = 0, binding = 3) uniform texture2D u_blur;
layout(set = 0, binding = 6) uniform sampler samp;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 out_color;


float sminCirc(float a, float b, float k) {
    if (k <= 0.001) {
        return min(a, b);
    }
    k *= 1.0 / (1.0 - sqrt(0.5));
    const float h = max(k - abs(a - b), 0.0) / k;
    const float t = max(1.0 - h * (h - 2.0), 0.0);
    return min(a, b) - k * 0.5 * (1.0 + h - sqrt(t));
}

float sdCapsulePx(vec2 p, vec2 c, vec2 he, float r) {
    vec2 a;
    vec2 b;
    if (he.x >= he.y) {
        a = c - vec2(he.x - r, 0.0);
        b = c + vec2(he.x - r, 0.0);
    } else {
        a = c - vec2(0.0, he.y - r);
        b = c + vec2(0.0, he.y - r);
    }
    const vec2 pa = p - a;
    const vec2 ba = b - a;
    const float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-4), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

float smax(float a, float b, float k) {
    const float h = clamp(0.5 + 0.5 * (b - a) / max(k, 1e-3), 0.0, 1.0);
    return mix(a, b, h) + k * h * (1.0 - h);
}

float sdRoundedPx(vec2 p, vec2 c, vec2 he, float r, float n) {
    const vec2 q = abs(p - c) - he + r;
    if (q.x > 0.0 && q.y > 0.0 && n > 2.01) {
        const vec2 pn = abs(q);
        return pow(pow(pn.x, n) + pow(pn.y, n), 1.0 / n) - r;
    }
    if (q.x < 0.0 && q.y < 0.0) {
        return smax(q.x, q.y, 8.0) - r;
    }
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float shapeSdf(vec2 p) {
    const int count = clamp(int(u.shapeCount), 0, 8);
    if (count <= 0) {
        return 1e6;
    }
    float minHalf = 1e6;
    for (int j = 0; j < 8; ++j) {
        if (j >= count) {
            break;
        }
        const vec2 hej = u.shapes[j].halfExtent * u.dpr;
        minHalf = min(minHalf, min(hej.x, hej.y));
    }
    const float globalK = min(u.shapes[0].mergeK * u.dpr, minHalf * 0.42);
    float d = 1e6;
    for (int i = 0; i < 8; ++i) {
        if (i >= count) {
            break;
        }
        const GlassShape s = u.shapes[i];
        const vec2 c = s.center * u.dpr;
        const vec2 he = s.halfExtent * u.dpr;
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

vec2 sdfNormal(vec2 p) {
    const float e = max(1.5 * u.dpr, 1.0);
    vec2 g = vec2(shapeSdf(p + vec2(e, 0.0)) - shapeSdf(p - vec2(e, 0.0)),
                  shapeSdf(p + vec2(0.0, e)) - shapeSdf(p - vec2(0.0, e)));
    const float glen = length(g);
    if (glen > 1e-3) {
        return g / glen;
    }
    const int count = clamp(int(u.shapeCount), 0, 8);
    vec2 best = vec2(0.0, 1.0);
    float bd = 1e30;
    for (int i = 0; i < 8; ++i) {
        if (i >= count) {
            break;
        }
        const vec2 v = p - u.shapes[i].center * u.dpr;
        const float dd = dot(v, v);
        if (dd < bd) {
            bd = dd;
            best = v;
        }
    }
    const float bl = length(best);
    return bl > 1e-4 ? best / bl : vec2(0.0, 1.0);
}

float filletWeight(vec2 p) {
    const int count = clamp(int(u.shapeCount), 0, 8);
    float w = 0.0;
    for (int i = 0; i < 8; ++i) {
        if (i >= count) {
            break;
        }
        const GlassShape s = u.shapes[i];
        const vec2 c = s.center * u.dpr;
        const vec2 he = s.halfExtent * u.dpr;
        const float r = min(s.corner * u.dpr, min(he.x, he.y));
        const float rr = max(r, 1.0);
        float fw;
        if (r >= min(he.x, he.y) - 0.5) {
            // Curvature lives only in the end caps: distance past the
            // straight segment, ramped smoothly from half a radius before the seam.
            const vec2 fc = abs(p - c) - (he - r);
            fw = smoothstep(-0.5 * rr, rr, (he.x >= he.y) ? fc.x : fc.y);
        } else {
            const vec2 f = abs(p - c) - (he - r);
            fw = clamp(f.x / rr, 0.0, 1.0) * clamp(f.y / rr, 0.0, 1.0);
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
vec3 swellAt(vec2 q, float ph) {
    const vec3 a = vec3(1.9 * q.x + 1.1 * q.y + 0.4 + ph,
                        -1.3 * q.x + 2.6 * q.y + 2.1 - ph,
                        3.4 * q.x - 2.1 * q.y + 4.7 + 2.0 * ph);
    const vec3 c = cos(a);
    return vec3((0.95 * c.x - 0.39 * c.y + 0.68 * c.z) * 0.5,
                (0.55 * c.x + 0.78 * c.y - 0.42 * c.z) * 0.5,
                dot(vec3(2.41, 2.535, 3.194), sin(a)) * 0.125);
}

// Chrome studio (lens::streak / lens::chromeStudio): soft white walls with
// dark seams and one bright softbox, seen along reflection R.
float streak(float e, float c, float w) {
    const float t = clamp(1.0 - abs(e - c) / max(w, 1e-4), 0.0, 1.0);
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
vec3 metalF0(vec4 tint) {
    const vec3 t = tint.a > 0.0 ? tint.rgb : vec3(1.0);
    return t / max(max(t.r, max(t.g, t.b)), 1e-4) * 0.95;
}

float rec709(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    const vec2 p = v_uv * u.resolution;
    float d = shapeSdf(p);
    const vec2 he0 = u.shapes[0].halfExtent * u.dpr;
    const float r0 = min(u.shapes[0].corner * u.dpr, min(he0.x, he0.y));
    const float alphaShape = smoothstep(0.75, -0.75, d);
    if (alphaShape <= 0.001) {
        out_color = vec4(0.0);
        return;
    }

    if (u.flatten > 0.5) {
        vec3 color = vec3(0.94);
        const float rim = smoothstep(1.6, 0.2, abs(d));
        color = mix(color, vec3(1.0), rim * 0.05);
        out_color = vec4(color * alphaShape, alphaShape);
        return;
    }
    const bool isMetal = u.material > 0.5 && u.material < 1.5 && u.transmit <= 0.0;
    const bool isWater = u.material > 1.5;
    const float T = max(u.thickness * u.dpr, 1.0);
    const float delta = max(-d, 0.0);
    const float dprN0 = max(u.dpr, 1.0);
    // Deep interior: beyond any possible wrap radius, refraction is zero.
    // Skip the normal (4 SDF evals) and fillet loop for these bulk pixels.
    const float maxWrap = max(12.0 * dprN0, max(T, r0 * 0.6));
    const bool deep = delta > maxWrap && !isMetal && !isWater;
    const vec2 n2 = deep ? vec2(0.0, 1.0) : sdfNormal(p);
    const float fillet = deep ? 0.0 : filletWeight(p);
    const float wrapT = mix(12.0 * dprN0, max(T, r0 * 0.6), fillet);
    float wrapX = 0.0;
    float wrapE = 0.0;
    if (delta < wrapT && u.thickness > 0.001) {
        const float t = clamp(delta / max(wrapT, 1.0), 0.0, 1.0);
        // Smooth profile, zero slope at the band edge so the
        // refraction field stays C1 where it meets the flat interior.
        wrapX = 1.0 - t * t * (3.0 - 2.0 * t);
        const float thetaI = safeAsin(pow(wrapX, 1.25));
        const float eta = max(u.ior, 1.01);
        const float thetaT = safeAsin(sin(thetaI) / eta);
        wrapE = -tan(thetaT - thetaI);
    }
    const float distK = u.refDistance / 0.35;
    const float diag = clamp(abs(n2.x * n2.y) * 2.0, 0.0, 1.0);
    const float magPx = wrapE * 32.0 * distK * dprN0 * mix(0.7, 1.2, fillet) * mix(1.0, 0.5, diag);
    vec2 offset = vec2(0.0);
    if (abs(magPx) > 1e-5) {
        offset = n2 * magPx / max(u.resolution, vec2(1.0));
    }

    vec2 rip = vec2(0.0);
    if (u.material > 1.5) {
        const float t = u.timeSec;
        const float ph1 = 6.28318 * (v_uv.x * 3.0 + v_uv.y * 2.0 + t * 0.12);
        const float ph2 = 6.28318 * (v_uv.x * 5.0 - v_uv.y * 3.0 - t * 0.09);
        rip = u.rippleAmp * vec2(cos(ph1) + 0.6 * cos(ph2), sin(ph1 * 1.3) + 0.6 * sin(ph2 * 1.1));
        offset += rip / max(u.resolution, vec2(1.0)) * 0.6;
    }
    // Water body lens: inward shift falling linearly rim -> dome center
    // (monotonic, magnifies R/(R-bend)). CPU: sampleGlassPlate water block.
    vec2 bodyOff = vec2(0.0);
    if (isWater) {
        const float domeR = max(min(he0.x, he0.y), 1.0);
        const float tiltw = clamp(1.0 - delta / domeR, 0.0, 1.0);
        const float bend = min((1.0 - 1.0 / max(u.ior, 1.01)) * T * 1.6, domeR * 0.45);
        // Still surface swell bends what lies beneath into soft waves.
        bodyOff = (-n2 * tiltw + swellAt(p / domeR * u.swellScale, u.swellPhase).xy * u.swellAmp) * bend /
                  max(u.resolution, vec2(1.0));
        offset += bodyOff;
    }
    const vec2 uvIso = clamp(v_uv + bodyOff, vec2(0.001), vec2(0.999));
    const vec2 uvL = mix(u.destUv0, u.destUv1, uvIso);
    // Clear is sharp: no interior frost. Regular keeps the milk blur core.
    const float frost = (u.blurEdge > 0.5) ? 0.88 : 0.0;
    const vec3 pane = mix(textureLod(sampler2D(u_sharp, samp), uvL, 0.0).rgb, textureLod(sampler2D(u_blur, samp), uvIso, 0.0).rgb, frost);
    // Outside the wrap band the refracted sample has zero weight: skip
    // its texture reads (most of a pane's pixels).
    vec3 sampleRgb = pane;
    if (wrapX > 0.0) {
        const float gamma = u.dispersion;
        vec3 wrapRgb;
        const vec2 uvIsoW = clamp(v_uv + offset, vec2(0.001), vec2(0.999));
        const vec2 uvW = mix(u.destUv0, u.destUv1, uvIsoW);
        if (gamma > 0.0 && wrapX > 0.08) {
            const vec2 uvIsoR = clamp(v_uv + offset * (1.0 - gamma), vec2(0.001), vec2(0.999));
            const vec2 uvIsoB = clamp(v_uv + offset * (1.0 + gamma), vec2(0.001), vec2(0.999));
            const vec2 uvR = mix(u.destUv0, u.destUv1, uvIsoR);
            const vec2 uvB = mix(u.destUv0, u.destUv1, uvIsoB);
            wrapRgb = vec3(textureLod(sampler2D(u_sharp, samp), uvR, 0.0).r, textureLod(sampler2D(u_sharp, samp), uvW, 0.0).g,
                           textureLod(sampler2D(u_sharp, samp), uvB, 0.0).b);
        } else {
            wrapRgb = textureLod(sampler2D(u_sharp, samp), uvW, 0.0).rgb;
        }
        wrapRgb = mix(wrapRgb, textureLod(sampler2D(u_blur, samp), uvIsoW, 0.0).rgb, frost);
        const float wrapMix = pow(clamp(wrapX, 0.0, 1.0), (u.blurEdge > 0.5) ? 1.25 : 1.0);
        sampleRgb = mix(pane, wrapRgb, wrapMix);
    }

    vec3 color = sampleRgb;
    const float L = rec709(pane);
    if (isMetal) {
        // Dome: |n.xy| 1 at the rim falling to 0 at depth = half height.
        // Dome folded by the still swell (CPU: metalNormal): full in the body,
        // gone at the rim so the bevel stays clean.
        const float domeM = max(min(he0.x, he0.y), 1.0);
        const float tiltm = clamp(1.0 - delta / domeM, 0.0, 1.0) * 0.97;
        const vec3 swM = swellAt(p / domeM * u.swellScale, u.swellPhase);
        vec2 nm = n2 * tiltm + swM.xy * (u.swellAmp * (1.0 - tiltm));
        nm *= min(1.0, 0.97 / max(length(nm), 1e-6));
        const vec3 Nc = vec3(nm, sqrt(max(1.0 - dot(nm, nm), 0.0)));
        const float la = u.glareAngle;
        const vec3 R = reflect(vec3(0.0, 0.0, -1.0), Nc);
        // envLight: the surroundings' brightness along R (reach 2.2x dome,
        // 3x3 roughness blur) lights the studio; the backdrop never shows.
        float envL = 0.0;
        if (u.envLight > 0.0) {
            const float reach = 2.2 * domeM;
            const vec2 duv = (u.destUv1 - u.destUv0) / max(u.resolution, vec2(1.0));
            const vec2 uvM = mix(u.destUv0, u.destUv1, v_uv) + R.xy * reach * duv;
            const float rough = 1.0 + u.envSoft * 4.0;
            vec3 env = vec3(0.0);
            for (int j = -1; j <= 1; ++j) {
                for (int i = -1; i <= 1; ++i) {
                    const vec2 q = clamp(uvM + vec2(float(i), float(j)) * rough * duv, vec2(0.0), vec2(1.0));
                    env += textureLod(sampler2D(u_sharp, samp), q, 0.0).rgb;
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
        vec3 mc = vec3(chromeStudio(R.y - split), chromeStudio(R.y), chromeStudio(R.y + split)) * lit;
        const vec3 rimC = vec3(chromeRim(n2.y - 2.0 * split), chromeRim(n2.y), chromeRim(n2.y + 2.0 * split)) * lit;
        mc = mix(mc, rimC, smoothstep(0.65, 0.97, tiltm) * 0.85);
        const vec3 F0m = metalF0(u.tint);
        const vec3 Fm = F0m + (1.0 - F0m) * pow(1.0 - clamp(Nc.z, 0.0, 1.0), 5.0);
        const vec3 Ldm = normalize(vec3(cos(la), sin(la), 0.8));
        const vec3 Hvm = normalize(Ldm + vec3(0.0, 0.0, 1.0));
        const float specm = pow(clamp(dot(Nc, Hvm), 0.0, 1.0), 56.0) * u.glareIntensity;
        const float bevel = pow(clamp(1.0 - abs(delta - 1.2 * dprN0) / (1.6 * dprN0), 0.0, 1.0), 5.0) * 0.55;
        color = mix(mc * Fm + specm, vec3(1.0), bevel);
    } else if (!isWater) {
    const float milkPull = (u.blurEdge > 0.5) ? 0.2 : 0.7;
    // Plate wash: a set tint drives it, else the white milk.
    vec3 washRgb;
    float washA;
    if (u.tint.a > 0.0) {
        washRgb = u.tint.rgb;
        washA = u.tint.a;
    } else {
        washRgb = vec3(1.0);
        washA = (u.lumaOn > 0.5 ? 0.04 : 0.008) * (1.0 - wrapX * milkPull);
    }
    color = mix(color, washRgb, washA);
    if (u.lumaOn > 0.5) {
        const float high = clamp((L - 0.72) / 0.28, 0.0, 1.0);
        const float low = clamp((0.28 - L) / 0.28, 0.0, 1.0);
        color *= 1.0 - high * u.lumaShadow;
        color = mix(color, color + vec3(0.06, 0.07, 0.08), low * u.lumaLift);
    }
    // Clear dimmer: 35% darkening when plate luma exceeds 0.6 (spec S9).
    if (u.dimmer > 0.0 && L > 0.6) {
        color *= 1.0 - u.dimmer;
    }
    }

    const float dprN = max(u.dpr, 1.0);
    const float inside = max(-d, 0.0);
    const float hair = pow(clamp(1.0 - abs(inside - 1.2 * dprN) / (1.8 * dprN), 0.0, 1.0), 5.0);
    const float ebits = u.edge;
    const bool eL = mod(ebits, 2.0) > 0.5;
    const bool eR = mod(floor(ebits / 2.0), 2.0) > 0.5;
    const bool eT = mod(floor(ebits / 4.0), 2.0) > 0.5;
    const bool eB = mod(floor(ebits / 8.0), 2.0) > 0.5;
    float sideW = 0.0;
    if (eL) {
        sideW += pow(clamp(-n2.x, 0.0, 1.0), 2.0);
    }
    if (eR) {
        sideW += pow(clamp(n2.x, 0.0, 1.0), 2.0);
    }
    color *= 1.0 - sideW * hair * 0.42;
    float top = 0.0;
    if (eT) {
        top = clamp(-n2.y, 0.0, 1.0);
    }
    float bot = 0.0;
    if (eB) {
        bot = clamp(n2.y, 0.0, 1.0);
    }
    const float hi = hair * (top * 0.35 + bot * 0.22) * u.glareIntensity;
    color = mix(color, vec3(1.0), clamp(hi, 0.0, 1.0));
    // Edge highlight lives in the outer 5px (CPU parity), fading from 3px;
    // the interior skips the lighting math it would scale by zero.
    const float edgeBand = 1.0 - smoothstep(3.0 * dprN, 5.0 * dprN, inside);
    if (!isMetal && edgeBand > 0.0) {
        vec2 n3xy = n2 * 0.9;  // CPU parity: flat edges stay off the glint lobe
        if (isWater) {
            n3xy += rip * 0.18;
        }
        const vec3 N3 = normalize(vec3(n3xy, 1.0));
        const float cosT = clamp(N3.z, 0.0, 1.0);
        const float fresPow = 5.0 + u.fresnelHardness;
        const float F0 = u.fresnelIntensity;
        const float fres = F0 + (1.0 - F0) * pow(1.0 - cosT, fresPow);
        const float rimW = clamp(wrapX + hair * 0.5, 0.0, 1.0);
        float edgeSpec = 0.0;
        if (eL) {
            edgeSpec += clamp(-n2.x, 0.0, 1.0);
        }
        if (eR) {
            edgeSpec += clamp(n2.x, 0.0, 1.0);
        }
        if (eT) {
            edgeSpec += clamp(-n2.y, 0.0, 1.0);
        }
        if (eB) {
            edgeSpec += clamp(n2.y, 0.0, 1.0);
        }
        edgeSpec = clamp(edgeSpec, 0.0, 1.0);
        const float la = u.glareAngle;
        const vec3 Ld = normalize(vec3(cos(la), sin(la), 0.8));
        const vec3 Hv = normalize(Ld + vec3(0.0, 0.0, 1.0));
        const float shin = max(u.glareRange * 4.0, 8.0);
        const float ndh = clamp(dot(N3, Hv), 0.0, 1.0);
        float spec = pow(ndh, shin) * u.glareIntensity;
        spec = clamp(spec - u.glareHardness * 0.1, 0.0, 1.0);
        spec *= mix(1.0, rimW, clamp(u.glareConvergence, 0.0, 1.0));
        spec *= edgeSpec;
        const float fresWash = fres * (u.fresnelRange / 18.0) * rimW * edgeSpec;
        color += (fresWash * 0.30 + spec * (0.35 + 0.65 * rimW)) * edgeBand;
    }
    if (isWater) {
        // Depth absorption in the tint; swell crests focus light.
        // CPU: shadeGlass water block.
        const float domeW = max(min(he0.x, he0.y), 1.0);
        const float depthW = smoothstep(0.0, domeW, delta);
        color = mix(color, color * u.tint.rgb * 1.6, 0.2 * clamp(u.absorbK, 0.0, 1.0) * depthW);
        const vec3 sw = swellAt(p / domeW * u.swellScale, u.swellPhase);
        color *= 1.0 + 0.23 * u.swellAmp * sw.z * depthW;
        // Crest lobe only for rippling water; still water has no crests.
        const float crest = u.rippleAmp > 0.0 ? clamp((rip.x + rip.y) * 0.25 + 0.5, 0.0, 1.0) : 0.0;
        const float rimWw = clamp(wrapX + hair * 0.5, 0.0, 1.0);
        const float ca = pow(rimWw * (1.0 - rimWw) * 4.0, 2.0);
        color += vec3(0.85, 1.0, 0.92) * (ca * 0.14 * crest);
    }
    if (u.interactive > 0.5) {
        color = mix(color, vec3(1.0), top * hair * 0.18);
    }

    out_color = vec4(color * alphaShape, alphaShape);
}
