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
  // Correct AABB: all four corners (rotation-safe). Previously only
  // origin+corner were mapped, producing negative sizes under rotation.
  const Vec4 p0 = t * Vec4{r.origin.x, r.origin.y, 0, 1};
  const Vec4 p1 = t * Vec4{r.origin.x + r.size.x, r.origin.y, 0, 1};
  const Vec4 p2 = t * Vec4{r.origin.x, r.origin.y + r.size.y, 0, 1};
  const Vec4 p3 = t * Vec4{r.origin.x + r.size.x, r.origin.y + r.size.y, 0, 1};
  const float x0 = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x));
  const float y0 = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y));
  const float x1 = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
  const float y1 = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y));
  return {{x0, y0}, {x1 - x0, y1 - y0}};
}

bool isAxisAligned(const Mat4& t) noexcept {
  // Pure scale+translate: off-diagonal 2D terms ~0. Rotation/skew
  // fails this — those groups must not bake (see canMerge).
  return std::fabs(t.m[1]) <= 1e-6f && std::fabs(t.m[4]) <= 1e-6f;
}

bool invert2DAffine(const Mat4& t, Mat4& out) noexcept {
  const float a = t.m[0], b = t.m[1], c = t.m[4], d = t.m[5];
  const float tx = t.m[12], ty = t.m[13];
  const float det = a * d - b * c;
  if (std::fabs(det) <= 1e-12f) {
    return false;
  }
  const float ia = d / det, ib = -b / det, ic = -c / det, id = a / det;
  out = Mat4::identity();
  out.m[0] = ia;
  out.m[1] = ib;
  out.m[4] = ic;
  out.m[5] = id;
  out.m[12] = -(ia * tx + ic * ty);
  out.m[13] = -(ib * tx + id * ty);
  return true;
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

Vec2 transformPoint(const Mat4& t, Vec2 p) {
  const Vec4 v = t * Vec4{p.x, p.y, 0, 1};
  return {v.x, v.y};
}

// Gradient geometry rides the Shape rect; radius scales by the largest axis scale.
Matter transformMatter(const Mat4& t, Matter m) {
  if (!m.isGradient()) {
    return m;
  }
  m.gradientP0 = transformPoint(t, m.gradientP0);
  m.gradientP1 = transformPoint(t, m.gradientP1);
  const Vec4 ox = t * Vec4{0, 0, 0, 1};
  const Vec4 xx = t * Vec4{1, 0, 0, 1};
  const Vec4 yx = t * Vec4{0, 1, 0, 1};
  const float sx = std::hypot(xx.x - ox.x, xx.y - ox.y);
  const float sy = std::hypot(yx.x - ox.x, yx.y - ox.y);
  const float s = std::max(sx, sy);
  m.gradientRadius *= s;
  return m;
}

}  // namespace

FillRect transformFill(const Mat4& t, const FillRect& src) {
  FillRect out = src;
  out.matter = transformMatter(t, src.matter);
  out.rect = transformRect(t, src.rect);
  return out;
}

FillRounded transformRounded(const Mat4& t, const FillRounded& src) {
  FillRounded out = src;
  out.matter = transformMatter(t, src.matter);
  out.radius = transformRadius(t, src.rect, src.radius);
  out.rect = transformRect(t, src.rect);
  return out;
}

Stroke transformStroke(const Mat4& t, const Stroke& src) {
  Stroke out = src;
  const float s = transformScale(t, src.rect);
  out.matter = transformMatter(t, src.matter);
  out.radius = transformRadius(t, src.rect, src.radius);
  out.width = src.width * s;
  out.rect = transformRect(t, src.rect);
  return out;
}

FillChamfer transformChamfer(const Mat4& t, const FillChamfer& src) {
  FillChamfer out = src;
  out.matter = transformMatter(t, src.matter);
  out.cut = src.cut * transformScale(t, src.rect);
  out.rect = transformRect(t, src.rect);
  return out;
}

