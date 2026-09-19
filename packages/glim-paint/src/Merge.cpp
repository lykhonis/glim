#include <glim/paint/Scene.h>

#include <algorithm>
#include <cmath>

namespace glim::paint {
namespace {

Rect unionRect(Rect a, Rect b) {
    if (a.size.x <= 0 || a.size.y <= 0) {
        return b;
    }
    if (b.size.x <= 0 || b.size.y <= 0) {
        return a;
    }
    const float x0 = std::min(a.origin.x, b.origin.x);
    const float y0 = std::min(a.origin.y, b.origin.y);
    const float x1 = std::max(a.origin.x + a.size.x, b.origin.x + b.size.x);
    const float y1 = std::max(a.origin.y + a.size.y, b.origin.y + b.size.y);
    return {{x0, y0}, {x1 - x0, y1 - y0}};
}

}  // namespace

Group::Group(const Group& o) : params(o.params), shapes(o.shapes), order(o.order) {
    children.reserve(o.children.size());
    for (const auto& child : o.children) {
        if (child) {
            children.push_back(std::make_unique<Group>(*child));
        }
    }
}

Group& Group::operator=(const Group& o) {
    if (this != &o) {
        Group tmp(o);
        *this = std::move(tmp);
    }
    return *this;
}

std::unique_ptr<Group> cloneGroup(const Group& g) {
    auto out = std::make_unique<Group>();
    out->params = g.params;
    out->shapes = g.shapes;
    out->order = g.order;
    out->children.reserve(g.children.size());
    for (const auto& child : g.children) {
        if (child) {
            out->children.push_back(cloneGroup(*child));
        }
    }
    return out;
}

Rect transformRect(const Mat4& t, Rect r) {
    const Vec4 o = t * Vec4{r.origin.x, r.origin.y, 0, 1};
    const Vec4 c = t * Vec4{r.origin.x + r.size.x, r.origin.y + r.size.y, 0, 1};
    return {{o.x, o.y}, {c.x - o.x, c.y - o.y}};
}

namespace {

float transformScale(const Mat4& t, const Rect& r) {
    if (r.size.x <= 1e-6f || r.size.y <= 1e-6f) {
        return 1.f;
    }
    const Rect xf = transformRect(t, r);
    const float sx = std::fabs(xf.size.x / r.size.x);
    const float sy = std::fabs(xf.size.y / r.size.y);
    return std::min(sx, sy);
}

Radius transformRadius(const Mat4& t, const Rect& r, Radius rad) {
    const float s = transformScale(t, r);
    rad.lt *= s;
    rad.rt *= s;
    rad.lb *= s;
    rad.rb *= s;
    return rad;
}

}  // namespace

FillRect transformFill(const Mat4& t, const FillRect& src) {
    FillRect out = src;
    out.rect = transformRect(t, src.rect);
    return out;
}

FillRounded transformRounded(const Mat4& t, const FillRounded& src) {
    FillRounded out = src;
    out.radius = transformRadius(t, src.rect, src.radius);
    out.rect = transformRect(t, src.rect);
    return out;
}

Stroke transformStroke(const Mat4& t, const Stroke& src) {
    Stroke out = src;
    const float s = transformScale(t, src.rect);
    out.radius = transformRadius(t, src.rect, src.radius);
    out.width = src.width * s;
    out.rect = transformRect(t, src.rect);
    return out;
}

Blit transformBlit(const Mat4& t, const Blit& src) {
    Blit out = src;
    out.rect = transformRect(t, src.rect);
    return out;
}

GlyphRun transformGlyphs(const Mat4& t, const GlyphRun& src) {
    GlyphRun out = src;
    out.origin = transformRect(t, Rect{src.origin, {1.f, 1.f}}).origin;
    const float s = transformScale(t, Rect{src.origin, {std::max(1.f, src.sizePx), 1.f}});
    out.sizePx = src.sizePx * s;
    for (GlyphQuad& g : out.glyphs) {
        g.dest = transformRect(t, g.dest);
    }
    return out;
}

SlotHole transformSlot(const Mat4& t, const SlotHole& src) {
    SlotHole out = src;
    out.rect = transformRect(t, src.rect);
    return out;
}

Shape transformShape(const Mat4& t, const Shape& s) {
    if (const auto* f = std::get_if<FillRect>(&s)) {
        return transformFill(t, *f);
    }
    if (const auto* r = std::get_if<FillRounded>(&s)) {
        return transformRounded(t, *r);
    }
    if (const auto* st = std::get_if<Stroke>(&s)) {
        return transformStroke(t, *st);
    }
    if (const auto* b = std::get_if<Blit>(&s)) {
        return transformBlit(t, *b);
    }
    if (const auto* g = std::get_if<GlyphRun>(&s)) {
        return transformGlyphs(t, *g);
    }
    if (const auto* h = std::get_if<SlotHole>(&s)) {
        return transformSlot(t, *h);
    }
    return s;
}

void appendTransformed(std::vector<Shape>& dst, const Mat4& t, const Shape& s) {
    dst.push_back(transformShape(t, s));
}

float snapBackdropSigma(float sigma) {
    if (sigma <= 0.f) {
        return 0.f;
    }
    if (sigma < 6.f) {
        return 4.f;
    }
    if (sigma < 12.f) {
        return 8.f;
    }
    if (sigma < 20.f) {
        return 16.f;
    }
    return 24.f;
}

bool hasExplicitBounds(const Group& g) {
    return g.params.bounds.size.x > 0.f && g.params.bounds.size.y > 0.f;
}

Rect backdropSurface(const Group& g) {
    return hasExplicitBounds(g) ? g.params.bounds : contentBounds(g);
}

int collectBackdropPills(const Group& g, BackdropPill* out) {
    if (!out) {
        return 0;
    }
    int n = 0;
    for (const Shape& s : g.shapes) {
        if (const auto* r = std::get_if<FillRounded>(&s)) {
            if (n >= kMaxBackdropPills) {
                break;
            }
            out[n].rect = r->rect;
            out[n].radius = plateRadius(r->radius);
            ++n;
        }
    }
    if (n == 0) {
        Rect b = hasExplicitBounds(g) ? g.params.bounds : contentBounds(g);
        out[0].rect = b;
        out[0].radius = plateRadius(g.params.clipRadius);
        n = 1;
    }
    return n;
}

void dropBackdropPills(Group& g) {
    // Drop only the shapes that were collected as pills (the first
    // kMaxBackdropPills FillRounded shapes). They define the plate mask drawn
    // by the backdrop program; dropping every FillRounded would delete visual
    // content beyond the pill cap.
    int pillShapes = 0;
    for (const Shape& s : g.shapes) {
        if (pillShapes >= kMaxBackdropPills) {
            break;
        }
        if (std::get_if<FillRounded>(&s)) {
            ++pillShapes;
        }
    }
    if (pillShapes == 0) {
        return;
    }
    int dropped = 0;
    std::vector<Shape> keep;
    std::vector<std::uint32_t> map(g.shapes.size(), ~0u);
    keep.reserve(g.shapes.size());
    for (std::size_t i = 0; i < g.shapes.size(); ++i) {
        if (dropped < pillShapes && std::get_if<FillRounded>(&g.shapes[i])) {
            ++dropped;
            continue;
        }
        map[i] = static_cast<std::uint32_t>(keep.size());
        keep.push_back(std::move(g.shapes[i]));
    }
    if (keep.size() == g.shapes.size()) {
        return;
    }
    std::vector<GroupItem> order;
    order.reserve(g.order.size());
    for (const GroupItem& item : g.order) {
        if (item.kind == GroupItem::Shape) {
            if (item.index < map.size() && map[item.index] != ~0u) {
                order.push_back({GroupItem::Shape, map[item.index]});
            }
        } else {
            order.push_back(item);
        }
    }
    g.shapes = std::move(keep);
    g.order = std::move(order);
}

void clearBackdropParams(GroupParams& p) {
    p.backdropBlur = 0.f;
    p.backdropBend = 0.f;
    p.backdropMerge = 0.f;
    p.backdropPress = 0.f;
    p.backdropFlat = false;
}

int isolatePixelSize(float logical, float pixelRatio) {
    const float pr = pixelRatio > 0.f ? pixelRatio : 1.f;
    return std::max(1, static_cast<int>(std::ceil(logical * pr)));
}

bool hasBackdrop(const Group& g) {
    return snapBackdropSigma(g.params.backdropBlur) > 0.f || g.params.backdropBend > 0.f;
}

bool hasGlass(const Group& g) noexcept {
    return g.params.glass.has_value();
}

bool isGlassContainer(const Group& g) noexcept {
    return g.params.glassContainer;
}

bool hasGlassWork(const Group& g) noexcept {
    if (hasGlass(g)) {
        return true;
    }
    if (!g.params.glassContainer) {
        return false;
    }
    for (const auto& c : g.children) {
        if (c && hasGlass(*c)) {
            return true;
        }
    }
    return false;
}

bool needsIsolate(const Group& g) {
    if (g.params.isolate) {
        return true;
    }
    if (g.params.opacity < 1.0f - 1e-5f) {
        return true;
    }
    if (g.params.transform.is3D()) {
        return true;
    }
    if (hasBackdrop(g)) {
        return true;
    }
    if (hasGlassWork(g)) {
        return true;
    }
    return false;
}

bool hasSlotHole(const Group& g) {
    for (const Shape& s : g.shapes) {
        if (std::holds_alternative<SlotHole>(s)) {
            return true;
        }
    }
    return false;
}

bool canMerge(const Group& g) {
    if (needsIsolate(g) || hasClip(g.params) || hasSlotHole(g)) {
        return false;
    }
    if (g.params.semantic != 0) {
        return false;
    }
    return g.params.blend == Blend::SrcOver || g.params.blend == Blend::Plus;
}

bool blendCompatible(Blend parent, Blend child) {
    if (parent != child) {
        return false;
    }
    // Only SrcOver and Plus exist in v1 (K9). A Plus child merges only into a
    // Plus parent; mixed blends never share a pass.
    // NOTE: GPU pipelines are still hardcoded to SrcOver
    // (Renderer::ensurePipelines). Merging both-Plus is the correct grouping;
    // a per-blend pipeline switch in the Renderer is follow-up work.
    return parent == Blend::SrcOver || parent == Blend::Plus;
}

Rect contentBounds(const Group& g) {
    Rect b = g.params.bounds;
    for (const Shape& s : g.shapes) {
        if (const auto* f = std::get_if<FillRect>(&s)) {
            b = unionRect(b, f->rect);
        } else if (const auto* r = std::get_if<FillRounded>(&s)) {
            b = unionRect(b, r->rect);
        } else if (const auto* st = std::get_if<Stroke>(&s)) {
            const float o = std::max(0.f, st->width) * 0.5f;
            b = unionRect(b, {{st->rect.origin.x - o, st->rect.origin.y - o},
                              {st->rect.size.x + o * 2.f, st->rect.size.y + o * 2.f}});
        } else if (const auto* blit = std::get_if<Blit>(&s)) {
            b = unionRect(b, blit->rect);
        } else if (const auto* run = std::get_if<GlyphRun>(&s)) {
            for (const GlyphQuad& g : run->glyphs) {
                b = unionRect(b, g.dest);
            }
        } else if (const auto* hole = std::get_if<SlotHole>(&s)) {
            b = unionRect(b, hole->rect);
        }
    }
    for (const auto& child : g.children) {
        if (child) {
            b = unionRect(b, contentBounds(*child));
        }
    }
    return b;
}

Group merge(Group g, Stats* stats) {
    Group out;
    out.params = g.params;
    bool lock = false;
    auto takeChild = [&](Group child) {
        if (canMerge(child) && blendCompatible(out.params.blend, child.params.blend) && !lock) {
            if (stats) {
                ++stats->mergedGroupCount;
            }
            for (const Shape& s : child.shapes) {
                appendTransformed(out.shapes, child.params.transform, s);
                out.order.push_back({GroupItem::Shape, static_cast<std::uint32_t>(out.shapes.size() - 1)});
            }
            for (auto& grand : child.children) {
                if (!grand) {
                    continue;
                }
                grand->params.transform = child.params.transform * grand->params.transform;
                out.order.push_back({GroupItem::Child, static_cast<std::uint32_t>(out.children.size())});
                out.children.push_back(std::move(grand));
                lock = true;
            }
            return;
        }
        lock = true;
        out.order.push_back({GroupItem::Child, static_cast<std::uint32_t>(out.children.size())});
        out.children.push_back(std::make_unique<Group>(std::move(child)));
    };

    if (g.order.empty()) {
        for (std::size_t i = 0; i < g.shapes.size(); ++i) {
            g.order.push_back({GroupItem::Shape, static_cast<std::uint32_t>(i)});
        }
        for (std::size_t i = 0; i < g.children.size(); ++i) {
            g.order.push_back({GroupItem::Child, static_cast<std::uint32_t>(i)});
        }
    }
    for (const GroupItem& item : g.order) {
        if (item.kind == GroupItem::Shape) {
            if (item.index >= g.shapes.size()) {
                continue;
            }
            out.shapes.push_back(g.shapes[item.index]);
            out.order.push_back({GroupItem::Shape, static_cast<std::uint32_t>(out.shapes.size() - 1)});
        } else if (item.index < g.children.size() && g.children[item.index]) {
            takeChild(merge(std::move(*g.children[item.index]), stats));
        }
    }
    if (out.params.bounds.size.x <= 0 || out.params.bounds.size.y <= 0) {
        out.params.bounds = contentBounds(out);
    }
    return out;
}

namespace {

Rect intersectAabb(Rect a, Rect b) {
    const float x0 = std::max(a.origin.x, b.origin.x);
    const float y0 = std::max(a.origin.y, b.origin.y);
    const float x1 = std::min(a.origin.x + a.size.x, b.origin.x + b.size.x);
    const float y1 = std::min(a.origin.y + a.size.y, b.origin.y + b.size.y);
    if (x1 <= x0 || y1 <= y0) {
        return {};
    }
    return {{x0, y0}, {x1 - x0, y1 - y0}};
}

void collectSlotsIn(const Group& g, const Mat4& extra, Rect clip, bool clipActive, bool underIsolate,
                    std::vector<SlotPlacement>* out) {
    const Mat4 world = extra;
    const bool isolate = underIsolate || needsIsolate(g);
    Rect nextClip = clip;
    bool nextActive = clipActive;
    if (hasClip(g.params)) {
        const Rect xf = transformRect(world, g.params.clip);
        if (!nextActive) {
            nextClip = xf;
            nextActive = true;
        } else {
            nextClip = intersectAabb(nextClip, xf);
        }
    }
    if (!isolate) {
        for (const Shape& s : g.shapes) {
            const auto* hole = std::get_if<SlotHole>(&s);
            if (!hole || hole->id == 0) {
                continue;
            }
            Rect r = transformRect(world, hole->rect);
            if (nextActive) {
                r = intersectAabb(r, nextClip);
            }
            if (r.size.x <= 0.f || r.size.y <= 0.f) {
                continue;
            }
            out->push_back(SlotPlacement{hole->id, r});
        }
    }
    for (const auto& child : g.children) {
        if (child) {
            collectSlotsIn(*child, world * child->params.transform, nextClip, nextActive, isolate, out);
        }
    }
}

}  // namespace

void collectSlots(const Scene& scene, std::vector<SlotPlacement>* out) {
    if (!out) {
        return;
    }
    collectSlotsIn(scene.root, Mat4::identity(), {}, false, false, out);
}

int coarseTileCount(Vec2 logicalSize, float pixelRatio) {
    const float pr = pixelRatio > 0 ? pixelRatio : 1.0f;
    const int w = std::max(1, static_cast<int>(std::ceil(logicalSize.x * pr)));
    const int h = std::max(1, static_cast<int>(std::ceil(logicalSize.y * pr)));
    const int tx = (w + kTileSize - 1) / kTileSize;
    const int ty = (h + kTileSize - 1) / kTileSize;
    return tx * ty;
}

}  // namespace glim::paint
