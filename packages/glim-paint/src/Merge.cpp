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

Group::Group(const Group& o) : params(o.params), shapes(o.shapes) {
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
    out->children.reserve(g.children.size());
    for (const auto& child : g.children) {
        if (child) {
            out->children.push_back(cloneGroup(*child));
        }
    }
    return out;
}

FillRect transformFill(const Mat4& t, const FillRect& src) {
    const Vec4 o = t * Vec4{src.rect.origin.x, src.rect.origin.y, 0, 1};
    const Vec4 c = t * Vec4{src.rect.origin.x + src.rect.size.x, src.rect.origin.y + src.rect.size.y, 0, 1};
    FillRect out = src;
    out.rect.origin = {o.x, o.y};
    out.rect.size = {c.x - o.x, c.y - o.y};
    return out;
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
    return false;
}

bool canMerge(const Group& g) {
    if (needsIsolate(g)) {
        return false;
    }
    return g.params.blend == Blend::SrcOver;
}

Rect contentBounds(const Group& g) {
    Rect b = g.params.bounds;
    for (const Shape& s : g.shapes) {
        if (const auto* f = std::get_if<FillRect>(&s)) {
            b = unionRect(b, f->rect);
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
    std::vector<std::unique_ptr<Group>> kept;
    kept.reserve(g.children.size());
    for (auto& childPtr : g.children) {
        if (!childPtr) {
            continue;
        }
        Group child = merge(std::move(*childPtr), stats);
        if (canMerge(child)) {
            if (stats) {
                ++stats->mergedGroupCount;
            }
            for (const Shape& s : child.shapes) {
                if (const auto* f = std::get_if<FillRect>(&s)) {
                    g.shapes.emplace_back(transformFill(child.params.transform, *f));
                }
            }
            for (auto& grand : child.children) {
                if (!grand) {
                    continue;
                }
                grand->params.transform = child.params.transform * grand->params.transform;
                kept.push_back(std::move(grand));
            }
        } else {
            kept.push_back(std::make_unique<Group>(std::move(child)));
        }
    }
    g.children = std::move(kept);
    if (g.params.bounds.size.x <= 0 || g.params.bounds.size.y <= 0) {
        g.params.bounds = contentBounds(g);
    }
    return g;
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