StrokeChamfer transformStrokeChamfer(const Mat4& t, const StrokeChamfer& src) {
  StrokeChamfer out = src;
  const float s = transformScale(t, src.rect);
  out.matter = transformMatter(t, src.matter);
  out.cut = src.cut * s;
  out.width = src.width * s;
  out.rect = transformRect(t, src.rect);
  return out;
}

Arc transformArc(const Mat4& t, const Arc& src) {
  Arc out = src;
  // Normalize negative (counterclockwise) sweep to positive equivalent:
  // start+sweep..start === start+sweep..(start+sweep)+|sweep|.
  if (out.sweepDeg < 0.f) {
    out.startDeg += out.sweepDeg;
    out.sweepDeg = -out.sweepDeg;
  }
  out.matter = transformMatter(t, src.matter);
  out.center = transformPoint(t, src.center);
  const Vec4 ox = t * Vec4{0, 0, 0, 1};
  const Vec4 xx = t * Vec4{1, 0, 0, 1};
  const Vec4 yx = t * Vec4{0, 1, 0, 1};
  const float sx = std::hypot(xx.x - ox.x, xx.y - ox.y);
  const float sy = std::hypot(yx.x - ox.x, yx.y - ox.y);
  const float s = std::max(sx, sy);
  out.radius = src.radius * s;
  out.thickness = src.thickness * s;
  return out;
}

