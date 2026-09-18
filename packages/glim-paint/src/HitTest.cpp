#include <glim/paint/Scene.h>

#include <algorithm>

namespace glim::paint {
namespace {

bool contains(Rect r, Vec2 p) {
    return p.x >= r.origin.x && p.x < r.origin.x + r.size.x && p.y >= r.origin.y &&
           p.y < r.origin.y + r.size.y;
}

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

bool shapeBounds(const Shape& s, const Mat4& world, Rect* out, bool* slot) {
    if (const auto* f = std::get_if<FillRect>(&s)) {
        *out = transformRect(world, f->rect);
        *slot = false;
        return out->size.x > 0.f && out->size.y > 0.f;
    }
    if (const auto* r = std::get_if<FillRounded>(&s)) {
        *out = transformRect(world, r->rect);
        *slot = false;
        return out->size.x > 0.f && out->size.y > 0.f;
    }
    if (const auto* st = std::get_if<Stroke>(&s)) {
        const float o = std::max(0.f, st->width) * 0.5f;
        const Rect expanded = {{st->rect.origin.x - o, st->rect.origin.y - o},
                               {st->rect.size.x + o * 2.f, st->rect.size.y + o * 2.f}};
        *out = transformRect(world, expanded);
        *slot = false;
        return out->size.x > 0.f && out->size.y > 0.f;
    }
    if (const auto* b = std::get_if<Blit>(&s)) {
        *out = transformRect(world, b->rect);
        *slot = false;
        return out->size.x > 0.f && out->size.y > 0.f;
    }
    if (const auto* run = std::get_if<GlyphRun>(&s)) {
        Rect bounds{};
        for (const GlyphQuad& g : run->glyphs) {
            bounds = unionRect(bounds, g.dest);
        }
        if (bounds.size.x <= 0.f || bounds.size.y <= 0.f) {
            return false;
        }
        *out = transformRect(world, bounds);
        *slot = false;
        return out->size.x > 0.f && out->size.y > 0.f;
    }
    if (const auto* h = std::get_if<SlotHole>(&s)) {
        if (h->id == 0) {
            return false;
        }
        *out = transformRect(world, h->rect);
        *slot = true;
        return out->size.x > 0.f && out->size.y > 0.f;
    }
    return false;
}

bool hitGroup(const Group& g, const Mat4& world, Rect clip, bool clipActive,
              std::uint32_t inherited, Vec2 point, Hit* out) {
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
        if (nextClip.size.x <= 0.f || nextClip.size.y <= 0.f) {
            return false;
        }
    }
    if (nextActive && !contains(nextClip, point)) {
        return false;
    }
    const std::uint32_t effective = g.params.semantic != 0 ? g.params.semantic : inherited;

    auto testShape = [&](const Shape& s) -> bool {
        Rect bounds{};
        bool slot = false;
        if (!shapeBounds(s, world, &bounds, &slot)) {
            return false;
        }
        if (!contains(bounds, point)) {
            return false;
        }
        out->semantic = effective;
        out->bounds = bounds;
        out->slot = slot;
        out->opaque = true;
        return true;
    };

    if (!g.order.empty()) {
        for (auto it = g.order.rbegin(); it != g.order.rend(); ++it) {
            if (it->kind == GroupItem::Shape) {
                if (it->index < g.shapes.size() && testShape(g.shapes[it->index])) {
                    return true;
                }
            } else if (it->index < g.children.size() && g.children[it->index] &&
                       hitGroup(*g.children[it->index],
                                world * g.children[it->index]->params.transform, nextClip,
                                nextActive, effective, point, out)) {
                return true;
            }
        }
        return false;
    }
    for (auto it = g.shapes.rbegin(); it != g.shapes.rend(); ++it) {
        if (testShape(*it)) {
            return true;
        }
    }
    for (auto it = g.children.rbegin(); it != g.children.rend(); ++it) {
        if (*it && hitGroup(**it, world * (*it)->params.transform, nextClip, nextActive,
                           effective, point, out)) {
            return true;
        }
    }
    return false;
}

}  // namespace

bool hitTest(const Scene& scene, Vec2 logical, Hit* out) {
    if (!out) {
        return false;
    }
    return hitGroup(scene.root, Mat4::identity(), {}, false, 0, logical, out);
}

}  // namespace glim::paint
