#include "Strip.h"

#include <algorithm>
#include <cmath>

namespace glim::paint {
namespace {

constexpr float kEps = 1e-4f;

float scaleOf(const Mat4& t, const Rect& r) {
    if (r.size.x <= kEps || r.size.y <= kEps) {
        const Vec4 o = t * Vec4{0, 0, 0, 1};
        const Vec4 x = t * Vec4{1, 0, 0, 1};
        const Vec4 y = t * Vec4{0, 1, 0, 1};
        const float sx = std::hypot(x.x - o.x, x.y - o.y);
        const float sy = std::hypot(y.x - o.x, y.y - o.y);
        return std::min(sx, sy);
    }
    const Rect xf = transformRect(t, r);
    const float sx = std::fabs(xf.size.x / r.size.x);
    const float sy = std::fabs(xf.size.y / r.size.y);
    return std::min(sx, sy);
}

Radius scaleRadius(Radius r, float s) {
    r.lt *= s;
    r.rt *= s;
    r.lb *= s;
    r.rb *= s;
    return r;
}

bool sharp(const Radius& r) {
    return r.lt <= kEps && r.rt <= kEps && r.lb <= kEps && r.rb <= kEps;
}

void addSpan(Strip& strip, float x0, float x1, float coverage) {
    if (x1 <= x0 + 1e-6f || coverage <= 1e-4f) {
        return;
    }
    if (!strip.spans.empty()) {
        StripSpan& last = strip.spans.back();
        if (std::fabs(last.coverage - coverage) <= 1e-3f && x0 <= last.x1 + 1e-3f) {
            last.x1 = std::max(last.x1, x1);
            return;
        }
    }
    strip.spans.push_back({x0, x1, coverage});
}

void packHeight1(std::vector<Strip>& strips) {
    if (strips.size() < 2) {
        return;
    }
    std::vector<Strip> packed;
    packed.reserve(strips.size());
    std::size_t i = 0;
    while (i < strips.size()) {
        Strip cur = std::move(strips[i]);
        ++i;
        while (i < strips.size() && std::fabs(cur.height - 1.f) <= kEps &&
               std::fabs(strips[i].height - 1.f) <= kEps &&
               std::fabs(strips[i].y - (cur.y + cur.height)) <= kEps &&
               cur.spans.size() == strips[i].spans.size() &&
               cur.height + 1.f <= static_cast<float>(kStripHeight) + kEps) {
            bool same = true;
            for (std::size_t s = 0; s < cur.spans.size(); ++s) {
                if (std::fabs(cur.spans[s].x0 - strips[i].spans[s].x0) > 1e-3f ||
                    std::fabs(cur.spans[s].x1 - strips[i].spans[s].x1) > 1e-3f ||
                    std::fabs(cur.spans[s].coverage - strips[i].spans[s].coverage) > 1e-3f) {
                    same = false;
                    break;
                }
            }
            if (!same) {
                break;
            }
            cur.height += 1.f;
            ++i;
        }
        packed.push_back(std::move(cur));
    }
    strips = std::move(packed);
}

template <typename Cov>
void flattenCoverage(int x0, int y0, int x1, int y1, std::vector<Strip>& out, Cov&& covAt) {
    for (int y = y0; y < y1; ++y) {
        Strip s;
        s.y = static_cast<float>(y);
        s.height = 1.f;
        int runX = x0;
        float runCov = -1.f;
        auto flush = [&](int x) {
            if (runCov > 1e-4f) {
                addSpan(s, static_cast<float>(runX), static_cast<float>(x), runCov);
            }
            runCov = -1.f;
        };
        for (int x = x0; x < x1; ++x) {
            float cov = covAt(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
            if (cov >= 1.f - 1e-3f) {
                cov = 1.f;
            } else if (cov <= 1e-4f) {
                cov = 0.f;
            }
            if (runCov < 0.f) {
                runX = x;
                runCov = cov;
            } else if (std::fabs(cov - runCov) <= 1e-3f) {
                continue;
            } else {
                flush(x);
                runX = x;
                runCov = cov;
            }
        }
        flush(x1);
        if (!s.spans.empty()) {
            out.push_back(std::move(s));
        }
    }
    packHeight1(out);
}

BlitQuad toBlitQuad(const Blit& b) {
    const Vec4 p = b.matter.color.premul();
    BlitQuad q;
    q.x = b.rect.origin.x;
    q.y = b.rect.origin.y;
    q.w = b.rect.size.x;
    q.h = b.rect.size.y;
    q.u0 = b.matter.uv.origin.x;
    q.v0 = b.matter.uv.origin.y;
    q.u1 = b.matter.uv.origin.x + b.matter.uv.size.x;
    q.v1 = b.matter.uv.origin.y + b.matter.uv.size.y;
    q.r = p.x;
    q.g = p.y;
    q.b = p.z;
    q.a = p.w;
    q.imageId = b.matter.imageId;
    q.sdf = 0;
    return q;
}

void appendGlyphs(std::vector<BlitQuad>& blits, const GlyphRun& run) {
    const Vec4 p = run.color.premul();
    for (const GlyphQuad& g : run.glyphs) {
        BlitQuad q;
        q.x = g.dest.origin.x;
        q.y = g.dest.origin.y;
        q.w = g.dest.size.x;
        q.h = g.dest.size.y;
        q.u0 = g.uv.origin.x;
        q.v0 = g.uv.origin.y;
        q.u1 = g.uv.origin.x + g.uv.size.x;
        q.v1 = g.uv.origin.y + g.uv.size.y;
        q.r = p.x;
        q.g = p.y;
        q.b = p.z;
        q.a = p.w;
        q.imageId = run.imageId;
        q.sdf = 1;
        blits.push_back(q);
    }
}

}  // namespace

Radius clampRadius(const Rect& rect, Radius r) {
    r.lt = std::max(0.f, r.lt);
    r.rt = std::max(0.f, r.rt);
    r.lb = std::max(0.f, r.lb);
    r.rb = std::max(0.f, r.rb);
    const float w = std::max(0.f, rect.size.x);
    const float h = std::max(0.f, rect.size.y);
    const float top = r.lt + r.rt;
    if (top > w && top > 0.f) {
        const float s = w / top;
        r.lt *= s;
        r.rt *= s;
    }
    const float bot = r.lb + r.rb;
    if (bot > w && bot > 0.f) {
        const float s = w / bot;
        r.lb *= s;
        r.rb *= s;
    }
    const float left = r.lt + r.lb;
    if (left > h && left > 0.f) {
        const float s = h / left;
        r.lt *= s;
        r.lb *= s;
    }
    const float right = r.rt + r.rb;
    if (right > h && right > 0.f) {
        const float s = h / right;
        r.rt *= s;
        r.rb *= s;
    }
    return r;
}

Rect intersectRect(Rect a, Rect b) {
    const float x0 = std::max(a.origin.x, b.origin.x);
    const float y0 = std::max(a.origin.y, b.origin.y);
    const float x1 = std::min(a.origin.x + a.size.x, b.origin.x + b.size.x);
    const float y1 = std::min(a.origin.y + a.size.y, b.origin.y + b.size.y);
    if (x1 <= x0 || y1 <= y0) {
        return {};
    }
    return {{x0, y0}, {x1 - x0, y1 - y0}};
}

Rect strokeBounds(const Stroke& s) {
    const float o = std::max(0.f, s.width) * 0.5f;
    return {{s.rect.origin.x - o, s.rect.origin.y - o},
            {s.rect.size.x + o * 2.f, s.rect.size.y + o * 2.f}};
}

ClipState clipOf(const GroupParams& p, const Mat4& extra) {
    if (!hasClip(p)) {
        return {};
    }
    ClipState c;
    c.active = true;
    c.rect = transformRect(extra, p.clip);
    c.radius = scaleRadius(p.clipRadius, scaleOf(extra, p.clip));
    return c;
}

ClipState intersectClip(const ClipState& parent, const ClipState& child) {
    if (!parent.active) {
        return child;
    }
    if (!child.active) {
        return parent;
    }
    ClipState out;
    out.active = true;
    out.rect = intersectRect(parent.rect, child.rect);
    if (out.rect.size.x <= 0.f || out.rect.size.y <= 0.f) {
        out.active = true;
        out.rect = {};
        return out;
    }
    if (sharp(parent.radius) || sharp(child.radius)) {
        out.radius = {};
    } else {
        out.radius = parent.radius;
        out.radius.lt = std::min(parent.radius.lt, child.radius.lt);
        out.radius.rt = std::min(parent.radius.rt, child.radius.rt);
        out.radius.lb = std::min(parent.radius.lb, child.radius.lb);
        out.radius.rb = std::min(parent.radius.rb, child.radius.rb);
    }
    return out;
}

float roundedCoverage(const Rect& rect, Radius radius, float px, float py) {
    if (rect.size.x <= 0.f || rect.size.y <= 0.f) {
        return 0.f;
    }
    radius = clampRadius(rect, radius);
    const float x = px - (rect.origin.x + rect.size.x * 0.5f);
    const float y = py - (rect.origin.y + rect.size.y * 0.5f);
    const float hw = rect.size.x * 0.5f;
    const float hh = rect.size.y * 0.5f;
    float r = 0.f;
    if (x < 0.f && y < 0.f) {
        r = radius.lt;
    } else if (x >= 0.f && y < 0.f) {
        r = radius.rt;
    } else if (x < 0.f && y >= 0.f) {
        r = radius.lb;
    } else {
        r = radius.rb;
    }
    const float qx = std::fabs(x) - hw + r;
    const float qy = std::fabs(y) - hh + r;
    const float ox = std::max(qx, 0.f);
    const float oy = std::max(qy, 0.f);
    const float outside = std::hypot(ox, oy) + std::min(std::max(qx, qy), 0.f) - r;
    const float aa = 0.5f - outside;
    if (aa <= 0.f) {
        return 0.f;
    }
    if (aa >= 1.f) {
        return 1.f;
    }
    return aa;
}

void flattenRounded(const Rect& rect, Radius radius, std::vector<Strip>& out) {
    if (rect.size.x <= 0.f || rect.size.y <= 0.f) {
        return;
    }
    radius = clampRadius(rect, radius);
    const auto emitRect = [&](Rect r) {
        if (r.size.x <= 0.f || r.size.y <= 0.f) {
            return;
        }
        Strip s;
        s.y = r.origin.y;
        s.height = r.size.y;
        addSpan(s, r.origin.x, r.origin.x + r.size.x, 1.f);
        out.push_back(std::move(s));
    };
    const auto inside = [](Rect r, float px, float py) {
        return px >= r.origin.x && px < r.origin.x + r.size.x && py >= r.origin.y &&
               py < r.origin.y + r.size.y;
    };
    if (radius.lt <= kEps && radius.rt <= kEps && radius.lb <= kEps && radius.rb <= kEps) {
        emitRect(rect);
        return;
    }
    const float iL = std::max(radius.lt, radius.lb);
    const float iR = std::max(radius.rt, radius.rb);
    const float iT = std::max(radius.lt, radius.rt);
    const float iB = std::max(radius.lb, radius.rb);
    const Rect body{{rect.origin.x + iL, rect.origin.y + iT},
                    {rect.size.x - iL - iR, rect.size.y - iT - iB}};
    const Rect top{{rect.origin.x + radius.lt, rect.origin.y},
                   {rect.size.x - radius.lt - radius.rt, iT}};
    const Rect bot{{rect.origin.x + radius.lb, rect.origin.y + rect.size.y - iB},
                   {rect.size.x - radius.lb - radius.rb, iB}};
    const Rect left{{rect.origin.x, rect.origin.y + radius.lt},
                    {iL, rect.size.y - radius.lt - radius.lb}};
    const Rect right{{rect.origin.x + rect.size.x - iR, rect.origin.y + radius.rt},
                     {iR, rect.size.y - radius.rt - radius.rb}};
    emitRect(body);
    emitRect(top);
    emitRect(bot);
    emitRect(left);
    emitRect(right);
    const auto corner = [&](float cx, float cy, float cw, float ch) {
        if (cw <= 0.f || ch <= 0.f) {
            return;
        }
        const int x0 = static_cast<int>(std::floor(cx)) - 1;
        const int y0 = static_cast<int>(std::floor(cy)) - 1;
        const int x1 = static_cast<int>(std::ceil(cx + cw)) + 1;
        const int y1 = static_cast<int>(std::ceil(cy + ch)) + 1;
        for (int y = y0; y < y1; ++y) {
            Strip s;
            s.y = static_cast<float>(y);
            s.height = 1.f;
            for (int x = x0; x < x1; ++x) {
                const float px = static_cast<float>(x) + 0.5f;
                const float py = static_cast<float>(y) + 0.5f;
                if (inside(body, px, py) || inside(top, px, py) || inside(bot, px, py) ||
                    inside(left, px, py) || inside(right, px, py)) {
                    continue;
                }
                addSpan(s, static_cast<float>(x), static_cast<float>(x + 1),
                        roundedCoverage(rect, radius, px, py));
            }
            if (!s.spans.empty()) {
                out.push_back(std::move(s));
            }
        }
    };
    corner(rect.origin.x, rect.origin.y, radius.lt, radius.lt);
    corner(rect.origin.x + rect.size.x - radius.rt, rect.origin.y, radius.rt, radius.rt);
    corner(rect.origin.x, rect.origin.y + rect.size.y - radius.lb, radius.lb, radius.lb);
    corner(rect.origin.x + rect.size.x - radius.rb, rect.origin.y + rect.size.y - radius.rb,
           radius.rb, radius.rb);
}

void flattenStroke(const Rect& rect, Radius radius, float width, std::vector<Strip>& out) {
    if (width <= kEps || rect.size.x < 0.f || rect.size.y < 0.f) {
        return;
    }
    const float half = width * 0.5f;
    const Rect outer{{rect.origin.x - half, rect.origin.y - half},
                     {rect.size.x + width, rect.size.y + width}};
    Radius orad = radius;
    orad.lt += half;
    orad.rt += half;
    orad.lb += half;
    orad.rb += half;
    const Rect inner{{rect.origin.x + half, rect.origin.y + half},
                     {rect.size.x - width, rect.size.y - width}};
    const bool hole = inner.size.x > kEps && inner.size.y > kEps;
    Radius irad{};
    if (hole) {
        irad.lt = std::max(0.f, radius.lt - half);
        irad.rt = std::max(0.f, radius.rt - half);
        irad.lb = std::max(0.f, radius.lb - half);
        irad.rb = std::max(0.f, radius.rb - half);
    }
    const int x0 = static_cast<int>(std::floor(outer.origin.x)) - 1;
    const int y0 = static_cast<int>(std::floor(outer.origin.y)) - 1;
    const int x1 = static_cast<int>(std::ceil(outer.origin.x + outer.size.x)) + 1;
    const int y1 = static_cast<int>(std::ceil(outer.origin.y + outer.size.y)) + 1;
    flattenCoverage(x0, y0, x1, y1, out, [&](float px, float py) {
        const float outerCov = roundedCoverage(outer, orad, px, py);
        if (!hole) {
            return outerCov;
        }
        return std::max(0.f, outerCov - roundedCoverage(inner, irad, px, py));
    });
}

void clipStrips(std::vector<Strip>& strips, const ClipState& clip) {
    if (!clip.active) {
        return;
    }
    if (clip.rect.size.x <= 0.f || clip.rect.size.y <= 0.f) {
        strips.clear();
        return;
    }
    std::vector<Strip> clipMask;
    flattenRounded(clip.rect, clip.radius, clipMask);
    std::vector<Strip> out;
    out.reserve(strips.size());
    for (const Strip& s : strips) {
        const float sy0 = s.y;
        const float sy1 = s.y + s.height;
        Strip acc;
        acc.y = s.y;
        acc.height = s.height;
        for (const Strip& m : clipMask) {
            const float my0 = m.y;
            const float my1 = m.y + m.height;
            if (my1 <= sy0 || my0 >= sy1) {
                continue;
            }
            for (const StripSpan& a : s.spans) {
                for (const StripSpan& b : m.spans) {
                    const float x0 = std::max(a.x0, b.x0);
                    const float x1 = std::min(a.x1, b.x1);
                    addSpan(acc, x0, x1, a.coverage * b.coverage);
                }
            }
        }
        if (!acc.spans.empty()) {
            out.push_back(std::move(acc));
        }
    }
    strips = std::move(out);
}

void stripsToQuads(const std::vector<Strip>& strips, const Vec4& premul, std::vector<Quad>& quads) {
    for (const Strip& s : strips) {
        for (const StripSpan& sp : s.spans) {
            if (sp.x1 <= sp.x0 || s.height <= 0.f || sp.coverage <= 1e-4f) {
                continue;
            }
            Quad q;
            q.x = sp.x0;
            q.y = s.y;
            q.w = sp.x1 - sp.x0;
            q.h = s.height;
            q.r = premul.x * sp.coverage;
            q.g = premul.y * sp.coverage;
            q.b = premul.z * sp.coverage;
            q.a = premul.w * sp.coverage;
            quads.push_back(q);
        }
    }
}

void appendShape(std::vector<Quad>& quads, std::vector<BlitQuad>& blits, const Shape& shape,
                 const ClipState& clip) {
    if (const auto* f = std::get_if<FillRect>(&shape)) {
        if (!clip.active) {
            const Vec4 p = f->matter.color.premul();
            Quad q;
            q.x = f->rect.origin.x;
            q.y = f->rect.origin.y;
            q.w = f->rect.size.x;
            q.h = f->rect.size.y;
            q.r = p.x;
            q.g = p.y;
            q.b = p.z;
            q.a = p.w;
            quads.push_back(q);
            return;
        }
        std::vector<Strip> strips;
        flattenRounded(f->rect, Radius{}, strips);
        clipStrips(strips, clip);
        stripsToQuads(strips, f->matter.color.premul(), quads);
    } else if (const auto* r = std::get_if<FillRounded>(&shape)) {
        std::vector<Strip> strips;
        flattenRounded(r->rect, r->radius, strips);
        clipStrips(strips, clip);
        stripsToQuads(strips, r->matter.color.premul(), quads);
    } else if (const auto* s = std::get_if<Stroke>(&shape)) {
        std::vector<Strip> strips;
        flattenStroke(s->rect, s->radius, s->width, strips);
        clipStrips(strips, clip);
        stripsToQuads(strips, s->matter.color.premul(), quads);
    } else if (const auto* b = std::get_if<Blit>(&shape)) {
        if (clip.active && (clip.rect.size.x <= 0.f || clip.rect.size.y <= 0.f)) {
            return;
        }
        blits.push_back(toBlitQuad(*b));
    } else if (const auto* run = std::get_if<GlyphRun>(&shape)) {
        if (clip.active && (clip.rect.size.x <= 0.f || clip.rect.size.y <= 0.f)) {
            return;
        }
        appendGlyphs(blits, *run);
    }
    // SlotHole: skip paint. Not dest-out.
}

}  // namespace glim::paint