Polyline transformPolyline(const Mat4& t, const Polyline& src) {
  Polyline out = src;
  out.matter = transformMatter(t, src.matter);
  const Vec4 ox = t * Vec4{0, 0, 0, 1};
  const Vec4 xx = t * Vec4{1, 0, 0, 1};
  const Vec4 yx = t * Vec4{0, 1, 0, 1};
  const float sx = std::hypot(xx.x - ox.x, xx.y - ox.y);
  const float sy = std::hypot(yx.x - ox.x, yx.y - ox.y);
  const float s = std::max(sx, sy);
  out.width = src.width * s;
  out.dotPitch = src.dotPitch * s;
  out.points.clear();
  out.points.reserve(src.points.size());
  for (const Vec2& p : src.points) {
    out.points.push_back(transformPoint(t, p));
  }
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
  if (const auto* ch = std::get_if<FillChamfer>(&s)) {
    return transformChamfer(t, *ch);
  }
  if (const auto* st = std::get_if<Stroke>(&s)) {
    return transformStroke(t, *st);
  }
  if (const auto* sc = std::get_if<StrokeChamfer>(&s)) {
    return transformStrokeChamfer(t, *sc);
  }
  if (const auto* a = std::get_if<Arc>(&s)) {
    return transformArc(t, *a);
  }
  if (const auto* pl = std::get_if<Polyline>(&s)) {
    return transformPolyline(t, *pl);
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
  // Drop only the collected pill shapes; dropping every FillRounded would delete content past the
  // cap.
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
  p.backdropFlat = false;
}

int isolatePixelSize(float logical, float pixelRatio) {
  const float pr = pixelRatio > 0.f ? pixelRatio : 1.f;
  return std::max(1, static_cast<int>(std::ceil(logical * pr)));
}

bool useHalfIsolate(const Group& g) {
  return g.params.transform.is3D() && !hasBackdrop(g) && !hasGlassWork(g);
}

int isolatePixelSizeFor(const Group& g, float logical, float pixelRatio) {
  const float pr = pixelRatio > 0.f ? pixelRatio : 1.f;
  const float scale = useHalfIsolate(g) ? pr * 0.5f : pr;
  return std::max(1, static_cast<int>(std::ceil(logical * scale)));
}

bool hasBackdrop(const Group& g) { return snapBackdropSigma(g.params.backdropBlur) > 0.f; }

bool hasShadow(const Group& g) noexcept {
  return g.params.shadow.has_value() && snapShadowSigma(g.params.shadow->sigma) > 0.f;
}

bool hasContentBlur(const Group& g) noexcept {
  return snapContentSigma(g.params.contentBlur) > 0.f;
}

float snapShadowSigma(float sigma) { return snapBackdropSigma(sigma); }

float snapContentSigma(float sigma) { return snapBackdropSigma(sigma); }

Rect shadowContentSurface(const Group& g) {
  Rect b = contentBounds(g);
  float pad = 0.f;
  if (g.params.shadow.has_value()) {
    const ShadowBlur& s = *g.params.shadow;
    pad = std::max(
        pad, snapShadowSigma(s.sigma) + s.expandPx + std::fabs(s.offset.x) + std::fabs(s.offset.y)
    );
  }
  if (g.params.contentBlur > 0.f) {
    pad = std::max(pad, snapContentSigma(g.params.contentBlur));
  }
  if (pad <= 0.f) {
    return b;
  }
  if (b.size.x <= 0.f || b.size.y <= 0.f) {
    return b;
  }
  return {{b.origin.x - pad, b.origin.y - pad}, {b.size.x + pad * 2.f, b.size.y + pad * 2.f}};
}

void clearShadowParams(GroupParams& p) {
  p.shadow.reset();
  p.contentBlur = 0.f;
}

bool hasGlass(const Group& g) noexcept { return g.params.glass.has_value(); }

bool isGlassContainer(const Group& g) noexcept { return g.params.glassContainer; }

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
  if (hasShadow(g)) {
    return true;
  }
  if (hasContentBlur(g)) {
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
  // Rotated groups must stay grouped: baking via transformRect would
  // collapse the rotation to an AABB. Leaves handle rotation via the
  // world-aware append path (inverse-mapped coverage).
  if (!isAxisAligned(g.params.transform)) {
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
  // v1 has only SrcOver/Plus (K9); Plus merges only into Plus. Per-blend pipeline switch is
  // follow-up.
  return parent == Blend::SrcOver || parent == Blend::Plus;
}

Rect contentBounds(const Group& g) {
  Rect b = g.params.bounds;
  for (const Shape& s : g.shapes) {
    if (const auto* f = std::get_if<FillRect>(&s)) {
      b = unionRect(b, f->rect);
    } else if (const auto* r = std::get_if<FillRounded>(&s)) {
      b = unionRect(b, r->rect);
    } else if (const auto* ch = std::get_if<FillChamfer>(&s)) {
      b = unionRect(b, ch->rect);
    } else if (const auto* st = std::get_if<Stroke>(&s)) {
      const float o = std::max(0.f, st->width) * 0.5f;
      b = unionRect(
          b,
          {{st->rect.origin.x - o, st->rect.origin.y - o},
           {st->rect.size.x + o * 2.f, st->rect.size.y + o * 2.f}}
      );
    } else if (const auto* sc = std::get_if<StrokeChamfer>(&s)) {
      const float o = std::max(0.f, sc->width) * 0.5f;
      b = unionRect(
          b,
          {{sc->rect.origin.x - o, sc->rect.origin.y - o},
           {sc->rect.size.x + o * 2.f, sc->rect.size.y + o * 2.f}}
      );
    } else if (const auto* a = std::get_if<Arc>(&s)) {
      const float o = a->radius + a->thickness * 0.5f;
      b = unionRect(b, {{a->center.x - o, a->center.y - o}, {o * 2.f, o * 2.f}});
    } else if (const auto* pl = std::get_if<Polyline>(&s)) {
      const float o = std::max(0.f, pl->width) * 0.5f;
      for (const Vec2& p : pl->points) {
        b = unionRect(b, {{p.x - o, p.y - o}, {o * 2.f, o * 2.f}});
      }
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

namespace {

// Authored identity matrices compare bit-equal; transformed ones do not.
bool isIdentityMatrix(const Mat4& t) {
  const Mat4 id = Mat4::identity();
  for (int i = 0; i < 16; ++i) {
    if (t.m[i] != id.m[i]) {
      return false;
    }
  }
  return true;
}

bool sameGlassMaterial(const Glass& a, const Glass& b) {
  return a.variant == b.variant && a.tint.rgba == b.tint.rgba && a.interactive == b.interactive &&
         a.ior == b.ior && a.thicknessPx == b.thicknessPx && a.mergeKPx == b.mergeKPx &&
         a.refDistance == b.refDistance && a.dispersion == b.dispersion && a.flatten == b.flatten;
}

// Slot-tag markers: empty leaf glass groups with explicit bounds; nothing else is reinterpreted.
bool fusibleGlassMarker(const std::unique_ptr<Group>& c, const Glass& mat) {
  if (!c || !hasGlass(*c)) {
    return false;
  }
  const GroupParams& p = c->params;
  if (p.glassContainer) {
    return false;
  }
  if (!c->shapes.empty() || !c->children.empty()) {
    return false;
  }
  if (p.opacity < 1.0f - 1e-5f) {
    return false;
  }
  if (p.blend != Blend::SrcOver) {
    return false;
  }
  if (p.semantic != 0) {
    return false;
  }
  if (p.contentBlur > 0.0f || hasBackdrop(*c) || hasShadow(*c)) {
    return false;
  }
  if (!isIdentityMatrix(p.transform)) {
    return false;
  }
  return p.glass.has_value() && sameGlassMaterial(*p.glass, mat);
}

// Fuse adjacent marker runs into one container; in-between shapes keep their order slots.
void fuseGlassRuns(Group& out) {
  std::size_t i = 0;
  while (i < out.children.size()) {
    if (!out.children[i] || !out.children[i]->params.glass.has_value() ||
        !hasGlass(*out.children[i])) {
      ++i;
      continue;
    }
    const Glass& mat = *out.children[i]->params.glass;
    std::size_t j = i;
    while (j < out.children.size() && fusibleGlassMarker(out.children[j], mat)) {
      ++j;
    }
    if (j - i < 2) {
      i = j > i ? j : i + 1;
      continue;
    }
    Group box;
    box.params.glassContainer = true;
    Rect bounds{};
    for (std::size_t k = i; k < j; ++k) {
      const Group& m = *out.children[k];
      Rect mb = (m.params.bounds.size.x > 0 && m.params.bounds.size.y > 0) ? m.params.bounds
                                                                           : contentBounds(m);
      bounds = unionRect(bounds, mb);
      box.order.push_back({GroupItem::Child, static_cast<std::uint32_t>(box.children.size())});
      box.children.push_back(std::move(out.children[k]));
    }
    box.params.bounds = bounds;
    out.children[i] = std::make_unique<Group>(std::move(box));
    out.children.erase(out.children.begin() + i + 1, out.children.begin() + j);
    const std::size_t removed = j - i - 1;
    std::vector<GroupItem> order;
    order.reserve(out.order.size());
    for (const GroupItem& item : out.order) {
      if (item.kind == GroupItem::Child && item.index > i && item.index < j) {
        continue;
      }
      if (item.kind == GroupItem::Child && item.index >= j) {
        order.push_back({GroupItem::Child, static_cast<std::uint32_t>(item.index - removed)});
        continue;
      }
      order.push_back(item);
    }
    out.order = std::move(order);
    ++i;
  }
}

}  // namespace

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
  // Never fuse inside an existing glass scope; containers own their pill collection.
  if (!hasGlass(g) && !g.params.glassContainer) {
    fuseGlassRuns(out);
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

void collectSlotsIn(
    const Group& g,
    const Mat4& extra,
    Rect clip,
    bool clipActive,
    bool underIsolate,
    std::vector<SlotPlacement>* out
) {
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
  // Off-viewport seats are dropped so a native view never lingers over
  // unpainted area eating input (archived input notes §1, roadmap P0.8).
  const bool viewport = scene.logicalSize.x > 0.f && scene.logicalSize.y > 0.f;
  collectSlotsIn(
      scene.root, Mat4::identity(), Rect::fromSize(scene.logicalSize), viewport, false, out
  );
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
