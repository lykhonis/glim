#include <glim/paint/Scene.h>

#include <algorithm>
#include <cmath>

namespace glim::paint {
namespace {

constexpr float kGlassRefArea = 200.f * 80.f;

const Glass& glassMaterial(const Group& g) {
    static const Glass kDefault{};
    if (g.params.glass.has_value()) {
        return *g.params.glass;
    }
    for (const auto& c : g.children) {
        if (c && c->params.glass.has_value()) {
            return *c->params.glass;
        }
    }
    return kDefault;
}

float blurRadiusLogical(const Glass& g) {
    switch (g.variant) {
        case GlassVariant::Clear:
            return 6.f;
        case GlassVariant::Identity:
            return 0.f;
        case GlassVariant::Regular:
        default:
            return 20.f;
    }
}

float sizeScaleFor(const Rect& aabb) {
    const float area = std::max(0.f, aabb.size.x * aabb.size.y);
    const float t = std::clamp(std::sqrt(area / std::max(kGlassRefArea, 1e-6f)), 0.f, 1.f);
    return 1.f + 0.6f * t;
}

void emitPill(GlassPill* out, int* n, Rect rect, float radius) {
    if (!out || *n >= kMaxGlassPills) {
        return;
    }
    const float minSide = std::min(rect.size.x, rect.size.y);
    GlassPill p;
    p.rect = rect;
    p.radius = radius;
    p.superellipseN = 4.f;
    // Capsule SDF in shaders handles both horizontal and vertical orientations.
    p.kind = radius >= minSide * 0.5f - 1e-3f ? GlassPillKind::Capsule : GlassPillKind::Rounded;
    out[*n] = p;
    ++*n;
}

void collectFills(const Group& g, GlassPill* out, int* n) {
    for (const Shape& s : g.shapes) {
        if (const auto* r = std::get_if<FillRounded>(&s)) {
            emitPill(out, n, r->rect, plateRadius(r->radius));
        }
    }
}

}  // namespace

Rect glassSurface(const Group& g) {
    const bool explicitBounds = g.params.bounds.size.x > 0.f && g.params.bounds.size.y > 0.f;
    Rect aabb = explicitBounds ? g.params.bounds : contentBounds(g);
    const Glass& mat = glassMaterial(g);
    const float T = mat.thicknessPx * sizeScaleFor(aabb);
    const float blur = blurRadiusLogical(mat);
    const float slack = mat.dispersion > 0.f ? 16.f : 12.f;
    const float gutter = std::ceil(T + blur + slack);
    aabb.origin.x -= gutter;
    aabb.origin.y -= gutter;
    aabb.size.x += gutter * 2.f;
    aabb.size.y += gutter * 2.f;
    return aabb;
}

int collectGlassPills(const Group& g, GlassPill* out) {
    if (!out) {
        return 0;
    }
    int n = 0;
    if (hasGlass(g)) {
        collectFills(g, out, &n);
    }
    if (isGlassContainer(g)) {
        for (const auto& child : g.children) {
            if (!child || !hasGlass(*child)) {
                continue;
            }
            const int before = n;
            collectFills(*child, out, &n);
            if (n == before) {
                const bool childBounds =
                    child->params.bounds.size.x > 0.f && child->params.bounds.size.y > 0.f;
                Rect b = childBounds ? child->params.bounds : contentBounds(*child);
                emitPill(out, &n, b, plateRadius(child->params.clipRadius));
            }
        }
    }
    if (hasGlass(g) && n == 0) {
        const bool ownBounds = g.params.bounds.size.x > 0.f && g.params.bounds.size.y > 0.f;
        Rect b = ownBounds ? g.params.bounds : contentBounds(g);
        emitPill(out, &n, b, plateRadius(g.params.clipRadius));
    }
    return n;
}

void dropGlassPills(Group& g) {
    dropBackdropPills(g);
}

void clearGlassParams(GroupParams& p) {
    p.glass.reset();
    p.glassContainer = false;
}

void stripGlassForIsolate(Group& g) {
    // Nested glass is flattened by design: outer plate wins, inner refraction
    // is dropped silently to avoid spamming logs on every frame.
    const bool hadGlass = hasGlass(g);
    clearGlassParams(g.params);
    if (hadGlass) {
        dropGlassPills(g);
    }
    for (auto& child : g.children) {
        if (child) {
            stripGlassForIsolate(*child);
        }
    }
}

}  // namespace glim::paint
